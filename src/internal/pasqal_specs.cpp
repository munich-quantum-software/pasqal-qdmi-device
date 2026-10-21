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

// Map selected Pasqal backend metadata to truthful QDMI properties.

#include "pasqal-qdmi-device/Specs.hpp"

#include <cstdint>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace pasqal {

DeviceSpecs DeviceSpecs::from_metadata(const std::string& name,
                                       const std::string& raw) {
  const auto metadata = nlohmann::json::parse(raw);
  const auto& capacity = metadata.at("max_atom_num");
  if (!capacity.is_number_unsigned() || capacity.get<std::uint64_t>() == 0 ||
      capacity.get<std::uint64_t>() > std::numeric_limits<std::size_t>::max()) {
    throw std::runtime_error("Invalid Pasqal backend capacity");
  }
  DeviceSpecs specs;
  specs.name = name;
  specs.qubits_num = capacity.get<std::size_t>();
  specs.is_emulator = metadata.at("is_emulator").get<bool>();
  const auto status = metadata.at("status").get<std::string>();
  const auto active = metadata.at("active").get<bool>();
  const auto availability = metadata.at("availability").get<std::string>();
  specs.status = status == "UP" && active && availability == "ACTIVE"
                     ? QDMI_DEVICE_STATUS_IDLE
                     : QDMI_DEVICE_STATUS_OFFLINE;
  specs.backend_metadata = raw;
  return specs;
}

DeviceSpecs DeviceSpecs::mock() {
  DeviceSpecs specs;
  specs.name = "Pasqal offline all-zero transport fixture";
  specs.status = QDMI_DEVICE_STATUS_IDLE;
  specs.qubits_num = 4;
  return specs;
}

} // namespace pasqal
