// Copyright (c) 2024 - 2026 QDMI Maintainers
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

#include "pasqal_qdmi/device.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>

namespace {

class DeviceMetadataTest : public ::testing::Test {
protected:
  PASQAL_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(PASQAL_QDMI_device_initialize(), QDMI_SUCCESS);
    ASSERT_EQ(PASQAL_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
    ASSERT_EQ(PASQAL_QDMI_device_session_init(session), QDMI_SUCCESS);
  }

  void TearDown() override {
    PASQAL_QDMI_device_session_free(session);
    PASQAL_QDMI_device_finalize();
  }
};

TEST_F(DeviceMetadataTest, ProvidesIdentityAndCapacity) {
  for (const auto property : {
           QDMI_DEVICE_PROPERTY_NAME,
           QDMI_DEVICE_PROPERTY_VERSION,
           QDMI_DEVICE_PROPERTY_LIBRARYVERSION,
           QDMI_DEVICE_PROPERTY_ID,
       }) {
    SCOPED_TRACE(property);
    size_t size = 0;
    ASSERT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, property, 0, nullptr, &size),
              QDMI_SUCCESS);
    ASSERT_GT(size, 1U);
    std::string value(size, '\0');
    ASSERT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, property, size, value.data(), nullptr),
              QDMI_SUCCESS);
    EXPECT_NE(value.front(), '\0');
    EXPECT_EQ(value.back(), '\0');
    EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, property, size - 1, value.data(), nullptr),
              QDMI_ERROR_INVALIDARGUMENT);
  }
  size_t capacity = 0;
  EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(capacity),
                &capacity, nullptr),
            QDMI_SUCCESS);
  EXPECT_GT(capacity, 0U);
}

} // namespace
