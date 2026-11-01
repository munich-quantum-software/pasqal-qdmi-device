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

#include "pasqal-qdmi-device/Cloud.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(CloudValidation, RejectsCredentialUnsafeEndpoints) {
  for (const auto* url : {
           "http://api.example.com",
           "api.example.com",
           "",
           "file:///tmp/token",
           "https://user:password@example.com",
           "https://@example.com",
           "https://example.com/#fragment",
           "http://localhost.example.com",
           "http://127.0.0.1@example.com",
       }) {
    pasqal::CloudConfig config;
    config.base_url = url;
    EXPECT_THROW(pasqal::CloudClient{config}, std::invalid_argument) << url;
    config.base_url = std::string(pasqal::DEFAULT_BASE_URL);
    config.auth_endpoint = url;
    EXPECT_THROW(pasqal::CloudClient{config}, std::invalid_argument) << url;
  }
}

TEST(CloudValidation, RejectsBearerHeaderInjection) {
  pasqal::CloudConfig config;
  config.token = "token\r\nX-Leak: secret";
  EXPECT_THROW(pasqal::CloudClient{config}, std::invalid_argument);
}

TEST(CloudValidation, RejectsPathInjectionBeforeNetworkAccess) {
  pasqal::CloudConfig config;
  config.base_url = "http://127.0.0.1:1";
  config.token = "opaque";
  pasqal::CloudClient cloud(config);
  EXPECT_THROW(cloud.job("../devices"), std::invalid_argument);
  EXPECT_THROW(cloud.cancel_job("id?redirect=elsewhere"),
               std::invalid_argument);
}

TEST(CloudValidation, RejectsMalformedTokenExpiry) {
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOnRydWV9.s", 0));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOiI5OTl4In0.s", 0));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOjEuNX0.s", 0));
}
