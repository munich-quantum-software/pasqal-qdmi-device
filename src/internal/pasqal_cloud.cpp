// Copyright (c) 2026 Munich Quantum Software Company GmbH
// Copyright (c) 2026 Pasqal SAS
// All rights reserved.
//
// Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://llvm.org/LICENSE.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "pasqal-qdmi-device/Cloud.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cpr/session.h>
#include <cstdint>
#include <cstdlib>
#include <curl/urlapi.h>
#include <fstream>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>

namespace pasqal {
namespace {

using json = nlohmann::json;

constexpr long long TOKEN_EXPIRY_GRACE_SECONDS = 10;
constexpr auto AUTH_GRANT_TYPE =
    "http://auth0.com/oauth/grant-type/password-realm";
constexpr auto AUTH_REALM = "pcs-users";
constexpr auto AUTH_CLIENT_ID = "PeZvo7Atx7IVv3iel59asJSb4Ig7vuSB";
constexpr auto AUTH_AUDIENCE = "https://apis.pasqal.cloud/account/api/v1";
constexpr std::chrono::seconds REQUEST_TIMEOUT{60};
constexpr std::chrono::seconds CONNECT_TIMEOUT{10};

std::string upper(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return out;
}

void validate_url(const std::string& url) {
  const std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> parsed(
      curl_url(), curl_url_cleanup);
  if (!parsed) {
    throw std::bad_alloc();
  }
  if (url.find("://") == std::string::npos ||
      url.find_first_of("\r\n\t\\") != std::string::npos ||
      url.find('\0') != std::string::npos ||
      curl_url_set(parsed.get(), CURLUPART_URL, url.c_str(), 0) != CURLUE_OK) {
    throw std::invalid_argument("invalid cloud URL");
  }
  const auto part = [&parsed](CURLUPart field) {
    char* raw = nullptr;
    if (curl_url_get(parsed.get(), field, &raw, 0) != CURLUE_OK) {
      return std::string{};
    }
    const std::unique_ptr<char, decltype(&curl_free)> value(raw, curl_free);
    return std::string(value.get());
  };
  const auto scheme = upper(part(CURLUPART_SCHEME));
  const auto host = upper(part(CURLUPART_HOST));
  const bool loopback =
      host == "127.0.0.1" || host == "[::1]" || host == "LOCALHOST";
  const auto authority_start = url.find("://") + 3;
  const auto authority =
      url.substr(authority_start,
                 url.find_first_of("/?#", authority_start) - authority_start);
  if ((scheme != "HTTPS" && !(scheme == "HTTP" && loopback)) ||
      authority.find('@') != std::string::npos ||
      !part(CURLUPART_USER).empty() || !part(CURLUPART_PASSWORD).empty() ||
      url.find('#') != std::string::npos) {
    throw std::invalid_argument(
        "cloud URLs require HTTPS without userinfo or fragments");
  }
}

void validate_id(std::string_view id) {
  if (!is_cloud_id(id)) {
    throw std::invalid_argument("invalid cloud identifier");
  }
}

json parse_json(const std::string& body) {
  auto parsed = json::parse(body, nullptr, false);
  if (parsed.is_discarded()) {
    throw CloudError(0, "malformed cloud JSON response");
  }
  return parsed;
}

const json& field(const json& object, const char* name) {
  const auto it = object.find(name);
  if (it == object.end()) {
    throw CloudError(0, "missing cloud response field");
  }
  return *it;
}

std::string string_field(const json& object, const char* name) {
  const auto& value = field(object, name);
  if (!value.is_string()) {
    throw CloudError(0, "invalid cloud string field");
  }
  return value.get<std::string>();
}

std::optional<std::string> optional_string(const json& object,
                                           const char* name) {
  if (!object.contains(name) || object.at(name).is_null()) {
    return std::nullopt;
  }
  return string_field(object, name);
}

std::size_t nonnegative_integer(const json& value) {
  if (!value.is_number_integer() ||
      (!value.is_number_unsigned() && value.get<std::int64_t>() < 0)) {
    throw CloudError(0, "invalid nonnegative cloud integer");
  }
  const auto number = value.get<std::uint64_t>();
  if (number > std::numeric_limits<std::size_t>::max()) {
    throw CloudError(0, "cloud integer exceeds supported range");
  }
  return static_cast<std::size_t>(number);
}

/// Resolve CURL_CA_BUNDLE, then SSL_CERT_FILE, then a readable Linux bundle,
/// since the bundled static libcurl does not know the distribution's CA path.
std::string ca_bundle() {
  for (const auto* variable : {"CURL_CA_BUNDLE", "SSL_CERT_FILE"}) {
    if (const auto* value = std::getenv(variable);
        value != nullptr && *value != '\0') {
      return value;
    }
  }
#ifdef __linux__
  for (const auto* bundle : {"/etc/ssl/certs/ca-certificates.crt",
                             "/etc/pki/tls/certs/ca-bundle.crt",
                             "/etc/ssl/ca-bundle.pem", "/etc/ssl/cert.pem"}) {
    if (const std::ifstream file{bundle}; file.good()) {
      return bundle;
    }
  }
#endif
  return {};
}

void configure(cpr::Session& session, const std::string& url,
               const cpr::ConnectionPool& connection_pool,
               Deadline deadline = std::nullopt) {
  validate_url(url);
  auto timeout =
      std::chrono::duration_cast<std::chrono::milliseconds>(REQUEST_TIMEOUT);
  if (deadline) {
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
        *deadline - std::chrono::steady_clock::now());
    if (remaining.count() <= 0) {
      throw TimeoutError("request deadline expired");
    }
    timeout = std::min(timeout, remaining);
  }
  session.SetConnectionPool(connection_pool);
  session.SetUrl(cpr::Url{url});
  // Avoid CVE-2026-92392 with system curl as well as the bundled build.
  session.SetHttpVersion(cpr::HttpVersion{cpr::HttpVersionCode::VERSION_1_1});
  session.SetTimeout(cpr::Timeout{timeout});
  session.SetConnectTimeout(cpr::ConnectTimeout{
      std::min<std::chrono::milliseconds>(timeout, CONNECT_TIMEOUT)});
  if (const auto bundle = ca_bundle(); !bundle.empty()) {
    session.SetSslOptions(cpr::Ssl(cpr::ssl::CaInfo{bundle}));
  }
}

/// Return the body of a successful response, throw `CloudError` otherwise.
std::string checked(cpr::Response&& response) {
  if (response.error.code == cpr::ErrorCode::OPERATION_TIMEDOUT) {
    throw TimeoutError("cloud request timed out");
  }
  if (response.error.code != cpr::ErrorCode::OK) {
    throw CloudError(0, "cloud transport error");
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    throw CloudError(response.status_code,
                     "cloud HTTP status " +
                         std::to_string(response.status_code));
  }
  return std::move(response.text);
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
      out.push_back(static_cast<char>(
          (buffer >> static_cast<unsigned int>(bits)) & 0xFFU));
    }
  }
  return out;
}

json token_claims(std::string_view token) {
  const auto first = token.find('.');
  const auto second = token.find('.', first + 1);
  if (first == std::string::npos || second == std::string::npos || first == 0 ||
      second == first + 1) {
    return nullptr;
  }
  const auto payload =
      base64url_decode(token.substr(first + 1, second - first - 1));
  return payload ? json::parse(*payload, nullptr, false)
                 : json(json::value_t::discarded);
}

long long now_unix_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

JobState parse_state(const std::string& state) {
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
  throw CloudError(0, "unknown Pasqal Cloud job status");
}

} // namespace

bool is_cloud_id(std::string_view id) {
  return !id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
  });
}

bool is_token_usable(const std::string& token, long long now) {
  if (token.empty()) {
    return false;
  }
  const auto claims = token_claims(token);
  if (claims.is_null()) {
    return true; // not a JWT: usable until the API rejects it
  }
  if (!claims.is_object()) {
    return false;
  }
  if (!claims.contains("exp")) {
    return true;
  }
  const auto& expiry = claims["exp"];
  return expiry.is_number() && std::isfinite(expiry.get<double>()) &&
         expiry.get<double>() - TOKEN_EXPIRY_GRACE_SECONDS >
             static_cast<double>(now);
}

CloudClient::CloudClient(CloudConfig config) : config_(std::move(config)) {
  validate_url(config_.base_url);
  if (config_.base_url.find('?') != std::string::npos) {
    throw std::invalid_argument("cloud base URL cannot contain a query");
  }
  while (config_.base_url.ends_with('/')) {
    config_.base_url.pop_back();
  }
  validate_url(config_.auth_endpoint);
  config_.device_type = upper(config_.device_type);
  token_ = std::move(config_.token).value_or("");
  if (token_.find_first_of("\r\n") != std::string::npos ||
      token_.find('\0') != std::string::npos) {
    throw std::invalid_argument("invalid bearer token");
  }
}

std::string CloudClient::bearer(Deadline deadline, bool refresh) {
  if (refresh) {
    token_.clear();
    token_expiry_.reset();
  }
  const bool usable =
      token_expiry_
          ? !token_.empty() &&
                *token_expiry_ >
                    std::chrono::steady_clock::now() +
                        std::chrono::seconds(TOKEN_EXPIRY_GRACE_SECONDS)
          : is_token_usable(token_, now_unix_seconds());
  if (usable) {
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
    throw AuthError("auth token is missing or expired and refresh credentials "
                    "are not configured");
  }
  cpr::Session session;
  configure(session, config_.auth_endpoint, connection_pool_, deadline);
  session.SetPayload(std::move(form));
  auto response = session.Post();
  if (response.error.code == cpr::ErrorCode::OK &&
      response.status_code >= 400 && response.status_code < 500) {
    throw AuthError("token request rejected with HTTP status " +
                    std::to_string(response.status_code));
  }
  const auto data = parse_json(checked(std::move(response)));
  auto token = string_field(data, "access_token");
  if (token.empty() || token.find_first_of("\r\n") != std::string::npos ||
      token.find('\0') != std::string::npos) {
    throw AuthError("invalid access token response");
  }
  const auto claims = token_claims(token);
  // Unknown expiry must not turn a freshly acquired token into a permanent one.
  token_expiry_ = claims.is_object() && claims.contains("exp")
                      ? std::nullopt
                      : std::optional{std::chrono::steady_clock::now()};
  if (data.contains("expires_in")) {
    const auto& duration = data.at("expires_in");
    if (!duration.is_number()) {
      throw AuthError("invalid token expiry response");
    }
    const auto seconds = duration.get<double>();
    const auto remaining = std::chrono::steady_clock::time_point::max() -
                           std::chrono::steady_clock::now();
    if (!std::isfinite(seconds) || seconds < 0 ||
        seconds >= std::chrono::duration<double>(remaining).count()) {
      throw AuthError("invalid token expiry response");
    }
    token_expiry_ =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(seconds));
  }
  token_ = std::move(token);
  return token_;
}

std::string CloudClient::get(const std::string& path, bool authenticated,
                             Deadline deadline) {
  cpr::Session session;
  if (authenticated) {
    session.SetBearer(cpr::Bearer{bearer(deadline)});
  }
  configure(session, config_.base_url + path, connection_pool_, deadline);
  session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
  auto response = session.Get();
  if (authenticated && response.error.code == cpr::ErrorCode::OK &&
      response.status_code == 401) {
    session.SetBearer(cpr::Bearer{bearer(deadline, true)});
    configure(session, config_.base_url + path, connection_pool_, deadline);
    response = session.Get();
  }
  return checked(std::move(response));
}

std::string CloudClient::post(const std::string& path,
                              const std::string& body) {
  cpr::Session session;
  configure(session, config_.base_url + path, connection_pool_);
  session.SetBearer(cpr::Bearer{bearer()});
  session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
  // The serialized body stays alive until this synchronous POST completes.
  session.SetBodyView(cpr::BodyView{body});
  return checked(session.Post());
}

std::string CloudClient::patch(const std::string& path) {
  cpr::Session session;
  configure(session, config_.base_url + path, connection_pool_);
  session.SetBearer(cpr::Bearer{bearer()});
  session.SetHeader(cpr::Header{{"Content-Type", "application/json"}});
  return checked(session.Patch());
}

std::string CloudClient::device_spec_entries(bool is_emulator) {
  validate_id(config_.device_type);
  const auto response =
      parse_json(get("/core-fast/api/v1/devices/public-specs", false));
  const auto& entries = field(response, "data");
  if (!entries.is_array()) {
    throw CloudError(0, "invalid cloud device specs");
  }
  json kept = json::array();
  for (const auto& entry : entries) {
    const auto device_type = string_field(entry, "device_type");
    if (is_emulator || upper(device_type) == config_.device_type) {
      kept.push_back({{"device_type", device_type},
                      {"specs", string_field(entry, "specs")}});
    }
  }
  return kept.dump();
}

std::string CloudClient::device_metadata() {
  validate_id(config_.device_type);
  // Inactive or account-specific devices may be absent from the public list.
  for (const bool authenticated : {false, true}) {
    const auto response =
        parse_json(get(std::string(authenticated ? "/core-fast/api/v2/"
                                                 : "/core-fast/api/v1/") +
                           "devices?device_type=" + config_.device_type,
                       authenticated));
    const auto& devices = field(response, "data");
    if (!devices.is_array()) {
      throw CloudError(0, "invalid cloud device list");
    }
    std::optional<std::string> selected;
    for (const auto& device : devices) {
      if (upper(string_field(device, "device_type")) == config_.device_type) {
        if (selected) {
          throw CloudError(0, "duplicate cloud backend metadata");
        }
        selected = device.dump();
      }
    }
    if (selected) {
      return *selected;
    }
  }
  throw UnsupportedError("selected cloud backend is unavailable");
}

std::string
CloudClient::submit_batch(const std::vector<std::string_view>& sequences,
                          std::size_t runs) {
  if (sequences.empty() || runs == 0) {
    throw std::invalid_argument("a batch requires programs and positive runs");
  }
  validate_id(config_.device_type);
  auto jobs = json::array();
  for (const auto& sequence : sequences) {
    jobs.push_back({{"sequence", sequence}, {"runs", runs}});
  }
  const json body{{"sequence_builder", nullptr},
                  {"jobs", std::move(jobs)},
                  {"open", false},
                  {"device_type", config_.device_type},
                  {"project_id", config_.project_id}};
  const auto response =
      parse_json(post("/core-fast/api/v1/batches", body.dump()));
  const auto& data = field(response, "data");
  const auto id = string_field(data, "id");
  validate_id(id);
  return id;
}

CloudBatch CloudClient::batch(const std::string& batch_id, Deadline deadline) {
  validate_id(batch_id);
  const auto response =
      parse_json(get("/core-fast/api/v2/batches/" + batch_id, true, deadline));
  const auto& data = field(response, "data");
  CloudBatch result;
  if (string_field(data, "id") != batch_id) {
    throw CloudError(0, "cloud batch identifier mismatch");
  }
  const auto& ids = field(data, "job_ids");
  if (!ids.is_array() || ids.empty()) {
    throw CloudError(0, "cloud batch has no job identifiers");
  }
  std::set<std::string> seen;
  for (const auto& value : ids) {
    if (!value.is_string()) {
      throw CloudError(0, "invalid cloud job identifier");
    }
    const auto id = value.get<std::string>();
    validate_id(id);
    if (!seen.insert(id).second) {
      throw CloudError(0, "duplicate cloud job identifier");
    }
    result.job_ids.push_back(id);
  }
  result.sequence = optional_string(data, "sequence_builder");
  return result;
}

CloudJob CloudClient::job(const std::string& job_id, Deadline deadline) {
  validate_id(job_id);
  const auto response =
      parse_json(get("/core-fast/api/v2/jobs/" + job_id, true, deadline));
  const auto& data = field(response, "data");
  CloudJob result;
  if (string_field(data, "id") != job_id) {
    throw CloudError(0, "cloud job identifier mismatch");
  }
  result.batch_id = string_field(data, "batch_id");
  validate_id(result.batch_id);
  result.state = parse_state(string_field(data, "status"));
  if (data.contains("runs") && !data.at("runs").is_null()) {
    result.runs = nonnegative_integer(data.at("runs"));
  }
  result.sequence = optional_string(data, "sequence");
  return result;
}

void CloudClient::cancel_batch(const std::string& batch_id) {
  validate_id(batch_id);
  patch("/core-fast/api/v2/batches/" + batch_id + "/cancel");
}

std::size_t CloudClient::queue_length() {
  validate_id(config_.device_type);
  json response;
  try {
    response = parse_json(get("/core-fast/api/v1/devices/" +
                              config_.device_type + "/queue-size"));
  } catch (const CloudError& error) {
    if (error.status == 404 || error.status == 405 || error.status == 501) {
      throw UnsupportedError("cloud queue information is unavailable");
    }
    throw;
  }
  const auto& data = field(response, "data");
  if (!data.is_object() || !data.contains("device_queues") ||
      data.at("device_queues").is_null()) {
    throw UnsupportedError("cloud queue information is unavailable");
  }
  const auto& queues = data.at("device_queues");
  if (!queues.is_array()) {
    throw CloudError(0, "invalid cloud queue information");
  }
  std::size_t count = 0;
  for (const auto& queue : queues) {
    if (!queue.is_object() || !queue.contains("number_of_jobs") ||
        queue.at("number_of_jobs").is_null()) {
      throw UnsupportedError("cloud queue length is unavailable");
    }
    const auto jobs = nonnegative_integer(queue.at("number_of_jobs"));
    if (jobs > std::numeric_limits<std::size_t>::max() - count) {
      throw CloudError(0, "cloud queue length exceeds supported range");
    }
    count += jobs;
  }
  return count;
}

void CloudClient::cancel_job(const std::string& job_id) {
  validate_id(job_id);
  patch("/core-fast/api/v2/jobs/" + job_id + "/cancel");
}

std::map<std::string, std::size_t>
CloudClient::job_counts(const std::string& job_id, Deadline deadline) {
  validate_id(job_id);
  const auto response = parse_json(get(
      "/core-fast/api/v1/jobs/" + job_id + "/results_link", true, deadline));
  const auto& data = field(response, "data");
  const auto& link = field(data, "results_link");
  if (!link.is_string()) {
    throw CloudError(0, "cloud job results are unavailable");
  }
  // The link is presigned: fetch it without the API bearer token.
  cpr::Session session;
  configure(session, link.get<std::string>(), connection_pool_, deadline);
  const auto results = parse_json(checked(session.Get()));
  const auto& counter = field(results, "counter");
  if (!counter.is_object() || counter.empty()) {
    throw CloudError(0, "Pasqal result counter is empty");
  }
  std::map<std::string, std::size_t> counts;
  for (const auto& [key, value] : counter.items()) {
    counts.emplace(key, nonnegative_integer(value));
  }
  return counts;
}

} // namespace pasqal
