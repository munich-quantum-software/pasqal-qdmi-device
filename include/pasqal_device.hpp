/*
 * Session and job state of the Pasqal QDMI device.
 */

#pragma once

#include "pasqal_cloud.hpp"
#include "pasqal_qdmi/device.h"
#include "pasqal_specs.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pasqal {

/// Backend name that selects the offline mock instead of Pasqal Cloud.
inline constexpr std::string_view MOCK_BACKEND = "MOCK";
/// Backend used when neither `PASQAL_QDMI_BACKEND` nor the backend parameter is
/// set.
inline constexpr std::string_view DEFAULT_BACKEND = "EMU_FREE";

/// Map the exception currently being handled to a QDMI status code.
int current_exception_status();

struct Histogram {
  std::string keys; // comma-separated bitstrings, sorted
  std::vector<std::size_t> values;
};

struct Program {
  std::optional<std::string> source;
  std::string id;
  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  std::optional<Histogram> results;
  std::optional<std::size_t> runs;
  std::optional<std::size_t> atom_count;
};

/// Convert Pulser's leftmost-first atom strings to Core's rightmost-first
/// convention.
Histogram make_histogram(const std::map<std::string, std::size_t>& counts,
                         std::optional<std::size_t> atom_count,
                         std::optional<std::size_t> runs);

} // namespace pasqal

struct PASQAL_QDMI_Device_Session_impl_d {
  bool initialized = false;
  std::optional<std::string> base_url;
  std::optional<std::string> token;
  std::optional<std::string> authfile;
  std::optional<std::string> authurl;
  std::optional<std::string> username;
  std::optional<std::string> password;
  std::optional<std::string> backend;
  std::optional<std::string> project_id;
  std::optional<std::string> client_id;
  std::optional<std::string> client_secret;
  pasqal::DeviceSpecs specs;
  std::unique_ptr<pasqal::CloudClient> cloud;

  PASQAL_QDMI_Device_Session_impl_d();
  [[nodiscard]] std::string backend_name() const;
  [[nodiscard]] bool is_mock() const;
};

/// Raw C callers must free jobs before their session; Core retains it
/// automatically.
struct PASQAL_QDMI_Device_Job_impl_d {
  PASQAL_QDMI_Device_Session session;
  std::string id;
  std::string batch_id;
  bool mapping_loaded = false;
  bool submission_attempted = false;
  std::vector<pasqal::Program> programs;
  std::optional<std::size_t> shots = 1024;
  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
};
