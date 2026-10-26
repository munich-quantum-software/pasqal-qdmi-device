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

#include "pasqal-qdmi-device/Device.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace {

int set_str(PASQAL_QDMI_Device_Session session,
            QDMI_Device_Session_Parameter param, const std::string& value) {
  return PASQAL_QDMI_device_session_set_parameter(
      session, param, value.size() + 1, value.c_str());
}

PASQAL_QDMI_Device_Session session_with(
    const std::vector<std::pair<QDMI_Device_Session_Parameter, std::string>>&
        params) {
  PASQAL_QDMI_Device_Session session = nullptr;
  EXPECT_EQ(PASQAL_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  for (const auto& [param, value] : params) {
    EXPECT_EQ(set_str(session, param, value), QDMI_SUCCESS);
  }
  return session;
}

PASQAL_QDMI_Device_Session initialized_session() {
  // Seed private state for tests that do not exercise cloud initialization.
  auto* session = session_with({});
  session->initialized = true;
  session->specs.qubits_num = 4;
  return session;
}

int set_shots(PASQAL_QDMI_Device_Job job, size_t shots) {
  return PASQAL_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(size_t), &shots);
}

int set_programs(PASQAL_QDMI_Device_Job job,
                 const std::vector<std::string>& programs) {
  std::vector<size_t> sizes;
  std::vector<const void*> data;
  for (const auto& program : programs) {
    sizes.push_back(program.size() + 1);
    data.push_back(program.c_str());
  }
  return PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_CUSTOM1,
                                             programs.size(), sizes.data(),
                                             data.data());
}

const std::string ONE_ATOM =
    R"({"version":"1","device":{},"channels":{},"variables":{},"operations":[],"measurement":"ground-rydberg","register":[{"name":"right","x":0,"y":0}]})";
const std::string TWO_ATOMS =
    R"({"version":"1","device":{},"channels":{},"variables":{},"operations":[],"measurement":"ground-rydberg","register":[{"name":"right","x":0,"y":0},{"name":"ancilla-7","x":5,"y":0}]})";

class ProgramListTest : public ::testing::Test {
protected:
  PASQAL_QDMI_Device_Session session = nullptr;
  PASQAL_QDMI_Device_Job job = nullptr;

  void SetUp() override {
    session = initialized_session();
    ASSERT_EQ(PASQAL_QDMI_device_session_create_device_job(session, &job),
              QDMI_SUCCESS);
  }

  void TearDown() override {
    PASQAL_QDMI_device_job_free(job);
    PASQAL_QDMI_device_session_free(session);
  }

  std::string source(size_t index) {
    size_t size = 0;
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, index, 0, nullptr, &size),
              QDMI_SUCCESS);
    std::string data(size, '\0');
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, index, size, data.data(),
                                                 nullptr),
              QDMI_SUCCESS);
    data.pop_back();
    return data;
  }
};

} // namespace

TEST(Session, AllocRejectsNull) {
  EXPECT_EQ(PASQAL_QDMI_device_session_alloc(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST(Session, NullValueProbesWithoutClearing) {
  auto* session =
      session_with({{QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "EMU_FREE"}});
  const auto probe = [session](QDMI_Device_Session_Parameter param) {
    return PASQAL_QDMI_device_session_set_parameter(session, param, 0, nullptr);
  };
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1), QDMI_SUCCESS);
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE), QDMI_SUCCESS);
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_CHILDDEVICE),
            QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2),
            QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(session->backend_name(), "EMU_FREE");
  PASQAL_QDMI_device_session_free(session);
}

TEST(Session, BackendIdentifiersAreValidatedWithoutAnAllowlist) {
  auto* session = session_with({});
  for (const auto* backend : {"emu_tn", "FRESNEL_SA1", "EMU_FUTURE"}) {
    EXPECT_EQ(set_str(session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, backend),
              QDMI_SUCCESS);
  }
  for (const auto* backend : {"", "../devices", "EMU_FREE?query=1"}) {
    EXPECT_EQ(set_str(session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, backend),
              QDMI_ERROR_INVALIDARGUMENT);
  }
  PASQAL_QDMI_device_session_free(session);
}

TEST(Session, SecondInitAndLateParametersAreBadState) {
  auto* session = initialized_session();
  EXPECT_EQ(PASQAL_QDMI_device_session_init(session), QDMI_ERROR_BADSTATE);
  EXPECT_EQ(set_str(session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "EMU_FREE"),
            QDMI_ERROR_BADSTATE);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Job, ShotsBeyondCloudRangeAreRejected) {
  auto* session = initialized_session();
  PASQAL_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(PASQAL_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);
  EXPECT_EQ(set_shots(job, static_cast<size_t>(INT32_MAX) + 1),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(set_shots(job, 100), QDMI_SUCCESS);
  PASQAL_QDMI_device_job_free(job);
  PASQAL_QDMI_device_session_free(session);
}

TEST_F(ProgramListTest, UnsubmittedJobsRejectWaitAndResults) {
  EXPECT_EQ(PASQAL_QDMI_device_job_wait(job, 0), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_ERROR_BADSTATE);
  QDMI_Job_Status status{};
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, 0, &status),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_QASM3,
                                                1, nullptr, nullptr),
            QDMI_ERROR_NOTSUPPORTED);
  ASSERT_EQ(set_programs(job, {TWO_ATOMS}), QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(
                job, 0, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, nullptr),
            QDMI_ERROR_BADSTATE);
}

TEST(Session, EmptyTopologyLeavesOutputBuffersUntouched) {
  auto* session = initialized_session();
  for (const auto property :
       {QDMI_DEVICE_PROPERTY_SITES, QDMI_DEVICE_PROPERTY_OPERATIONS}) {
    size_t size = 99;
    EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, property, 0, nullptr, &size),
              QDMI_SUCCESS);
    EXPECT_EQ(size, 0U);
    char sentinel = '#';
    EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, property, 0, &sentinel, nullptr),
              QDMI_SUCCESS);
    EXPECT_EQ(sentinel, '#');
    EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, property, sizeof(sentinel), &sentinel, &size),
              QDMI_SUCCESS);
    EXPECT_EQ(size, 0U);
    EXPECT_EQ(sentinel, '#');
  }
  EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_COUPLINGMAP, 0, nullptr, nullptr),
            QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_CUSTOM1, 0, nullptr, nullptr),
            QDMI_ERROR_NOTSUPPORTED);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Cloud, ErrorsMapToQdmiStatus) {
  const auto status_of = [](auto error) {
    try {
      throw error;
    } catch (...) {
      return pasqal::current_exception_status();
    }
  };
  EXPECT_EQ(status_of(pasqal::CloudError(404, "")), QDMI_ERROR_NOTFOUND);
  EXPECT_EQ(status_of(pasqal::CloudError(401, "")),
            QDMI_ERROR_PERMISSIONDENIED);
  EXPECT_EQ(status_of(pasqal::CloudError(403, "")),
            QDMI_ERROR_PERMISSIONDENIED);
  EXPECT_EQ(status_of(pasqal::CloudError(503, "")), QDMI_ERROR_FATAL);
  EXPECT_EQ(status_of(pasqal::CloudError(0, "connection refused")),
            QDMI_ERROR_FATAL);
  EXPECT_EQ(status_of(pasqal::AuthError("rejected")),
            QDMI_ERROR_PERMISSIONDENIED);
  EXPECT_EQ(status_of(pasqal::TimeoutError("expired")), QDMI_ERROR_TIMEOUT);
}

TEST(Cloud, TokenUsability) {
  EXPECT_FALSE(pasqal::is_token_usable("", 0));
  EXPECT_TRUE(pasqal::is_token_usable("opaque-token", 0));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOjF9.s", 100)); // exp 1
  EXPECT_TRUE(pasqal::is_token_usable("h.eyJleHAiOjk5OTk5OTk5OTl9.s", 100));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOiI5OTk5OTk5OTk5In0.s", 100));
  EXPECT_TRUE(pasqal::is_token_usable("h.eyJleHAiOjExMC41fQ.s", 100));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOjExMC41fQ.s", 101));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOjk5OTk5OTk5OTl9.s",
                                       9999999995));                // grace
  EXPECT_TRUE(pasqal::is_token_usable("h.eyJzdWIiOiJ4In0.s", 100)); // no exp
  EXPECT_FALSE(pasqal::is_token_usable("h.!!!.s", 100));
}

TEST(Cloud, ExpiredDeadlineStopsStatusRequest) {
  pasqal::CloudConfig config;
  config.base_url = "http://127.0.0.1:1";
  config.token = "token";
  pasqal::CloudClient cloud(config);
  EXPECT_THROW(cloud.job("job", std::chrono::steady_clock::now() -
                                    std::chrono::seconds(1)),
               pasqal::TimeoutError);
}

TEST(Cloud, ExpiredDeadlineStopsTokenRefresh) {
  pasqal::CloudConfig config;
  config.auth_endpoint = "http://127.0.0.1:1";
  config.username = "user";
  config.password = "password";
  pasqal::CloudClient cloud(config);
  EXPECT_THROW(cloud.job("job", std::chrono::steady_clock::now() -
                                    std::chrono::seconds(1)),
               pasqal::TimeoutError);
}

TEST_F(ProgramListTest, CopiesAndReplacesProgramsAtomically) {
  size_t count = 0;
  EXPECT_EQ(PASQAL_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_PROGRAMSNUM, sizeof(count),
                &count, nullptr),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, 0, 0, nullptr, nullptr),
            QDMI_ERROR_BADSTATE);
  ASSERT_EQ(set_programs(job, {ONE_ATOM, TWO_ATOMS}), QDMI_SUCCESS);
  EXPECT_EQ(source(0), ONE_ATOM);
  EXPECT_EQ(source(1), TWO_ATOMS);
  EXPECT_EQ(set_programs(job, {TWO_ATOMS, "not JSON"}),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(source(0), ONE_ATOM);
  EXPECT_EQ(source(1), TWO_ATOMS);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(
                job, QDMI_PROGRAM_FORMAT_CUSTOM1, 3, nullptr, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_PROGRAMSNUM, sizeof(count),
                &count, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(count, 2U);
  ASSERT_EQ(set_programs(job, {TWO_ATOMS}), QDMI_SUCCESS);
  EXPECT_EQ(source(0), TWO_ATOMS);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, 1, 0, nullptr, nullptr),
            QDMI_ERROR_OUTOFRANGE);
}

TEST_F(ProgramListTest, RejectsInvalidProgramText) {
  const void* data = ONE_ATOM.c_str();
  size_t size = ONE_ATOM.size();
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(
                job, QDMI_PROGRAM_FORMAT_CUSTOM1, 1, &size, &data),
            QDMI_ERROR_INVALIDARGUMENT);
  const std::string trailing = ONE_ATOM + std::string("\0extra", 6);
  data = trailing.c_str();
  size = trailing.size() + 1;
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(
                job, QDMI_PROGRAM_FORMAT_CUSTOM1, 1, &size, &data),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(
                job, QDMI_PROGRAM_FORMAT_CUSTOM1, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(ProgramListTest, RejectsIncompleteOrUnbuiltPulserEnvelopesAtomically) {
  ASSERT_EQ(set_programs(job, {ONE_ATOM}), QDMI_SUCCESS);
  for (const auto& program :
       {std::string("{}"), std::string("[]"), std::string("null")}) {
    EXPECT_EQ(set_programs(job, {TWO_ATOMS, program}),
              QDMI_ERROR_INVALIDARGUMENT);
    EXPECT_EQ(source(0), ONE_ATOM);
  }
  auto program = nlohmann::json::parse(TWO_ATOMS);
  program["measurement"] = nullptr;
  EXPECT_EQ(set_programs(job, {program.dump()}), QDMI_ERROR_NOTSUPPORTED);
  program["measurement"] = "ground-rydberg";
  program["variables"] = {{"t", {{"type", "int"}, {"value", {4}}}}};
  EXPECT_EQ(set_programs(job, {program.dump()}), QDMI_ERROR_NOTSUPPORTED);
  program["variables"] = nlohmann::json::object();
  program["register"][1]["name"] = "right";
  EXPECT_EQ(set_programs(job, {program.dump()}), QDMI_ERROR_INVALIDARGUMENT);
  program["register"][1]["name"] = "different";
  program["register"][1].erase("x");
  EXPECT_EQ(set_programs(job, {program.dump()}), QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(source(0), ONE_ATOM);
}

TEST_F(ProgramListTest, BufferSizesAndFailureOutputsAreBounded) {
  ASSERT_EQ(set_programs(job, {TWO_ATOMS}), QDMI_SUCCESS);
  size_t size = 0;
  ASSERT_EQ(PASQAL_QDMI_device_job_get_program(job, 0, 999, nullptr, &size),
            QDMI_SUCCESS);
  EXPECT_EQ(size, TWO_ATOMS.size() + 1);
  std::vector<char> buffer(size + 2, '#');
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, 0, size - 1, buffer.data(),
                                               nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(buffer.front(), '#');
  ASSERT_EQ(
      PASQAL_QDMI_device_job_get_program(job, 0, size, buffer.data(), nullptr),
      QDMI_SUCCESS);
  EXPECT_EQ(buffer[size - 1], '\0');
  EXPECT_EQ(buffer[size], '#');
  job->programs[0].status = QDMI_JOB_STATUS_DONE;
  job->programs[0].results = pasqal::Histogram{"10", {7}};
  size_t count = 42;
  EXPECT_EQ(
      PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_HIST_VALUES,
                                         sizeof(count) - 1, &count, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(count, 42U);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_SHOTS, 0,
                                               nullptr, nullptr),
            QDMI_ERROR_NOTSUPPORTED);
  QDMI_Job_Status status{};
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, 1, &status),
            QDMI_ERROR_OUTOFRANGE);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(
                job, 1, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, nullptr),
            QDMI_ERROR_OUTOFRANGE);
  job->submission_attempted = true;
  job->status = QDMI_JOB_STATUS_DONE;
  EXPECT_EQ(set_programs(job, {ONE_ATOM}), QDMI_ERROR_BADSTATE);
  EXPECT_EQ(PASQAL_QDMI_device_job_cancel(job), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_wait(job, 0), QDMI_SUCCESS);
  EXPECT_EQ(
      PASQAL_QDMI_device_job_wait(job, std::numeric_limits<size_t>::max()),
      QDMI_SUCCESS);
}

TEST_F(ProgramListTest, CancelsAllUnsubmittedPrograms) {
  ASSERT_EQ(set_programs(job, {ONE_ATOM, TWO_ATOMS}), QDMI_SUCCESS);
  ASSERT_EQ(PASQAL_QDMI_device_job_cancel(job), QDMI_SUCCESS);
  QDMI_Job_Status status{};
  for (size_t index = 0; index < 2; ++index) {
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, index, &status),
              QDMI_SUCCESS);
    EXPECT_EQ(status, QDMI_JOB_STATUS_CANCELED);
  }
  EXPECT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_ERROR_BADSTATE);
}

TEST(Specs, UsesSelectedBackendCapacityAndAvailability) {
  const auto metadata =
      R"({"max_atom_num":12,"status":"UP","active":true,"availability":"ACTIVE","is_emulator":true})";
  const auto specs = pasqal::DeviceSpecs::from_metadata("EMU_FREE", metadata);
  EXPECT_EQ(specs.qubits_num, 12U);
  EXPECT_EQ(specs.status, QDMI_DEVICE_STATUS_IDLE);
  EXPECT_EQ(specs.backend_metadata, metadata);
  for (const auto* capacity : {"0", "-1", "1.5", "true", "null"}) {
    const std::string invalid =
        std::string("{\"max_atom_num\":") + capacity + "}";
    EXPECT_THROW(pasqal::DeviceSpecs::from_metadata("EMU_FREE", invalid),
                 std::exception);
  }
}

TEST(Results, RejectsEmptyAndOverflowingCounters) {
  EXPECT_THROW(pasqal::make_histogram({}, 3, 9), pasqal::CloudError);
  EXPECT_THROW(pasqal::make_histogram(
                   {{"00", std::numeric_limits<size_t>::max()}, {"01", 1}}, 2,
                   std::nullopt),
               pasqal::CloudError);
}

TEST_F(ProgramListTest, ChecksDoNotChangeUnsubmittedParameters) {
  QDMI_Job_Status status{};
  ASSERT_EQ(PASQAL_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_CREATED);
  size_t shots = 0;
  ASSERT_EQ(PASQAL_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM, sizeof(shots), &shots,
                nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(shots, 1024U);
  EXPECT_EQ(set_shots(job, 0), QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(ProgramListTest, CopiesInputAndRejectsOverCapacityWithoutChangingIt) {
  auto mutable_program = ONE_ATOM;
  ASSERT_EQ(set_programs(job, {mutable_program}), QDMI_SUCCESS);
  mutable_program[0] = '!';
  EXPECT_EQ(source(0), ONE_ATOM);
  auto sequence = nlohmann::json::parse(TWO_ATOMS);
  for (size_t i = 2; i < 5; ++i) {
    sequence["register"].push_back({{"name", i}, {"x", i * 5}, {"y", 0}});
  }
  EXPECT_EQ(set_programs(job, {sequence.dump()}), QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(source(0), ONE_ATOM);
}

TEST_F(ProgramListTest, RejectsNullOutputsAndForeignMetadataHandles) {
  EXPECT_EQ(PASQAL_QDMI_device_job_check(job, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_session_query_site_property(
                session, reinterpret_cast<PASQAL_QDMI_Site>(job),
                QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_session_query_operation_property(
                session, reinterpret_cast<PASQAL_QDMI_Operation>(job), 0,
                nullptr, 0, nullptr, QDMI_OPERATION_PROPERTY_NAME, 0, nullptr,
                nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(ProgramListTest, RejectsInvalidEnumsAndNullMetadataSessions) {
  EXPECT_EQ(PASQAL_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_CUSTOM1, 0, nullptr),
            QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_MAX, 1,
                                                nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_MAX, 0,
                                               nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                nullptr, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_session_query_site_property(
                nullptr, nullptr, QDMI_SITE_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_session_query_operation_property(
                nullptr, nullptr, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}
