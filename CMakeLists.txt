# Copyright (c) 2026 Munich Quantum Software Company GmbH
# Copyright (c) 2026 Pasqal SAS
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

cmake_minimum_required(VERSION 3.24...4.4)

project(
  pasqal-qdmi-device
  LANGUAGES C CXX
  VERSION 1.0.0
  DESCRIPTION "Pasqal QDMI Device Implementation")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel"
                                               "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(CMakeDependentOption)

option(USE_INSTALLED_PASQAL_QDMI_DEVICE
       "Use an installed version of the Pasqal QDMI Device to build tests" OFF)

if(USE_INSTALLED_PASQAL_QDMI_DEVICE)
  find_package(pasqal-qdmi-device ${PROJECT_VERSION} REQUIRED CONFIG)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE BOOL "Export compile commands" FORCE)

set(CMAKE_VERIFY_INTERFACE_HEADER_SETS
    ON
    CACHE BOOL "Verify interface header sets" FORCE)

set(QDMI_PREFIX "PASQAL")
set(PASQAL_QDMI_DEVICE_ID
    "pasqal.default"
    CACHE STRING "Stable identifier for the Pasqal QDMI Device")

cmake_dependent_option(
  INSTALL_PASQAL_QDMI_DEVICE "Generate installation instructions for the Pasqal QDMI Device"
  ${PROJECT_IS_TOP_LEVEL} "NOT USE_INSTALLED_PASQAL_QDMI_DEVICE" OFF)
option(BUILD_PASQAL_QDMI_TESTS "Build tests for the Pasqal QDMI device" ${PROJECT_IS_TOP_LEVEL})
cmake_dependent_option(
  BUILD_PASQAL_QDMI_LIVE_TESTS
  "Also run the device tests against Pasqal Cloud using ~/.pasqal/config" OFF
  "BUILD_PASQAL_QDMI_TESTS" OFF)

include(cmake/ExternalDependencies.cmake)

set(QDMI_TARGET_NAME "pasqal-qdmi-device")

if(NOT USE_INSTALLED_PASQAL_QDMI_DEVICE)
  add_subdirectory(src)
endif()

if(BUILD_PASQAL_QDMI_TESTS)
  get_target_property(EXPORTED_QDMI_DEVICE_ID ${QDMI_TARGET_NAME} QDMI_DEVICE_ID)
  get_target_property(EXPORTED_QDMI_DEVICE_PREFIX ${QDMI_TARGET_NAME} QDMI_DEVICE_PREFIX)
  get_target_property(EXPORTED_QDMI_MANIFEST_NAME ${QDMI_TARGET_NAME} QDMI_MANIFEST_NAME)
  if(NOT EXPORTED_QDMI_DEVICE_ID STREQUAL "${PASQAL_QDMI_DEVICE_ID}")
    message(FATAL_ERROR "The Pasqal QDMI target does not export its stable ID")
  endif()
  if(NOT EXPORTED_QDMI_DEVICE_PREFIX STREQUAL "${QDMI_PREFIX}")
    message(FATAL_ERROR "The Pasqal QDMI target does not export its symbol prefix")
  endif()
  if(NOT EXPORTED_QDMI_MANIFEST_NAME STREQUAL "${QDMI_TARGET_NAME}.qdmi.json")
    message(FATAL_ERROR "The Pasqal QDMI target does not export its device catalog")
  endif()

  enable_testing()
  include(GoogleTest)
  add_subdirectory(test)
endif()
