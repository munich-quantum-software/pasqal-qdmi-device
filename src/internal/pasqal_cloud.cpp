/*
 * Pasqal Cloud REST client, following QRMI's Rust pasqal-cloud-api crate.
 */

#include "pasqal_cloud.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cpr/session.h>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>

namespace pasqal {
namespace {

using json = nlohmann::json;

constexpr std::array DEVICE_TYPES{"FRESNEL", "FRESNEL_CAN1", "EMU_MPS", "EMU_FREE",
                                  "EMU_FRESNEL"};
constexpr long long TOKEN_EXPIRY_GRACE_SECONDS = 10;
constexpr auto AUTH_GRANT_TYPE = "http://auth0.com/oauth/grant-type/password-realm";
constexpr auto AUTH_REALM = "pcs-users";
constexpr auto AUTH_CLIENT_ID = "PeZvo7Atx7IVv3iel59asJSb4Ig7vuSB";
constexpr auto AUTH_AUDIENCE = "https://apis.pasqal.cloud/account/api/v1";
constexpr std::chrono::seconds REQUEST_TIMEOUT{60};
constexpr std::chrono::seconds CONNECT_TIMEOUT{10};

std::string upper(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return out;
}

/// Resolve CURL_CA_BUNDLE, then SSL_CERT_FILE, then a readable system bundle,
/// since the bundled static libcurl does not know the distribution's CA path.
std::string ca_bundle() {
  for (const auto *variable : {"CURL_CA_BUNDLE", "SSL_CERT_FILE"}) {
    if (const auto *value = std::getenv(variable); value != nullptr && *value != '\0') {
      return value;
    }
  }
  for (const auto *bundle : {"/etc/ssl/certs/ca-certificates.crt",
                             "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/ca-bundle.pem",
                             "/etc/ssl/cert.pem"}) {
    if (const std::ifstream file{bundle}; file.good()) {
      return bundle;
    }
  }
  return {};
}

void configure(cpr::Session &session, const std::string &url,
               Deadline deadline = std::nullopt) {
  auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(REQUEST_TIMEOUT);
  if (deadline) {
    const auto remaining =
        std::chrono::ceil<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      throw TimeoutError("request deadline expired");
    }
    timeout = std::min(timeout, remaining);
  }
  session.SetUrl(cpr::Url{url});
  session.SetTimeout(cpr::Timeout{timeout});
  session.SetConnectTimeout(
      cpr::ConnectTimeout{std::min<std::chrono::milliseconds>(timeout, CONNECT_TIMEOUT)});
  if (const auto bundle = ca_bundle(); !bundle.empty()) {
    session.SetSslOptions(cpr::Ssl(cpr::ssl::CaInfo{bundle}));
  }
}

/// Return the body of a successful response, throw `CloudError` otherwise.
std::string checked(const cpr::Response &response) {
  if (response.error.code != cpr::ErrorCode::OK) {
    throw CloudError(0, response.error.message);
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    throw CloudError(response.status_code, "Status: " + std::to_string(response.status_code) +
                                               ", Fail " + response.text);
  }
  return response.text;
}

std::optional<std::string> base64url_decode(std::string_view text) {
  std::string out;
  unsigned int buffer = 0;
  int bits = 0;
  for (const char c : text) {
    int value = 0;
    if (c >= 'A' && c <= 'Z') {
      value = c - 'A';
    } else if (c >= 'a' && c <= 'z') {
      value = c - 'a' + 26;
    } else if (c >= '0' && c <= '9') {
      value = c - '0' + 52;
    } else if (c == '-') {
      value = 62;
    } else if (c == '_') {
      value = 63;
    } else {
      return std::nullopt;
    }
    buffer = (buffer << 6U) | static_cast<unsigned int>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<char>((buffer >> static_cast<unsigned int>(bits)) & 0xFFU));
    }
  }
  return out;
}

std::string normalize_auth_endpoint(const std::string &endpoint) {
  if (endpoint.empty()) {
    throw AuthError("auth endpoint must be configured when requesting an access token");
  }
  return endpoint.find("://") != std::string::npos ? endpoint : "https://" + endpoint;
}

long long now_unix_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

JobState parse_state(const std::string &state) {
  if (state == "PENDING") {
    return JobState::Pending;
  }
  if (state == "RUNNING") {
    return JobState::Running;
  }
  if (state == "CANCELING") {
    return JobState::Canceling;
  }
  if (state == "DONE") {
    return JobState::Done;
  }
  if (state == "CANCELED") {
    return JobState::Canceled;
  }
  if (state == "ERROR") {
    return JobState::Error;
  }
  throw CloudError(0, "unknown Pasqal Cloud job status: " + state);
}

} // namespace

bool is_device_type(std::string_view name) {
  return std::find(DEVICE_TYPES.begin(), DEVICE_TYPES.end(), upper(name)) !=
         DEVICE_TYPES.end();
}

bool is_token_usable(const std::string &token, long long now) {
  if (token.empty()) {
    return false;
  }
  const auto first = token.find('.');
  const auto second = token.find('.', first + 1);
  if (first == std::string::npos || second == std::string::npos || first == 0 ||
      second == first + 1) {
    return true; // not a JWT: usable until the API rejects it
  }
  const auto payload = base64url_decode(std::string_view(token).substr(first + 1, second - first - 1));
  if (!payload) {
    return false;
  }
  const auto claims = json::parse(*payload, nullptr, false);
  if (claims.is_discarded()) {
    return false;
  }
  if (!claims.is_object() || !claims.contains("exp")) {
    return true;
  }
  const auto &exp = claims["exp"];
  long long expiry = 0;
  if (exp.is_number()) {
    expiry = exp.get<long long>();
  } else if (exp.is_string()) {
    try {
      expiry = std::stoll(exp.get<std::string>());
    } catch (const std::exception &) {
      return false;
    }
  } else {
    return true;
  }
  return expiry > now + TOKEN_EXPIRY_GRACE_SECONDS;
}

CloudClient::CloudClient(CloudConfig config) : config_(std::move(config)) {
  config_.device_type = upper(config_.device_type);
  token_ = config_.token.value_or("");
}

std::string CloudClient::bearer(Deadline deadline) {
  std::unique_lock lock(token_mutex_, std::defer_lock);
  if (deadline) {
    if (!lock.try_lock_until(*deadline)) {
      throw TimeoutError("token refresh deadline expired");
    }
  } else {
    lock.lock();
  }
  if (is_token_usable(token_, now_unix_seconds())) {
    return token_;
  }
  cpr::Payload form{{"audience", AUTH_AUDIENCE}};
  if (config_.username && config_.password) {
    form.Add({"grant_type", AUTH_GRANT_TYPE});
    form.Add({"realm", AUTH_REALM});
    form.Add({"client_id", AUTH_CLIENT_ID});
    form.Add({"username", *config_.username});
    form.Add({"password", *config_.password});
  } else if (config_.client_id && config_.client_secret) {
    form.Add({"grant_type", "client_credentials"});
    form.Add({"client_id", *config_.client_id});
    form.Add({"client_secret", *config_.client_secret});
  } else {
    throw AuthError(
        "auth token is missing or expired and refresh credentials are not configured");
  }
  cpr::Session session;
  configure(session, normalize_auth_endpoint(config_.auth_endpoint), deadline);
  session.SetPayload(std::move(form));
  const auto response = session.Post();
  if (response.error.code == cpr::ErrorCode::OK && response.status_code >= 400 &&
      response.status_code < 500) {
    throw AuthError("Token request failed: " + std::to_string(response.status_code) + " " +
                    response.text);
  }
  token_ = json::parse(checked(response)).at("access_token").get<std::string>();
  return token_;
}

std::string CloudClient::get(const std::string &path, bool authenticated, Deadline deadline) {
  cpr::Session session;
  if (authenticated) {
    session.SetBearer(cpr::Bearer{bearer(deadline)});
  }
  configure(session, config_.base_url + path, deadline);
  session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
  return checked(session.Get());
}

std::string CloudClient::post(const std::string &path, const std::string &body) {
  cpr::Session session;
  configure(session, config_.base_url + path);
  session.SetBearer(cpr::Bearer{bearer()});
  session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
  session.SetBody(cpr::Body{body});
  return checked(session.Post());
}

std::string CloudClient::patch(const std::string &path) {
  cpr::Session session;
  configure(session, config_.base_url + path);
  session.SetBearer(cpr::Bearer{bearer()});
  session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
  return checked(session.Patch());
}

std::string CloudClient::device_spec_entries() {
  // Emulators report the public specs of the real devices, so that users build
  // sequences against current hardware constraints.
  json entries;
  if (config_.device_type.starts_with("EMU")) {
    entries = json::parse(get("/core-fast/api/v1/devices/public-specs", false)).at("data");
  } else {
    entries = json::array(
        {json::parse(get("/core-fast/api/v1/devices/specs/" + config_.device_type))
             .at("data")});
  }
  if (!entries.is_array() || entries.empty()) {
    throw CloudError(0, "Pasqal Cloud returned no device specs");
  }
  json kept = json::array();
  for (const auto &entry : entries) {
    kept.push_back({{"device_type", entry.at("device_type")}, {"specs", entry.at("specs")}});
  }
  return kept.dump();
}

std::string select_device_specs(const std::string &entries, const std::string &device_type) {
  const auto parsed = json::parse(entries);
  for (const auto &entry : parsed) {
    if (upper(entry.at("device_type").get<std::string>()) == upper(device_type)) {
      return entry.at("specs").get<std::string>();
    }
  }
  return parsed.front().at("specs").get<std::string>();
}

bool CloudClient::device_available() {
  const auto devices =
      json::parse(get("/core-fast/api/v1/devices?device_type=" + config_.device_type, false))
          .at("data");
  return !devices.empty() && devices.front().at("availability") == "ACTIVE";
}

std::string CloudClient::submit_job(const std::string &sequence, int runs) {
  const json batch{{"sequence_builder", sequence},
                   {"jobs", json::array({{{"runs", runs}}})},
                   {"open", false},
                   {"device_type", config_.device_type},
                   {"project_id", config_.project_id}};
  const auto data = json::parse(post("/core-fast/api/v1/batches", batch.dump())).at("data");
  if (const auto jobs = data.value("jobs", json::array()); !jobs.empty()) {
    return jobs.front().at("id").get<std::string>();
  }
  const auto batch_id = data.at("id").get<std::string>();
  const auto job_ids =
      json::parse(get("/core-fast/api/v2/batches/" + batch_id)).at("data").at("job_ids");
  if (job_ids.empty()) {
    throw CloudError(0, "batch has no jobs: " + batch_id);
  }
  return job_ids.front().get<std::string>();
}

void CloudClient::cancel_job(const std::string &job_id) {
  patch("/core-fast/api/v2/jobs/" + job_id + "/cancel");
}

JobState CloudClient::job_state(const std::string &job_id, Deadline deadline) {
  return parse_state(json::parse(get("/core-fast/api/v2/jobs/" + job_id, true, deadline))
                         .at("data")
                         .at("status")
                         .get<std::string>());
}

std::map<std::string, std::size_t> CloudClient::job_counts(const std::string &job_id,
                                                        Deadline deadline) {
  const auto link = json::parse(get("/core-fast/api/v1/jobs/" + job_id + "/results_link", true,
                                    deadline))
                        .at("data")
                        .at("results_link");
  if (!link.is_string()) {
    throw CloudError(0, "no results available for job " + job_id);
  }
  // The link is presigned: fetch it without the API bearer token.
  cpr::Session session;
  configure(session, link.get<std::string>(), deadline);
  const auto counts =
      json::parse(checked(session.Get())).at("counter").get<std::map<std::string, std::size_t>>();
  if (counts.empty()) {
    throw CloudError(0, "Pasqal result counter is empty");
  }
  return counts;
}

} // namespace pasqal
