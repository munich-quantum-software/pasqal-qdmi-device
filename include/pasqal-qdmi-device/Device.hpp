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

// Session and job state of the Pasqal QDMI device.

#pragma once

#include "pasqal-qdmi-device/Cloud.hpp"
#include "pasqal-qdmi-device/Specs.hpp"
#include "pasqal_qdmi/device.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pasqal {

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
};

/// Raw C callers must free jobs before their session; Core retains it
/// automatically.
struct PASQAL_QDMI_Device_Job_impl_d {
  PASQAL_QDMI_Device_Session session;
  std::string id;
  bool is_batch = false;
  bool mapping_loaded = false;
  bool submission_attempted = false;
  std::vector<pasqal::Program> programs;
  std::optional<std::size_t> shots = 1024;
  // Batch snapshot; indexed reads only update their selected program.
  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
};
