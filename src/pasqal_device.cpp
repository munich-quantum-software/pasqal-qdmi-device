/*
 * Pasqal QDMI device: Pulser sequences on Pasqal Cloud devices and emulators.
 */

#include "pasqal_device.hpp"

#include "pasqal_qdmi/device.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <new>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <string_view>
#include <thread>

namespace {

constexpr int CUSTOM_MIN = 999999995;
constexpr int CUSTOM_MAX = 999999999;
constexpr std::string_view JOB_LIST_PREFIX = "pasqal-jobs:";
/// Consecutive failed status checks tolerated by `job_wait`.
constexpr int MAX_WAIT_FAILURES = 5;
/// Upper bound of the exponential polling delay in `job_wait`.
constexpr std::chrono::seconds MAX_WAIT_DELAY{30};

bool is_valid(int value, int max) {
  return (value >= 0 && value < max) || (value >= CUSTOM_MIN && value <= CUSTOM_MAX);
}

bool iequals(std::string_view a, std::string_view b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) {
    return std::tolower(x) == std::tolower(y);
  });
}

template <typename T> int copy_value(const T &value, size_t size, void *out, size_t *size_ret) {
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
int copy_vector(const std::vector<T> &values, size_t size, void *out, size_t *size_ret) {
  const auto bytes = values.size() * sizeof(T);
  if (out != nullptr) {
    if (size < bytes) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    std::memcpy(out, values.data(), bytes);
  }
  if (size_ret != nullptr) {
    *size_ret = bytes;
  }
  return QDMI_SUCCESS;
}

int copy_str(std::string_view value, size_t size, void *out, size_t *size_ret) {
  const auto needed = value.size() + 1;
  if (out != nullptr) {
    if (size < needed) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    std::memcpy(out, value.data(), value.size());
    static_cast<char *>(out)[value.size()] = '\0';
  }
  if (size_ret != nullptr) {
    *size_ret = needed;
  }
  return QDMI_SUCCESS;
}

/// Read a null-terminated string of at most `size` bytes.
std::optional<std::string> read_string(size_t size, const void *value) {
  const auto *text = static_cast<const char *>(value);
  if (size == 0 || std::memchr(text, '\0', size) == nullptr) {
    return std::nullopt;
  }
  return std::string(text);
}

/// Run `body`, mapping escaping exceptions to QDMI status codes.
template <typename F> int guarded(F &&body) noexcept {
  try {
    return body();
  } catch (...) {
    return pasqal::current_exception_status();
  }
}

std::optional<std::string> default_authfile() {
  if (const auto *path = std::getenv("PASQAL_AUTHFILE"); path != nullptr) {
    return path;
  }
  const auto *home = std::getenv("HOME");
  if (home == nullptr) {
    return std::nullopt;
  }
  const auto path = std::filesystem::path(home) / ".pasqal" / "config";
  return std::filesystem::exists(path) ? std::optional{path.string()} : std::nullopt;
}

std::string trim(std::string_view text, std::string_view chars = " \t\r\n") {
  const auto begin = text.find_first_not_of(chars);
  if (begin == std::string_view::npos) {
    return {};
  }
  return std::string(text.substr(begin, text.find_last_not_of(chars) - begin + 1));
}

/// Fill unset credentials from a `key=value` file such as `~/.pasqal/config`.
void apply_authfile(PASQAL_QDMI_Device_Session_impl_d &session) {
  if (!session.authfile) {
    return;
  }
  std::ifstream file(*session.authfile);
  std::map<std::string, std::string> values;
  for (std::string line; std::getline(file, line);) {
    line = trim(line);
    const auto eq = line.find('=');
    if (line.empty() || line[0] == '#' || line[0] == ';' || eq == std::string::npos) {
      continue;
    }
    values[trim(line.substr(0, eq))] = trim(trim(line.substr(eq + 1)), "\"'");
  }
  const auto fill = [&values](std::optional<std::string> &field, const char *key,
                              const char *alias = nullptr) {
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

bool has_credentials(const PASQAL_QDMI_Device_Session_impl_d &session) {
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
    return std::nullopt; // cancellation in progress: keep the last known status
  case pasqal::JobState::Done:
    return QDMI_JOB_STATUS_DONE;
  case pasqal::JobState::Canceled:
    return QDMI_JOB_STATUS_CANCELED;
  case pasqal::JobState::Error:
    return QDMI_JOB_STATUS_FAILED;
  }
  return std::nullopt;
}

pasqal::Histogram to_histogram(const std::map<std::string, std::size_t> &counts) {
  pasqal::Histogram histogram;
  for (const auto &[key, value] : counts) {
    histogram.keys += (histogram.keys.empty() ? "" : ",") + key;
    histogram.values.push_back(value);
  }
  return histogram;
}

std::string unique_job_id() {
  std::random_device device;
  std::ostringstream id;
  id << std::hex << device() << device() << device() << device();
  return id.str();
}

bool is_terminal(QDMI_Job_Status status) {
  return status == QDMI_JOB_STATUS_DONE || status == QDMI_JOB_STATUS_CANCELED ||
         status == QDMI_JOB_STATUS_FAILED;
}

void update_status(PASQAL_QDMI_Device_Job job) {
  const auto has_status = [job](QDMI_Job_Status status) {
    return std::any_of(job->programs.begin(), job->programs.end(),
                       [status](const auto &program) { return program.status == status; });
  };
  // An aggregate failure or cancellation is terminal only after every program stops.
  for (const auto status : {QDMI_JOB_STATUS_RUNNING, QDMI_JOB_STATUS_QUEUED,
                            QDMI_JOB_STATUS_SUBMITTED, QDMI_JOB_STATUS_CREATED,
                            QDMI_JOB_STATUS_FAILED, QDMI_JOB_STATUS_CANCELED,
                            QDMI_JOB_STATUS_DONE}) {
    if (has_status(status)) {
      job->status = status;
      return;
    }
  }
}

/// Refresh one program; DONE is only set once its results are fetched.
int check_program(PASQAL_QDMI_Device_Job job, pasqal::Program &program,
                  pasqal::Deadline deadline = std::nullopt) {
  if (program.status == QDMI_JOB_STATUS_CREATED || is_terminal(program.status) ||
      !job->session->cloud) {
    return QDMI_SUCCESS;
  }
  // Keep the last known status on errors so that a later check can recover.
  const auto code = guarded([&]() -> int {
    const auto status = to_qdmi(job->session->cloud->job_state(program.id, deadline));
    if (!status) {
      return QDMI_SUCCESS;
    }
    if (*status == QDMI_JOB_STATUS_DONE) {
      program.results = to_histogram(job->session->cloud->job_counts(program.id, deadline));
    }
    program.status = *status;
    return QDMI_SUCCESS;
  });
  update_status(job);
  return code;
}

int check_job(PASQAL_QDMI_Device_Job job, pasqal::Deadline deadline = std::nullopt) {
  int error = QDMI_SUCCESS;
  for (auto &program : job->programs) {
    if (const auto code = check_program(job, program, deadline); code != QDMI_SUCCESS) {
      if (code == QDMI_ERROR_TIMEOUT) {
        return code;
      }
      if (error == QDMI_SUCCESS) {
        error = code;
      }
    }
  }
  return error;
}

/// Retain input order and unsubmitted outcomes in a recoverable, opaque job ID.
std::string job_list_id(const std::vector<pasqal::Program> &programs) {
  if (programs.size() == 1 && !programs.front().id.empty()) {
    return programs.front().id;
  }
  auto ids = nlohmann::json::array();
  for (const auto &program : programs) {
    ids.push_back(program.id.empty() ? nlohmann::json(program.status) : nlohmann::json(program.id));
  }
  // Keep jobs distinct even when every submission failed before receiving an ID.
  return std::string(JOB_LIST_PREFIX) +
         nlohmann::json{{"id", unique_job_id()}, {"programs", ids}}.dump();
}

} // namespace

int pasqal::current_exception_status() {
  try {
    throw;
  } catch (const AuthError &) {
    return QDMI_ERROR_PERMISSIONDENIED;
  } catch (const TimeoutError &) {
    return QDMI_ERROR_TIMEOUT;
  } catch (const CloudError &error) {
    if (error.status == 404) {
      return QDMI_ERROR_NOTFOUND;
    }
    return error.status == 401 || error.status == 403 ? QDMI_ERROR_PERMISSIONDENIED
                                                      : QDMI_ERROR_FATAL;
  } catch (const std::bad_alloc &) {
    return QDMI_ERROR_OUTOFMEM;
  } catch (...) {
    return QDMI_ERROR_FATAL;
  }
}

PASQAL_QDMI_Device_Session_impl_d::PASQAL_QDMI_Device_Session_impl_d()
    : authfile(default_authfile()) {
  if (const auto *backend_env = std::getenv("PASQAL_QDMI_BACKEND"); backend_env != nullptr) {
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

int PASQAL_QDMI_device_session_alloc(PASQAL_QDMI_Device_Session *session) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return guarded([session]() -> int {
    *session = new PASQAL_QDMI_Device_Session_impl_d();
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_session_set_parameter(PASQAL_QDMI_Device_Session session,
                                             QDMI_Device_Session_Parameter param, size_t size,
                                             const void *value) {
  if (session == nullptr || !is_valid(param, QDMI_DEVICE_SESSION_PARAMETER_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  std::optional<std::string> *field = nullptr;
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
        !iequals(*text, pasqal::MOCK_BACKEND) && !pasqal::is_device_type(*text)) {
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
      session->specs =
          pasqal::DeviceSpecs::from_pulser(backend, QDMI_DEVICE_STATUS_IDLE, pasqal::MOCK_SPECS);
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
      // Rejected credentials fail the init; other errors leave the device offline.
      try {
        auto entries = cloud->device_spec_entries();
        auto status = QDMI_DEVICE_STATUS_OFFLINE;
        try {
          status = cloud->device_available() ? QDMI_DEVICE_STATUS_IDLE : QDMI_DEVICE_STATUS_OFFLINE;
        } catch (...) {
        }
        session->specs = pasqal::DeviceSpecs::from_pulser(
            backend, status, pasqal::select_device_specs(entries, backend));
        session->specs.pulser_specs = std::move(entries);
      } catch (...) {
        if (const auto code = pasqal::current_exception_status();
            code == QDMI_ERROR_PERMISSIONDENIED) {
          return code;
        }
        session->specs = pasqal::DeviceSpecs::offline(backend);
      }
      session->cloud = std::move(cloud);
    }
    session->initialized = true;
    return QDMI_SUCCESS;
  });
}

void PASQAL_QDMI_device_session_free(PASQAL_QDMI_Device_Session session) { delete session; }

int PASQAL_QDMI_device_session_query_device_property(PASQAL_QDMI_Device_Session session,
                                                     QDMI_Device_Property prop, size_t size,
                                                     void *value, size_t *size_ret) {
  if (session == nullptr || !is_valid(prop, QDMI_DEVICE_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  const auto &specs = session->specs;
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
    case QDMI_DEVICE_PROPERTY_SITES: {
      std::vector<PASQAL_QDMI_Site> sites;
      for (const auto &site : specs.sites) {
        sites.push_back(const_cast<PASQAL_QDMI_Site>(&site));
      }
      return copy_vector(sites, size, value, size_ret);
    }
    case QDMI_DEVICE_PROPERTY_OPERATIONS: {
      std::vector<PASQAL_QDMI_Operation> operations;
      for (const auto &operation : specs.operations) {
        operations.push_back(const_cast<PASQAL_QDMI_Operation>(&operation));
      }
      return copy_vector(operations, size, value, size_ret);
    }
    case QDMI_DEVICE_PROPERTY_LENGTHUNIT:
      return copy_str("um", size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_LENGTHSCALEFACTOR:
      return copy_value(1.0 / pasqal::NM_PER_UM, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_DURATIONUNIT:
      return copy_str("ns", size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR:
      return copy_value(1.0, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_MINATOMDISTANCE:
      return specs.min_atom_distance
                 ? copy_value(*specs.min_atom_distance, size, value, size_ret)
                 : QDMI_ERROR_NOTSUPPORTED;
    case QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS:
      return copy_vector(std::vector{QDMI_PROGRAM_FORMAT_CUSTOM1}, size, value, size_ret);
    case QDMI_DEVICE_PROPERTY_CUSTOM1:
      return specs.pulser_specs.empty()
                 ? QDMI_ERROR_NOTSUPPORTED
                 : copy_str(specs.pulser_specs, size, value, size_ret);
    default:
      return QDMI_ERROR_NOTSUPPORTED;
    }
  });
}

int PASQAL_QDMI_device_session_query_site_property(PASQAL_QDMI_Device_Session session,
                                                   PASQAL_QDMI_Site site, QDMI_Site_Property prop,
                                                   size_t size, void *value, size_t *size_ret) {
  if (session == nullptr || site == nullptr || !is_valid(prop, QDMI_SITE_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  switch (prop) {
  case QDMI_SITE_PROPERTY_INDEX:
    return copy_value(site->index, size, value, size_ret);
  case QDMI_SITE_PROPERTY_XCOORDINATE:
    return site->position ? copy_value(site->position->first, size, value, size_ret)
                          : QDMI_ERROR_NOTSUPPORTED;
  case QDMI_SITE_PROPERTY_YCOORDINATE:
    return site->position ? copy_value(site->position->second, size, value, size_ret)
                          : QDMI_ERROR_NOTSUPPORTED;
  case QDMI_SITE_PROPERTY_ISZONE:
    return copy_value(site->is_zone, size, value, size_ret);
  case QDMI_SITE_PROPERTY_XEXTENT:
  case QDMI_SITE_PROPERTY_YEXTENT:
    return site->zone_extent ? copy_value(*site->zone_extent, size, value, size_ret)
                             : QDMI_ERROR_NOTSUPPORTED;
  default:
    return QDMI_ERROR_NOTSUPPORTED;
  }
}

int PASQAL_QDMI_device_session_query_operation_property(
    PASQAL_QDMI_Device_Session session, PASQAL_QDMI_Operation operation, size_t num_sites,
    const PASQAL_QDMI_Site *sites, size_t num_params, const double *params,
    QDMI_Operation_Property prop, size_t size, void *value, size_t *size_ret) {
  if (session == nullptr || operation == nullptr ||
      (num_sites > 0 && sites == nullptr) || (num_params > 0 && params == nullptr) ||
      !is_valid(prop, QDMI_OPERATION_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!session->initialized) {
    return QDMI_ERROR_BADSTATE;
  }
  return guarded([&]() -> int {
    switch (prop) {
    case QDMI_OPERATION_PROPERTY_NAME:
      return copy_str(operation->name, size, value, size_ret);
    case QDMI_OPERATION_PROPERTY_QUBITSNUM:
      return operation->is_zoned ? QDMI_ERROR_NOTSUPPORTED
                                 : copy_value(size_t{1}, size, value, size_ret);
    case QDMI_OPERATION_PROPERTY_ISZONED:
      return copy_value(operation->is_zoned, size, value, size_ret);
    case QDMI_OPERATION_PROPERTY_SITES: {
      std::vector<PASQAL_QDMI_Site> matching;
      for (const auto &site : session->specs.sites) {
        if (site.is_zone == operation->is_zoned) {
          matching.push_back(const_cast<PASQAL_QDMI_Site>(&site));
        }
      }
      return copy_vector(matching, size, value, size_ret);
    }
    default:
      return QDMI_ERROR_NOTSUPPORTED;
    }
  });
}

int PASQAL_QDMI_device_session_create_device_job(PASQAL_QDMI_Device_Session session,
                                                 PASQAL_QDMI_Device_Job *job) {
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

int PASQAL_QDMI_device_session_retrieve_device_job_by_id(PASQAL_QDMI_Device_Session session,
                                                         const char *job_id,
                                                         PASQAL_QDMI_Device_Job *job) {
  if (session == nullptr || job_id == nullptr || *job_id == '\0' || job == nullptr) {
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
    const std::string_view id(job_id);
    const bool is_list = id.starts_with(JOB_LIST_PREFIX);
    auto ids = nlohmann::json::array({job_id});
    if (is_list) {
      const auto list = nlohmann::json::parse(id.substr(JOB_LIST_PREFIX.size()), nullptr, false);
      if (!list.is_object() || !list.contains("programs")) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      ids = list.at("programs");
      if (!ids.is_array() || ids.empty()) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
    }
    for (const auto &entry : ids) {
      pasqal::Program program;
      if (entry.is_string() && !entry.get_ref<const std::string &>().empty()) {
        program.id = entry.get<std::string>();
        program.status = QDMI_JOB_STATUS_SUBMITTED;
      } else if (entry == QDMI_JOB_STATUS_FAILED || entry == QDMI_JOB_STATUS_CANCELED) {
        program.status = entry.get<QDMI_Job_Status>();
      } else {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      retrieved->programs.push_back(std::move(program));
    }
    update_status(retrieved.get());
    // List IDs already contain the mapping; poll each program when it is queried.
    if (!is_list) {
      if (const auto code = check_job(retrieved.get()); code != QDMI_SUCCESS) {
        return code;
      }
    }
    *job = retrieved.release();
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_job_set_parameter(PASQAL_QDMI_Device_Job job,
                                         QDMI_Device_Job_Parameter param, size_t size,
                                         const void *value) {
  if (job == nullptr || !is_valid(param, QDMI_DEVICE_JOB_PARAMETER_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (param != QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (job->status != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }
  if (value == nullptr) {
    return QDMI_SUCCESS;
  }
  if (size != sizeof(size_t)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  // Pasqal Cloud takes the number of runs as a 32-bit signed integer.
  const auto shots = *static_cast<const size_t *>(value);
  if (shots > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  job->shots = shots;
  return QDMI_SUCCESS;
}

int PASQAL_QDMI_device_job_set_programs(PASQAL_QDMI_Device_Job job, QDMI_Program_Format format,
                                        size_t count, const size_t *sizes,
                                        const void *const *programs) {
  if (job == nullptr || count == 0 || !is_valid(format, QDMI_PROGRAM_FORMAT_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (format != QDMI_PROGRAM_FORMAT_CUSTOM1) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  if (job->status != QDMI_JOB_STATUS_CREATED) {
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
      if (!source || source->size() + 1 != sizes[index] || !nlohmann::json::accept(*source)) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      pasqal::Program program;
      program.source = std::move(source);
      copied.push_back(std::move(program));
    }
    job->programs = std::move(copied);
    return QDMI_SUCCESS;
  });
}

int PASQAL_QDMI_device_job_get_program(PASQAL_QDMI_Device_Job job, size_t program_index,
                                       size_t size, void *data, size_t *size_ret) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (job->programs.empty()) {
    return QDMI_ERROR_BADSTATE;
  }
  if (program_index >= job->programs.size()) {
    return QDMI_ERROR_OUTOFRANGE;
  }
  const auto &source = job->programs[program_index].source;
  return source ? copy_str(*source, size, data, size_ret) : QDMI_ERROR_NOTSUPPORTED;
}

int PASQAL_QDMI_device_job_get_program_status(PASQAL_QDMI_Device_Job job, size_t program_index,
                                              QDMI_Job_Status *status) {
  if (job == nullptr || status == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (job->programs.empty()) {
    return QDMI_ERROR_BADSTATE;
  }
  if (program_index >= job->programs.size()) {
    return QDMI_ERROR_OUTOFRANGE;
  }
  auto &program = job->programs[program_index];
  const auto code = check_program(job, program);
  if (code == QDMI_SUCCESS) {
    *status = program.status;
  }
  return code;
}

int PASQAL_QDMI_device_job_query_property(PASQAL_QDMI_Device_Job job,
                                          QDMI_Device_Job_Property prop, size_t size,
                                          void *value, size_t *size_ret) {
  if (job == nullptr || !is_valid(prop, QDMI_DEVICE_JOB_PROPERTY_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  switch (prop) {
  case QDMI_DEVICE_JOB_PROPERTY_ID:
    return copy_str(job->id, size, value, size_ret);
  case QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT:
    return job->programs.empty() ? QDMI_ERROR_BADSTATE
                                : copy_value(QDMI_PROGRAM_FORMAT_CUSTOM1, size, value, size_ret);
  case QDMI_DEVICE_JOB_PROPERTY_PROGRAMSNUM:
    return job->programs.empty() ? QDMI_ERROR_BADSTATE
                                : copy_value(job->programs.size(), size, value, size_ret);
  case QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM:
    return job->shots ? copy_value(*job->shots, size, value, size_ret) : QDMI_ERROR_BADSTATE;
  default:
    return QDMI_ERROR_NOTSUPPORTED;
  }
}

int PASQAL_QDMI_device_job_submit(PASQAL_QDMI_Device_Job job) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (job->status != QDMI_JOB_STATUS_CREATED || job->programs.empty() || !job->shots) {
    return QDMI_ERROR_BADSTATE;
  }
  return guarded([job]() -> int {
    auto &session = *job->session;
    int error = QDMI_SUCCESS;
    for (auto &program : job->programs) {
      if (error != QDMI_SUCCESS) {
        program.status = QDMI_JOB_STATUS_CANCELED;
        continue;
      }
      error = guarded([&]() -> int {
        if (session.cloud) {
          program.id = session.cloud->submit_job(*program.source, static_cast<int>(*job->shots));
          program.status = QDMI_JOB_STATUS_SUBMITTED;
        } else {
          const auto qubits = nlohmann::json::parse(*program.source).at("register").size();
          program.id = "mock-" + unique_job_id();
          program.results = to_histogram({{std::string(qubits, '0'), *job->shots}});
          program.status = QDMI_JOB_STATUS_DONE;
        }
        return QDMI_SUCCESS;
      });
      if (error != QDMI_SUCCESS) {
        program.status = QDMI_JOB_STATUS_FAILED;
      }
    }
    job->id = job_list_id(job->programs);
    update_status(job);
    return error;
  });
}

int PASQAL_QDMI_device_job_cancel(PASQAL_QDMI_Device_Job job) {
  if (job == nullptr || job->status == QDMI_JOB_STATUS_DONE ||
      job->status == QDMI_JOB_STATUS_FAILED) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (job->status == QDMI_JOB_STATUS_CANCELED) {
    return QDMI_SUCCESS;
  }
  if (job->programs.empty()) {
    job->status = QDMI_JOB_STATUS_CANCELED;
    return QDMI_SUCCESS;
  }
  // The cloud cancels asynchronously and the job may still finish first, so the
  // final status is left to `check`.
  int error = QDMI_SUCCESS;
  for (auto &program : job->programs) {
    if (is_terminal(program.status)) {
      continue;
    }
    if (program.id.empty() || !job->session->cloud) {
      program.status = QDMI_JOB_STATUS_CANCELED;
    } else {
      const auto code = guarded([&]() -> int {
        job->session->cloud->cancel_job(program.id);
        return QDMI_SUCCESS;
      });
      if (error == QDMI_SUCCESS) {
        error = code;
      }
    }
  }
  update_status(job);
  return error;
}

int PASQAL_QDMI_device_job_check(PASQAL_QDMI_Device_Job job, QDMI_Job_Status *status) {
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
  using clock = std::chrono::steady_clock;
  const auto deadline = timeout > 0 ? std::optional{clock::now() + std::chrono::seconds(timeout)}
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
        deadline ? std::min<clock::duration>(delay, *deadline - now) : clock::duration(delay));
    delay = std::min<std::chrono::milliseconds>(delay * 2, MAX_WAIT_DELAY);
  }
}

int PASQAL_QDMI_device_job_get_results(PASQAL_QDMI_Device_Job job, size_t program_index,
                                       QDMI_Job_Result result,
                                       size_t size, void *data, size_t *size_ret) {
  if (job == nullptr || !is_valid(result, QDMI_JOB_RESULT_MAX)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (program_index >= job->programs.size()) {
    return QDMI_ERROR_OUTOFRANGE;
  }
  if (result != QDMI_JOB_RESULT_HIST_KEYS && result != QDMI_JOB_RESULT_HIST_VALUES) {
    return QDMI_ERROR_NOTSUPPORTED;
  }
  auto &program = job->programs[program_index];
  if (const auto code = check_program(job, program); code != QDMI_SUCCESS) {
    return code;
  }
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
