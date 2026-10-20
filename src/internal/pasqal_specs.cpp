/*
 * Map Pulser device specs onto QDMI sites and operations.
 */

#include "pasqal_specs.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <nlohmann/json.hpp>

namespace pasqal {
namespace {

std::int64_t to_nm(double um) { return std::llround(um * NM_PER_UM); }

template <typename T> std::optional<T> optional_field(const nlohmann::json &j, const char *key) {
  if (!j.contains(key) || j[key].is_null()) {
    return std::nullopt;
  }
  return j[key].get<T>();
}

} // namespace

const char *const MOCK_SPECS = R"({
  "name": "MOCK",
  "max_atom_num": 4,
  "min_atom_distance": 5,
  "max_radial_distance": 10,
  "pre_calibrated_layouts": [
    {"coordinates": [[-2.5, -2.5], [2.5, -2.5], [-2.5, 2.5], [2.5, 2.5]]}
  ],
  "channels": [
    {"id": "rydberg_global", "addressing": "Global"},
    {"id": "raman_local", "addressing": "Local"}
  ]
})";

DeviceSpecs DeviceSpecs::from_pulser(const std::string &name, QDMI_Device_Status status,
                                     const std::string &raw) {
  const auto device = nlohmann::json::parse(raw);
  const auto max_atom_num = optional_field<std::size_t>(device, "max_atom_num");
  DeviceSpecs specs;
  specs.name = name;
  specs.status = status;
  if (const auto layouts = device.value("pre_calibrated_layouts", nlohmann::json::array());
      !layouts.empty()) {
    for (const auto &xy : layouts.front().at("coordinates")) {
      specs.sites.push_back({.index = specs.sites.size(),
                             .position = std::pair{to_nm(xy.at(0).get<double>()),
                                                   to_nm(xy.at(1).get<double>())},
                             .is_zone = false,
                             .zone_extent = std::nullopt});
    }
  } else {
    for (std::size_t index = 0; index < max_atom_num.value_or(0); ++index) {
      specs.sites.push_back({.index = index,
                             .position = std::nullopt,
                             .is_zone = false,
                             .zone_extent = std::nullopt});
    }
  }
  for (const auto &channel : device.at("channels")) {
    auto addressing = channel.at("addressing").get<std::string>();
    std::transform(addressing.begin(), addressing.end(), addressing.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    specs.operations.push_back(
        {.name = channel.at("id").get<std::string>(), .is_zoned = addressing == "global"});
  }
  const auto traps = specs.sites.size();
  if (std::any_of(specs.operations.begin(), specs.operations.end(),
                  [](const auto &operation) { return operation.is_zoned; })) {
    // The zone is the square of side 2R centred on the register; its coordinate is
    // the corner, which keeps it distinct from a trap at the origin.
    const auto radius = optional_field<double>(device, "max_radial_distance");
    const auto r = radius ? std::optional{to_nm(*radius)} : std::nullopt;
    specs.sites.push_back(
        {.index = traps,
         .position = r ? std::optional{std::pair{-*r, -*r}} : std::nullopt,
         .is_zone = true,
         .zone_extent = r ? std::optional{static_cast<std::uint64_t>(2 * *r)} : std::nullopt});
  }
  specs.qubits_num = max_atom_num.value_or(traps);
  if (const auto distance = optional_field<double>(device, "min_atom_distance")) {
    specs.min_atom_distance = static_cast<std::uint64_t>(to_nm(*distance));
  }
  return specs;
}

DeviceSpecs DeviceSpecs::offline(const std::string &name) {
  DeviceSpecs specs;
  specs.name = name;
  return specs;
}

} // namespace pasqal
