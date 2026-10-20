/*
 * QDMI device metadata derived from Pulser device specs.
 */

#pragma once

#include "pasqal_qdmi/constants.h"
#include "pasqal_qdmi/types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/// A trap of the calibrated register layout, or the global addressing zone.
struct PASQAL_QDMI_Site_impl_d {
  std::size_t index;
  std::optional<std::pair<std::int64_t, std::int64_t>> position;
  bool is_zone;
  std::optional<std::uint64_t> zone_extent;
};

/// A Pulser channel.
struct PASQAL_QDMI_Operation_impl_d {
  std::string name;
  bool is_zoned;
};

namespace pasqal {

/// Pulser device specs used by the offline mock backend.
extern const char *const MOCK_SPECS;

/// Lengths are reported as integer nanometres: unit "um", scale factor 1e-3.
inline constexpr double NM_PER_UM = 1000.0;

struct DeviceSpecs {
  std::string name;
  QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
  std::size_t qubits_num = 0;
  std::optional<std::uint64_t> min_atom_distance;
  std::vector<PASQAL_QDMI_Site_impl_d> sites;
  std::vector<PASQAL_QDMI_Operation_impl_d> operations;
  /// The device-spec entries fetched from Pasqal Cloud (see
  /// CloudClient::device_spec_entries); empty for the mock and when offline.
  std::string pulser_specs;

  /// Build specs for backend `name` from a Pulser device in abstract representation.
  static DeviceSpecs from_pulser(const std::string &name, QDMI_Device_Status status,
                                 const std::string &raw);
  /// Build placeholder specs for a device whose specs could not be fetched.
  static DeviceSpecs offline(const std::string &name);
};

} // namespace pasqal
