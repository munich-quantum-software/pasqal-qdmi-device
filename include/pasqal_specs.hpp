/*
 * QDMI device metadata derived from Pasqal Cloud.
 */

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
  std::string pulser_specs;
  std::string backend_metadata;

  /// Use the selected backend's capacity, not an arbitrary QPU's Pulser model.
  static DeviceSpecs from_metadata(const std::string& name,
                                   const std::string& raw);
  /// Deterministic all-zero transport fixture, not a physical simulator.
  static DeviceSpecs mock();
};

} // namespace pasqal
