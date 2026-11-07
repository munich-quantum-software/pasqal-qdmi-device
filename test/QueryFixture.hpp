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

#pragma once

#include "pasqal_qdmi/device.h"

#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <string_view>
#include <utility>

class QDMIQueryFixture : public ::testing::Test {
protected:
  PASQAL_QDMI_Device_Session session = nullptr;

  static void SetUpTestSuite() {
    ASSERT_EQ(PASQAL_QDMI_device_initialize(), QDMI_SUCCESS);
  }
  static void TearDownTestSuite() {
    EXPECT_EQ(PASQAL_QDMI_device_finalize(), QDMI_SUCCESS);
  }
  void SetUp() override {
    const char* url = std::getenv("PASQAL_QDMI_QUERY_URL");
    ASSERT_NE(url, nullptr);
    ASSERT_TRUE(std::string_view(url).starts_with("http://127.0.0.1:"));
    ASSERT_EQ(PASQAL_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
    for (const auto& [parameter, value] : {
             std::pair{QDMI_DEVICE_SESSION_PARAMETER_BASEURL, url},
             std::pair{QDMI_DEVICE_SESSION_PARAMETER_TOKEN, "token"},
             std::pair{QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "EMU_FREE"},
             std::pair{QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3, "project"},
         }) {
      ASSERT_EQ(PASQAL_QDMI_device_session_set_parameter(
                    session, parameter, std::strlen(value) + 1, value),
                QDMI_SUCCESS);
    }
    ASSERT_EQ(PASQAL_QDMI_device_session_init(session), QDMI_SUCCESS);
  }
  void TearDown() override { PASQAL_QDMI_device_session_free(session); }
};

TEST_F(QDMIQueryFixture, RequiredProperties) {
  for (const auto property : {
           QDMI_DEVICE_PROPERTY_NAME,
           QDMI_DEVICE_PROPERTY_ID,
           QDMI_DEVICE_PROPERTY_VERSION,
           QDMI_DEVICE_PROPERTY_LIBRARYVERSION,
           QDMI_DEVICE_PROPERTY_STATUS,
           QDMI_DEVICE_PROPERTY_QUBITSNUM,
           QDMI_DEVICE_PROPERTY_SITES,
           QDMI_DEVICE_PROPERTY_OPERATIONS,
           QDMI_DEVICE_PROPERTY_QUEUELENGTH,
           QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS,
       }) {
    EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, property, 0, nullptr, nullptr),
              QDMI_SUCCESS);
  }
}
