#include "pasqal_cloud.hpp"

#include <gtest/gtest.h>

TEST(CloudValidation, RejectsCredentialUnsafeEndpoints) {
  for (const auto* url :
       {"http://api.example.com", "file:///tmp/token",
        "https://user:password@example.com", "https://@example.com",
        "https://example.com/#fragment", "http://localhost.example.com",
        "http://127.0.0.1@example.com"}) {
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
  EXPECT_THROW(cloud.job_state("../devices"), std::invalid_argument);
  EXPECT_THROW(cloud.cancel_job("id?redirect=elsewhere"),
               std::invalid_argument);
}

TEST(CloudValidation, RejectsMalformedTokenExpiry) {
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOnRydWV9.s", 0));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOiI5OTl4In0.s", 0));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOjEuNX0.s", 0));
}
