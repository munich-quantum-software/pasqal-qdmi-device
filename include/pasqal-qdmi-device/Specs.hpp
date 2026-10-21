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

// QDMI device metadata derived from Pasqal Cloud.

#pragma once

#include "pasqal_qdmi/constants.h"

#include <cstddef>
#include <string>

namespace pasqal {

/// Session snapshot; per-program register sites and pulse channels remain
/// Pulser data.
struct DeviceSpecs {
  std::string name;
  QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
  std::size_t qubits_num = 0;
  bool is_emulator = false;
  std::string pulser_specs;
  std::string backend_metadata;

  /// Use the selected backend's capacity, not an arbitrary QPU's Pulser model.
  static DeviceSpecs from_metadata(const std::string& name,
                                   const std::string& raw);
  /// Deterministic all-zero transport fixture, not a physical simulator.
  static DeviceSpecs mock();
};

} // namespace pasqal
