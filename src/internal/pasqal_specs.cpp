/*
 * Map selected Pasqal backend metadata to truthful QDMI properties.
 */

#include "pasqal_specs.hpp"

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
