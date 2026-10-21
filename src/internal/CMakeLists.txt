# Copyright (c) 2026 Munich Quantum Software Company GmbH
# Copyright (c) 2025 - 2026 Pasqal SAS
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set(QDMI_INTERNAL_TARGET_NAME pasqal_qdmi_device_internals)

# The device implementation as an object library, shared by the device target and the unit tests.
add_library(${QDMI_INTERNAL_TARGET_NAME} OBJECT)
add_library(pasqal::qdmi_device_internals ALIAS ${QDMI_INTERNAL_TARGET_NAME})
target_compile_features(${QDMI_INTERNAL_TARGET_NAME} PRIVATE cxx_std_20)
set_target_properties(
  ${QDMI_INTERNAL_TARGET_NAME}
  PROPERTIES POSITION_INDEPENDENT_CODE ON
             C_VISIBILITY_PRESET hidden
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN 1)
target_sources(
  ${QDMI_INTERNAL_TARGET_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pasqal_cloud.cpp ${CMAKE_CURRENT_SOURCE_DIR}/pasqal_specs.cpp
          ${PROJECT_SOURCE_DIR}/src/pasqal_device.cpp)
target_include_directories(
  ${QDMI_INTERNAL_TARGET_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include
                                      ${CMAKE_CURRENT_BINARY_DIR}/../include)
get_target_property(qdmi_library_VERSION qdmi::qdmi VERSION)
target_compile_definitions(
  ${QDMI_INTERNAL_TARGET_NAME}
  PRIVATE ${QDMI_PREFIX}_QDMI_DEVICE_ID="${PASQAL_QDMI_DEVICE_ID}"
          QDMI_VERSION="${qdmi_library_VERSION}"
          ${QDMI_PREFIX}_QDMI_DEVICE_VERSION="${PROJECT_VERSION}"
          ${QDMI_PREFIX}_QDMI_device_EXPORTS
          $<$<PLATFORM_ID:Windows>:NOMINMAX>
  INTERFACE $<$<PLATFORM_ID:Windows>:${QDMI_PREFIX}_QDMI_STATIC_DEFINE>)
target_link_libraries(
  ${QDMI_INTERNAL_TARGET_NAME}
  PUBLIC cpr::cpr nlohmann_json::nlohmann_json
  PRIVATE qdmi::qdmi_project_warnings)
