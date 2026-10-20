/*
 * Pasqal QDMI device: Pulser sequences on Pasqal Cloud devices and emulators.
 */

#include "pasqal_device.hpp"

#include "pasqal_qdmi/device.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <new>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <string_view>
#include <thread>

namespace {

constexpr int CUSTOM_MIN = 999999995;
constexpr int CUSTOM_MAX = 999999999;
constexpr std::string_view BATCH_PREFIX = "pasqal-batch:";
/// Consecutive failed status checks tolerated by `job_wait`.
constexpr int MAX_WAIT_FAILURES = 5;
/// Upper bound of the exponential polling delay in `job_wait`.
constexpr std::chrono::seconds MAX_WAIT_DELAY{30};

bool is_valid(int value, int max) {
  return (value >= 0 && value < max) ||
         (value >= CUSTOM_MIN && value <= CUSTOM_MAX);
}

bool iequals(std::string_view a, std::string_view b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                    [](unsigned char x, unsigned char y) {
                      return std::tolower(x) == std::tolower(y);
                    });
}

template <typename T>
int copy_value(const T& value, size_t size, void* out, size_t* size_ret) {
  if (out != nullptr) {
    if (size < sizeof(T)) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    std::memcpy(out, &value, sizeof(T));
  }
  if (size_ret != nullptr) {
    *size_ret = sizeof(T);
  }
  return QDMI_SUCCESS;
}

template <typename T>
int copy_vector(const std::vector<T>& values, size_t size, void* out,
                size_t* size_ret) {
  const auto bytes = values.size() * sizeof(T);
  if (out != nullptr) {
    if (size < bytes) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    if (bytes != 0) {
      std::memcpy(out, values.data(), bytes);
    }
  }
  if (size_ret != nullptr) {
    *size_ret = bytes;
  }
  return QDMI_SUCCESS;
}

int copy_str(std::string_view value, size_t size, void* out, size_t* size_ret) {
  const auto needed = value.size() + 1;
  if (out != nullptr) {
    if (size < needed) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    std::memcpy(out, value.data(), value.size());
    static_cast<char*>(out)[value.size()] = '\0';
  }
  if (size_ret != nullptr) {
    *size_ret = needed;
  }
  return QDMI_SUCCESS;
}

/// Read a null-terminated string of at most `size` bytes.
std::optional<std::string> read_string(size_t size, const void* value) {
  const auto* text = static_cast<const char*>(value);
  if (size == 0 || std::memchr(text, '\0', size) == nullptr) {
    return std::nullopt;
  }
  return std::string(text);
}

/// Run `body`, mapping escaping exceptions to QDMI status codes.
template <typename F> int guarded(F&& body) noexcept {
  try {
    return body();
  } catch (...) {
    return pasqal::current_exception_status();
  }
}

std::optional<std::string> default_authfile() {
  if (const auto* path = std::getenv("PASQAL_AUTHFILE"); path != nullptr) {
    return path;
  }
  const auto* home = std::getenv("HOME");
  if (home == nullptr) {
    return std::nullopt;
  }
  const auto path = std::filesystem::path(home) / ".pasqal" / "config";
  return std::filesystem::exists(path) ? std::optional{path.string()}
                                       : std::nullopt;
}

std::string trim(std::string_view text, std::string_view chars = " \t\r\n") {
  const auto begin = text.find_first_not_of(chars);
  if (begin == std::string_view::npos) {
    return {};
  }
  return std::string(
      text.substr(begin, text.find_last_not_of(chars) - begin + 1));
}

/// Fill unset credentials from a `key=value` file such as `~/.pasqal/config`.
void apply_authfile(PASQAL_QDMI_Device_Session_impl_d& session) {
  if (!session.authfile) {
    return;
  }
  std::ifstream file(*session.authfile);
  std::map<std::string, std::string> values;
  for (std::string line; std::getline(file, line);) {
    line = trim(line);
    const auto eq = line.find('=');
    if (line.empty() || line[0] == '#' || line[0] == ';' ||
        eq == std::string::npos) {
      continue;
    }
    values[trim(line.substr(0, eq))] = trim(trim(line.substr(eq + 1)), "\"'");
  }
  const auto fill = [&values](std::optional<std::string>& field,
                              const char* key, const char* alias = nullptr) {
    if (field) {
      return;
    }
    if (const auto it = values.find(key); it != values.end()) {
      field = it->second;
    } else if (alias != nullptr) {
      if (const auto alt = values.find(alias); alt != values.end()) {
        field = alt->second;
      }
    }
  };
  fill(session.username, "username");
  fill(session.password, "password");
  fill(session.token, "token");
  fill(session.project_id, "project_id");
  fill(session.authurl, "auth_endpoint", "QRMI_PASQAL_CLOUD_AUTH_ENDPOINT");
  fill(session.client_id, "client_id");
  fill(session.client_secret, "client_secret");
}

bool has_credentials(const PASQAL_QDMI_Device_Session_impl_d& session) {
  return session.token || (session.username && session.password) ||
         (session.client_id && session.client_secret);
}

std::optional<QDMI_Job_Status> to_qdmi(pasqal::JobState state) {
  switch (state) {
  case pasqal::JobState::Pending:
    return QDMI_JOB_STATUS_QUEUED;
  case pasqal::JobState::Running:
    return QDMI_JOB_STATUS_RUNNING;
  case pasqal::JobState::Canceling:
    return std::nullopt;
  case pasqal::JobState::Done:
    return QDMI_JOB_STATUS_DONE;
  case pasqal::JobState::Canceled:
    return QDMI_JOB_STATUS_CANCELED;
  case pasqal::JobState::Error:
    return QDMI_JOB_STATUS_FAILED;
  }
  return std::nullopt;
}

/// Validate only the concrete submission envelope; Pulser owns pulse semantics.
int validate_program(const std::string& source, size_t capacity,
                     size_t& atoms) {
  const auto sequence = nlohmann::json::parse(source, nullptr, false);
  if (!sequence.is_object() ||
      sequence.value("version", nlohmann::json{}) != "1" ||
      !sequence.contains("device") ||
      !(sequence["device"].is_object() || sequence["device"].is_string()) ||
      !sequence.contains("channels") || !sequence["channels"].is_object() ||
      !sequence.contains("operations") || !sequence["operations"].is_array() ||
      !sequence.contains("variables") || !sequence["variables"].is_object() ||
      !sequence.contains("register") || !sequence["register"].is_array() ||
      sequence["register"].empty()) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!sequence["variables"].empty() || !sequence.contains("measurement") ||
      sequence["measurement"].is_null()) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (sequence["measurement"] != "ground-rydberg" &&
      sequence["measurement"] != "digital" && sequence["measurement"] != "XY") {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  atoms = sequence["register"].size();
  if (capacity != 0 && atoms > capacity) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  std::set<nlohmann::json> names;
  for (const auto& atom : sequence["register"]) {
    if (!atom.is_object() || !atom.contains("name") ||
        !(atom["name"].is_string() || atom["name"].is_number_integer()) ||
        !names.insert(atom["name"]).second) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    if (!atom.contains("x") || !atom.contains("y")) {
      return QDMI_ERROR_NOTSUPPORTED; // A mappable register must first be built
                                      // by Pulser.
    }
    for (const auto* axis : {"x", "y", "z"}) {
      if (atom.contains(axis) && (!atom[axis].is_number() ||
                                  !std::isfinite(atom[axis].get<double>()))) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
    }
  }
  return QDMI_SUCCESS;
}

bool is_terminal(QDMI_Job_Status status) {
  return status == QDMI_JOB_STATUS_DONE || status == QDMI_JOB_STATUS_CANCELED ||
         status == QDMI_JOB_STATUS_FAILED;
}

void update_status(PASQAL_QDMI_Device_Job job) {
  // A batch is terminal only when every program has stopped.
  for (const auto status : {QDMI_JOB_STATUS_RUNNING, QDMI_JOB_STATUS_QUEUED,
                            QDMI_JOB_STATUS_SUBMITTED, QDMI_JOB_STATUS_CREATED,
                            QDMI_JOB_STATUS_FAILED, QDMI_JOB_STATUS_CANCELED,
                            QDMI_JOB_STATUS_DONE}) {
    if (std::any_of(job->programs.begin(), job->programs.end(),
                    [status](const auto& program) {
                      return program.status == status;
                    })) {
      job->status = status;
      return;
    }
  }
}

void refresh_mapping(PASQAL_QDMI_Device_Job job, pasqal::Deadline deadline) {
  if (job->mapping_loaded || job->batch_id.empty()) {
    return;
  }
  const auto batch = job->session->cloud->batch(job->batch_id, deadline);
  if (batch.job_ids.empty() || (!job->programs.empty() &&
                                batch.job_ids.size() != job->programs.size())) {
    throw pasqal::CloudError(
        0, "Pasqal batch program count does not match submission");
  }
  auto programs = job->programs;
  if (programs.empty()) {
    programs.resize(batch.job_ids.size());
  }
  for (size_t i = 0; i < programs.size(); ++i) {
    programs[i].id = batch.job_ids[i];
    programs[i].status = QDMI_JOB_STATUS_SUBMITTED;
    if (!programs[i].source && batch.sequence) {
      programs[i].source = batch.sequence;
    }
  }
  job->programs = std::move(programs);
  job->mapping_loaded = true;
}

/// Refresh metadata and retain terminal outcomes; transport errors never become
/// failures.
void refresh_program(PASQAL_QDMI_Device_Job job, pasqal::Program& program,
                     pasqal::Deadline deadline) {
  if (program.status == QDMI_JOB_STATUS_CREATED ||
      is_terminal(program.status) || !job->session->cloud) {
    return;
  }
  const auto metadata = job->session->cloud->job(program.id, deadline);
  if (!job->batch_id.empty() && metadata.batch_id != job->batch_id) {
    throw pasqal::CloudError(0, "Pasqal job belongs to a different batch");
  }
  if (metadata.sequence) {
    if (program.source && *program.source != *metadata.sequence &&
        program.atom_count) {
      // Serialized cloud normalization may differ; validate the atom order
      // below.
      const auto submitted = nlohmann::json::parse(*program.source);
      const auto returned = nlohmann::json::parse(*metadata.sequence);
      if (submitted.at("register") != returned.at("register")) {
        throw pasqal::CloudError(
            0, "Pasqal job register differs from submitted register");
      }
    }
    if (!program.atom_count) {
      program.source = metadata.sequence;
    }
  } else if (!program.source && job->batch_id.empty()) {
    program.source =
        job->session->cloud->batch(metadata.batch_id, deadline).sequence;
  }
  if (program.source && !program.atom_count) {
    size_t atoms = 0;
    if (validate_program(*program.source, 0, atoms) != QDMI_SUCCESS) {
      throw pasqal::CloudError(0,
                               "Pasqal returned an invalid concrete sequence");
    }
    program.atom_count = atoms;
  }
  if (metadata.runs) {
    if (program.runs && program.runs != metadata.runs) {
      throw pasqal::CloudError(0,
                               "Pasqal job run count differs from submission");
    }
    program.runs = metadata.runs;
  }
  const auto status = to_qdmi(metadata.state);
  if (status == QDMI_JOB_STATUS_DONE) {
    program.results = pasqal::make_histogram(
        job->session->cloud->job_counts(program.id, deadline),
        program.atom_count, program.runs);
  }
  if (status) {
    program.status = *status;
  }
}

int check_job(PASQAL_QDMI_Device_Job job,
              pasqal::Deadline deadline = std::nullopt) {
  return guarded([&]() -> int {
    refresh_mapping(job, deadline);
    int error = QDMI_SUCCESS;
    for (auto& program : job->programs) {
      const auto code = guarded([&]() -> int {
        refresh_program(job, program, deadline);
        return QDMI_SUCCESS;
      });
      if (error == QDMI_SUCCESS) {
        error = code;
      }
      if (code == QDMI_ERROR_TIMEOUT) {
        break;
      }
    }
    if (job->submission_attempted && !job->programs.empty()) {
      const auto common = job->programs.front().runs;
      job->shots = std::all_of(job->programs.begin(), job->programs.end(),
                               [common](const auto& program) {
                                 return program.runs == common;
                               })
                       ? common
                       : std::nullopt;
    }
    update_status(job);
    return error;
  });
}

int check_program(PASQAL_QDMI_Device_Job job, size_t index) {
  return guarded([&]() -> int {
    refresh_mapping(job, std::nullopt);
    if (index >= job->programs.size()) {
      return QDMI_ERROR_OUTOFRANGE;
    }
    refresh_program(job, job->programs[index], std::nullopt);
    update_status(job);
    return QDMI_SUCCESS;
  });
}

} // namespace

pasqal::Histogram
pasqal::make_histogram(const std::map<std::string, std::size_t>& counts,
                       std::optional<std::size_t> atom_count,
                       std::optional<std::size_t> runs) {
  if (counts.empty()) {
    throw CloudError(0, "Pasqal returned an empty counter");
  }
  const auto width = atom_count.value_or(counts.begin()->first.size());
  std::map<std::string, std::size_t> reversed;
  std::size_t total = 0;
  for (const auto& [key, count] : counts) {
    if (key.size() != width || width == 0 ||
        key.find_first_not_of("01") != std::string::npos ||
        count > std::numeric_limits<size_t>::max() - total) {
      throw CloudError(0,
                       "Invalid Pasqal histogram width, bits, or count total");
    }
    total += count;
    reversed.emplace(std::string(key.rbegin(), key.rend()), count);
  }
  if (runs && total != *runs) {
    throw CloudError(0, "Pasqal histogram total does not match run count");
  }
  Histogram histogram;
  for (const auto& [key, count] : reversed) {
    if (!histogram.keys.empty()) {
      histogram.keys += ',';
    }
    histogram.keys += key;
    histogram.values.push_back(count);
  }
  return histogram;
}

int pasqal::current_exception_status() {
  try {
    throw;
  } catch (const AuthError&) {
    return QDMI_ERROR_PERMISSIONDENIED;
  } catch (const UnsupportedError&) {
    return QDMI_ERROR_NOTSUPPORTED;
  } catch (const TimeoutError&) {
    return QDMI_ERROR_TIMEOUT;
  } catch (const CloudError& error) {
    if (error.status == 404) {
      return QDMI_ERROR_NOTFOUND;
    }
    return error.status == 401 || error.status == 403
               ? QDMI_ERROR_PERMISSIONDENIED
               : QDMI_ERROR_FATAL;
  } catch (const std::bad_alloc&) {
    return QDMI_ERROR_OUTOFMEM;
  } catch (...) {
    return QDMI_ERROR_FATAL;
  }
}

PASQAL_QDMI_Device_Session_impl_d::PASQAL_QDMI_Device_Session_impl_d()
    : authfile(default_authfile()) {
  if (const auto* backend_env = std::getenv("PASQAL_QDMI_BACKEND");
      backend_env != nullptr) {
    backend = backend_env;
  }
}

std::string PASQAL_QDMI_Device_Session_impl_d::backend_name() const {
  return backend.value_or(std::string(pasqal::DEFAULT_BACKEND));
}

bool PASQAL_QDMI_Device_Session_impl_d::is_mock() const {
  return iequals(backend_name(), pasqal::MOCK_BACKEND);
}

int PASQAL_QDMI_device_initialize() { return QDMI_SUCCESS; }

int PASQAL_QDMI_device_finalize() { return QDMI_SUCCESS; }

int PASQAL_QDMI_device_session_alloc(PASQAL_QDMI_Device_Session* session) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return guarded([session]() -> int {
    *session = new PASQAL_QDMI_Device_Session_impl_d();
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_session_set_parameter(
    PASQAL_QDMI_Device_Session session, QDMI_Device_Session_Parameter param,
    size_t size, const void* value) {
  if (session == nullptr ||
      !is_valid(param, QDMI_DEVICE_SESSION_PARAMETER_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  std::optional<std::string>* field = nullptr;
  switch (param) {
  case QDMI_DEVICE_SESSION_PARAMETER_BASEURL:
    field = &session->base_url;
    break;
  case QDMI_DEVICE_SESSION_PARAMETER_TOKEN:
    field = &session->token;
    break;
  case QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE:
    field = &session->authfile;
    break;
  case QDMI_DEVICE_SESSION_PARAMETER_AUTHURL:
    field = &session->authurl;
    break;
  case QDMI_DEVICE_SESSION_PARAMETER_USERNAME:
    field = &session->username;
    break;
  case QDMI_DEVICE_SESSION_PARAMETER_PASSWORD:
    field = &session->password;
    break;
  case QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1:
    field = &session->backend;
    break;
  case QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3:
    field = &session->project_id;
    break;
  default:
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  if (value == nullptr) {
    return QDMI_SUCCESS; // probe: the parameter is supported
  }
  return guarded([&]() -> int {
    auto text = read_string(size, value);
    if (!text) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    if (param == QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1 &&
        !iequals(*text, pasqal::MOCK_BACKEND) &&
        !pasqal::is_device_type(*text)) {
      return QDMI_ERROR_NOTSUPPORTED;
    }
    *field = std::move(text);
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_session_init(PASQAL_QDMI_Device_Session session) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  return guarded([session]() -> int {
    apply_authfile(*session);
    const auto backend = session->backend_name();
    if (session->is_mock()) {
      session->specs = pasqal::DeviceSpecs::mock();
    } else {
      if (!pasqal::is_device_type(backend)) {
        return QDMI_ERROR_FATAL; // e.g. a misspelled PASQAL_QDMI_BACKEND
      }
      if (!has_credentials(*session)) {
        return QDMI_ERROR_PERMISSIONDENIED;
      }
      pasqal::CloudConfig config{.device_type = backend,
                                 .project_id = session->project_id.value_or(""),
                                 .token = session->token,
                                 .username = session->username,
                                 .password = session->password,
                                 .client_id = session->client_id,
                                 .client_secret = session->client_secret};
      if (session->base_url) {
        config.base_url = *session->base_url;
      }
      if (session->authurl) {
        config.auth_endpoint = *session->authurl;
      }
      auto cloud = std::make_unique<pasqal::CloudClient>(std::move(config));
      session->specs =
          pasqal::DeviceSpecs::from_metadata(backend, cloud->device_metadata());
      session->specs.pulser_specs = cloud->device_spec_entries();
      session->cloud = std::move(cloud);
    }
    session->initialized = true;
    return QDMI_SUCCESS;
  });
}

void PASQAL_QDMI_device_session_free(PASQAL_QDMI_Device_Session session) {
  delete session;
}

int PASQAL_QDMI_device_session_query_device_property(
    PASQAL_QDMI_Device_Session session, QDMI_Device_Property prop, size_t size,
    void* value, size_t* size_ret) {
  if (session == nullptr || !is_valid(prop, QDMI_DEVICE_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  const auto& specs = session->specs;
  return guarded([&]() -> int {
    switch (prop) {
    case QDMI_DEVICE_PROPERTY_NAME:
      return copy_str(specs.name, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_VERSION:
      return copy_str(PASQAL_QDMI_DEVICE_VERSION, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_STATUS:
      return copy_value(specs.status, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_LIBRARYVERSION:
      return copy_str(QDMI_VERSION, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_QUBITSNUM:
      return copy_value(specs.qubits_num, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_SITES:
      return copy_vector(std::vector<PASQAL_QDMI_Site>{}, size, value,
                         size_ret);
    case QDMI_DEVICE_PROPERTY_OPERATIONS:
      return copy_vector(std::vector<PASQAL_QDMI_Operation>{}, size, value,
                         size_ret);
    case QDMI_DEVICE_PROPERTY_QUEUELENGTH:
      return session->cloud ? copy_value(session->cloud->queue_length(), size,
                                         value, size_ret)
                            : QDMI_ERROR_NOTSUPPORTED;
    case QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS:
      return copy_vector(std::vector{QDMI_PROGRAM_FORMAT_CUSTOM1}, size, value,
                         size_ret);
    case QDMI_DEVICE_PROPERTY_CUSTOM1:
      return specs.pulser_specs.empty()
                 ? QDMI_ERROR_NOTSUPPORTED
                 : copy_str(specs.pulser_specs, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_CUSTOM2:
      return specs.backend_metadata.empty()
                 ? QDMI_ERROR_NOTSUPPORTED
                 : copy_str(specs.backend_metadata, size, value, size_ret);
    default:
      return QDMI_ERROR_NOTSUPPORTED;
    }
  });
}

int PASQAL_QDMI_device_session_query_site_property(
    PASQAL_QDMI_Device_Session session, PASQAL_QDMI_Site site,
    QDMI_Site_Property prop, size_t size, void* value, size_t* size_ret) {
  if (session == nullptr || site == nullptr ||
      !is_valid(prop, QDMI_SITE_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  // This backend exposes no device-global site handles.
  (void)size;
  (void)value;
  (void)size_ret;
  return QDMI_ERROR_INVALIDARGUMENT;
}

int PASQAL_QDMI_device_session_query_operation_property(
    PASQAL_QDMI_Device_Session session, PASQAL_QDMI_Operation operation,
    size_t num_sites, const PASQAL_QDMI_Site* sites, size_t num_params,
    const double* params, QDMI_Operation_Property prop, size_t size,
    void* value, size_t* size_ret) {
  if (session == nullptr || operation == nullptr ||
      (num_sites > 0 && sites == nullptr) ||
      (num_params > 0 && params == nullptr) ||
      !is_valid(prop, QDMI_OPERATION_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  // This backend exposes no gate or operation handles.
  (void)size;
  (void)value;
  (void)size_ret;
  return QDMI_ERROR_INVALIDARGUMENT;
}

int PASQAL_QDMI_device_session_create_device_job(
    PASQAL_QDMI_Device_Session session, PASQAL_QDMI_Device_Job* job) {
  if (session == nullptr || job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  return guarded([&]() -> int {
    *job = new PASQAL_QDMI_Device_Job_impl_d();
    (*job)->session = session;
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_session_retrieve_device_job_by_id(
    PASQAL_QDMI_Device_Session session, const char* job_id,
    PASQAL_QDMI_Device_Job* job) {
  if (session == nullptr || job_id == nullptr || *job_id == '\0' ||
      job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  if (!session->cloud) {
    return QDMI_ERROR_NOTSUPPORTED; // the mock keeps no job registry
  }
  return guarded([&]() -> int {
    auto retrieved = std::make_unique<PASQAL_QDMI_Device_Job_impl_d>();
    retrieved->session = session;
    retrieved->id = job_id;
    retrieved->shots = std::nullopt;
    retrieved->submission_attempted = true;
    retrieved->status = QDMI_JOB_STATUS_SUBMITTED;
    const std::string_view id(job_id);
    if (id.starts_with(BATCH_PREFIX)) {
      retrieved->batch_id = id.substr(BATCH_PREFIX.size());
      if (retrieved->batch_id.empty()) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
    } else {
      pasqal::Program program;
      program.id = job_id;
      program.status = QDMI_JOB_STATUS_SUBMITTED;
      retrieved->programs.push_back(std::move(program));
      retrieved->mapping_loaded = true;
    }
    if (const auto code = check_job(retrieved.get()); code != QDMI_SUCCESS) {
      return code;
    }
    *job = retrieved.release();
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_job_set_parameter(PASQAL_QDMI_Device_Job job,
                                         QDMI_Device_Job_Parameter param,
                                         size_t size, const void* value) {
  if (job == nullptr || !is_valid(param, QDMI_DEVICE_JOB_PARAMETER_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (param != QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (job->submission_attempted || job->status != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }
  if (value == nullptr) {
    return QDMI_SUCCESS;
  }
  if (size != sizeof(size_t)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  // Pasqal Cloud takes the number of runs as a 32-bit signed integer.
  const auto shots = *static_cast<const size_t*>(value);
  if (shots == 0 ||
      shots > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  job->shots = shots;
  return QDMI_SUCCESS;
}

int PASQAL_QDMI_device_job_set_programs(PASQAL_QDMI_Device_Job job,
                                        QDMI_Program_Format format,
                                        size_t count, const size_t* sizes,
                                        const void* const* programs) {
  if (job == nullptr || count == 0 ||
      !is_valid(format, QDMI_PROGRAM_FORMAT_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (format != QDMI_PROGRAM_FORMAT_CUSTOM1) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (job->submission_attempted || job->status != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }
  if (programs == nullptr) {
    return QDMI_SUCCESS;
  }
  if (sizes == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return guarded([&]() -> int {
    std::vector<pasqal::Program> copied;
    copied.reserve(count);
    for (size_t index = 0; index < count; ++index) {
      if (programs[index] == nullptr) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      auto source = read_string(sizes[index], programs[index]);
      if (!source || source->size() + 1 != sizes[index]) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      size_t atoms = 0;
      if (const auto code =
              validate_program(*source, job->session->specs.qubits_num, atoms);
          code != QDMI_SUCCESS) {
        return code;
      }
      pasqal::Program program;
      program.source = std::move(source);
      program.atom_count = atoms;
      copied.push_back(std::move(program));
    }
    job->programs = std::move(copied);
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_job_get_program(PASQAL_QDMI_Device_Job job,
                                       size_t program_index, size_t size,
                                       void* data, size_t* size_ret) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (job->programs.empty()) {
    return QDMI_ERROR_BADSTATE;
  }
  if (program_index >= job->programs.size()) {
    return QDMI_ERROR_OUTOFRANGE;
  }
  const auto& source = job->programs[program_index].source;
  return source ? copy_str(*source, size, data, size_ret)
                : QDMI_ERROR_NOTSUPPORTED;
}

int PASQAL_QDMI_device_job_get_program_status(PASQAL_QDMI_Device_Job job,
                                              size_t program_index,
                                              QDMI_Job_Status* status) {
  if (job == nullptr || status == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (job->programs.empty()) {
    return QDMI_ERROR_BADSTATE;
  }
  if (program_index >= job->programs.size()) {
    return QDMI_ERROR_OUTOFRANGE;
  }
  const auto code = check_program(job, program_index);
  if (code == QDMI_SUCCESS) {
    *status = job->programs[program_index].status;
  }
  return code;
}

int PASQAL_QDMI_device_job_query_property(PASQAL_QDMI_Device_Job job,
                                          QDMI_Device_Job_Property prop,
                                          size_t size, void* value,
                                          size_t* size_ret) {
  if (job == nullptr || !is_valid(prop, QDMI_DEVICE_JOB_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return guarded([&]() -> int {
    switch (prop) {
    case QDMI_DEVICE_JOB_PROPERTY_ID:
      return job->id.empty() ? QDMI_ERROR_BADSTATE
                             : copy_str(job->id, size, value, size_ret);
    case QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT:
      if (job->programs.empty()) {
        return QDMI_ERROR_BADSTATE;
      }
      return std::any_of(job->programs.begin(), job->programs.end(),
                         [](const auto& program) { return !program.source; })
                 ? QDMI_ERROR_NOTSUPPORTED
                 : copy_value(QDMI_PROGRAM_FORMAT_CUSTOM1, size, value,
                              size_ret);
    case QDMI_DEVICE_JOB_PROPERTY_PROGRAMSNUM:
      return job->programs.empty()
                 ? QDMI_ERROR_BADSTATE
                 : copy_value(job->programs.size(), size, value, size_ret);
    case QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM:
      return job->shots ? copy_value(*job->shots, size, value, size_ret)
                        : QDMI_ERROR_NOTSUPPORTED;
    default:
      return QDMI_ERROR_NOTSUPPORTED;
    }
  });
}

int PASQAL_QDMI_device_job_submit(PASQAL_QDMI_Device_Job job) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (job->submission_attempted || job->status != QDMI_JOB_STATUS_CREATED ||
      job->programs.empty() || !job->shots) {
    return QDMI_ERROR_BADSTATE;
  }
  return guarded([job]() -> int {
    std::vector<std::string> sources;
    sources.reserve(job->programs.size());
    for (const auto& program : job->programs) {
      sources.push_back(*program.source);
    }
    // A lost POST response is ambiguous: never allow the same handle to submit
    // again.
    job->submission_attempted = true;
    const auto code = guarded([&]() -> int {
      if (job->session->cloud) {
        job->batch_id = job->session->cloud->submit_batch(sources, *job->shots);
        job->id = std::string(BATCH_PREFIX) + job->batch_id;
        for (auto& program : job->programs) {
          program.status = QDMI_JOB_STATUS_SUBMITTED;
          program.runs = job->shots;
        }
      } else {
        static std::atomic<size_t> next_mock_id{0};
        job->id = "mock-" + std::to_string(next_mock_id.fetch_add(1));
        for (auto& program : job->programs) {
          program.runs = job->shots;
          program.results = pasqal::make_histogram(
              {{std::string(*program.atom_count, '0'), *job->shots}},
              program.atom_count, program.runs);
          program.status = QDMI_JOB_STATUS_DONE;
        }
        job->mapping_loaded = true;
      }
      return QDMI_SUCCESS;
    });
    if (code != QDMI_SUCCESS) {
      for (auto& program : job->programs) {
        program.status = QDMI_JOB_STATUS_FAILED;
      }
    }
    update_status(job);
    return code;
  });
}

int PASQAL_QDMI_device_job_cancel(PASQAL_QDMI_Device_Job job) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return guarded([&]() -> int {
    if (job->submission_attempted) {
      if (const auto code = check_job(job); code != QDMI_SUCCESS) {
        return code;
      }
    }
    if (job->status == QDMI_JOB_STATUS_DONE ||
        job->status == QDMI_JOB_STATUS_FAILED) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    if (job->status == QDMI_JOB_STATUS_CANCELED) {
      return QDMI_SUCCESS;
    }
    if (job->status == QDMI_JOB_STATUS_CREATED || !job->session->cloud) {
      for (auto& program : job->programs) {
        program.status = QDMI_JOB_STATUS_CANCELED;
      }
      job->status = QDMI_JOB_STATUS_CANCELED;
      return QDMI_SUCCESS;
    }
    if (!job->batch_id.empty()) {
      job->session->cloud->cancel_batch(job->batch_id);
    } else {
      job->session->cloud->cancel_job(job->programs.front().id);
    }
    if (const auto code = check_job(job); code != QDMI_SUCCESS) {
      return code;
    }
    // Request acceptance is not a terminal cancellation; callers may poll and
    // retry cancel.
    if (job->status == QDMI_JOB_STATUS_DONE ||
        job->status == QDMI_JOB_STATUS_FAILED) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    return job->status == QDMI_JOB_STATUS_CANCELED ? QDMI_SUCCESS
                                                   : QDMI_ERROR_FATAL;
  });
}

int PASQAL_QDMI_device_job_check(PASQAL_QDMI_Device_Job job,
                                 QDMI_Job_Status* status) {
  if (job == nullptr || status == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  const auto code = check_job(job);
  if (code == QDMI_SUCCESS) {
    *status = job->status;
  }
  return code;
}

int PASQAL_QDMI_device_job_wait(PASQAL_QDMI_Device_Job job, size_t timeout) {
  if (job == nullptr || job->status == QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return guarded([&]() -> int {
    using clock = std::chrono::steady_clock;
    const auto started = clock::now();
    const auto max_timeout =
        static_cast<size_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                clock::time_point::max() - started)
                                .count());
    const auto deadline =
        timeout > 0 ? std::optional{started + std::chrono::seconds(std::min(
                                                  timeout, max_timeout))}
                    : std::nullopt;
    std::chrono::milliseconds delay{1000};
    int failures = 0;
    while (true) {
      if (is_terminal(job->status)) {
        return QDMI_SUCCESS;
      }
      if (deadline && clock::now() >= *deadline) {
        return QDMI_ERROR_TIMEOUT;
      }
      // Tolerate a few consecutive transient polling errors before giving up.
      if (const auto code = check_job(job, deadline); code == QDMI_SUCCESS) {
        failures = 0;
      } else if (code != QDMI_ERROR_FATAL || ++failures > MAX_WAIT_FAILURES) {
        return code;
      }
      if (is_terminal(job->status)) {
        return QDMI_SUCCESS;
      }
      const auto now = clock::now();
      if (deadline && now >= *deadline) {
        return QDMI_ERROR_TIMEOUT;
      }
      std::this_thread::sleep_for(
          deadline ? std::min<clock::duration>(delay, *deadline - now)
                   : clock::duration(delay));
      delay = std::min<std::chrono::milliseconds>(delay * 2, MAX_WAIT_DELAY);
    }
  });
}

int PASQAL_QDMI_device_job_get_results(PASQAL_QDMI_Device_Job job,
                                       size_t program_index,
                                       QDMI_Job_Result result, size_t size,
                                       void* data, size_t* size_ret) {
  if (job == nullptr || !is_valid(result, QDMI_JOB_RESULT_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (program_index >= job->programs.size()) {
    return QDMI_ERROR_OUTOFRANGE;
  }
  if (result != QDMI_JOB_RESULT_HIST_KEYS &&
      result != QDMI_JOB_RESULT_HIST_VALUES) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (const auto code = check_program(job, program_index);
      code != QDMI_SUCCESS) {
    return code;
  }
  const auto& program = job->programs[program_index];
  if (program.status != QDMI_JOB_STATUS_DONE || !program.results) {
    return QDMI_ERROR_BADSTATE;
  }
  switch (result) {
  case QDMI_JOB_RESULT_HIST_KEYS:
    return copy_str(program.results->keys, size, data, size_ret);
  case QDMI_JOB_RESULT_HIST_VALUES:
    return copy_vector(program.results->values, size, data, size_ret);
  default:
    return QDMI_ERROR_NOTSUPPORTED;
  }
}

void PASQAL_QDMI_device_job_free(PASQAL_QDMI_Device_Job job) { delete job; }
