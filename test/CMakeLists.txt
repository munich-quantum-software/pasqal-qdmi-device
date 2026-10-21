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

# ------------------------------------------------------------------------------
# Non-Functional Tests
# ------------------------------------------------------------------------------

generate_device_defs_executable(${QDMI_PREFIX} TARGET ${QDMI_TARGET_NAME})

# ------------------------------------------------------------------------------
# Functional Tests
# ------------------------------------------------------------------------------

set(QDMI_TEST_TARGET_NAME "${QDMI_TARGET_NAME}-test")

# test_pasqal_device.cpp is the upstream QDMI device template test, generated with the PASQAL
# prefix. Sessions query public EMU_FREE metadata.
add_executable(${QDMI_TEST_TARGET_NAME} test_pasqal_device.cpp)

target_link_libraries(
  ${QDMI_TEST_TARGET_NAME} PRIVATE GTest::gtest_main qdmi::qdmi ${QDMI_TARGET_NAME}
                                   qdmi::qdmi_project_warnings)

target_compile_features(${QDMI_TEST_TARGET_NAME} PRIVATE cxx_std_20)

add_dependencies(${QDMI_TEST_TARGET_NAME} qdmi_test_pasqal_device_defs)

if(WIN32)
  add_custom_command(
    TARGET ${QDMI_TEST_TARGET_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${QDMI_TARGET_NAME}>
            $<TARGET_FILE_DIR:${QDMI_TEST_TARGET_NAME}>)
endif()

gtest_discover_tests(
  ${QDMI_TEST_TARGET_NAME}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  PROPERTIES ENVIRONMENT "PASQAL_QDMI_BACKEND=EMU_FREE" LABELS integration DISCOVERY_TIMEOUT 60)

if(NOT USE_INSTALLED_PASQAL_QDMI_DEVICE)
  add_executable(${QDMI_TARGET_NAME}-unit-test unit/test_pasqal_cloud.cpp unit/test_pasqal_unit.cpp)
  target_link_libraries(
    ${QDMI_TARGET_NAME}-unit-test PRIVATE GTest::gtest_main pasqal::qdmi_device_internals
                                          qdmi::qdmi_project_warnings)
  target_compile_features(${QDMI_TARGET_NAME}-unit-test PRIVATE cxx_std_20)
  gtest_discover_tests(${QDMI_TARGET_NAME}-unit-test DISCOVERY_TIMEOUT 60)
endif()
