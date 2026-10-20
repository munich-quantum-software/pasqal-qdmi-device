#include "pasqal-qdmi-device/constants.h"
#include "pasqal_device.hpp"

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

int set_str(PASQAL_QDMI_Device_Session session, QDMI_Device_Session_Parameter param,
            const std::string &value) {
  return PASQAL_QDMI_device_session_set_parameter(session, param, value.size() + 1,
                                                  value.c_str());
}

PASQAL_QDMI_Device_Session
session_with(const std::vector<std::pair<QDMI_Device_Session_Parameter, std::string>> &params) {
  PASQAL_QDMI_Device_Session session = nullptr;
  EXPECT_EQ(PASQAL_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  for (const auto &[param, value] : params) {
    EXPECT_EQ(set_str(session, param, value), QDMI_SUCCESS);
  }
  return session;
}

PASQAL_QDMI_Device_Session mock_session() {
  auto *session = session_with({{QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "MOCK"}});
  EXPECT_EQ(PASQAL_QDMI_device_session_init(session), QDMI_SUCCESS);
  return session;
}

int set_shots(PASQAL_QDMI_Device_Job job, size_t shots) {
  return PASQAL_QDMI_device_job_set_parameter(job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM,
                                              sizeof(size_t), &shots);
}

int set_programs(PASQAL_QDMI_Device_Job job, const std::vector<std::string> &programs) {
  std::vector<size_t> sizes;
  std::vector<const void *> data;
  for (const auto &program : programs) {
    sizes.push_back(program.size() + 1);
    data.push_back(program.c_str());
  }
  return PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_CUSTOM1, programs.size(),
                                             sizes.data(), data.data());
}

const std::string ONE_ATOM = R"({"register": [{"name": "q0", "x": 0, "y": 0}]})";
const std::string TWO_ATOMS =
    R"({"register": [{"name": "q0", "x": 0, "y": 0}, {"name": "q1", "x": 5, "y": 0}]})";

class ProgramListTest : public ::testing::Test {
protected:
  PASQAL_QDMI_Device_Session session = nullptr;
  PASQAL_QDMI_Device_Job job = nullptr;

  void SetUp() override {
    session = mock_session();
    ASSERT_EQ(PASQAL_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);
  }

  void TearDown() override {
    PASQAL_QDMI_device_job_free(job);
    PASQAL_QDMI_device_session_free(session);
  }

  std::string source(size_t index) {
    size_t size = 0;
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, index, 0, nullptr, &size), QDMI_SUCCESS);
    std::string data(size, '\0');
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, index, size, data.data(), nullptr), QDMI_SUCCESS);
    data.pop_back();
    return data;
  }
};

} // namespace

TEST(Session, AllocRejectsNull) {
  EXPECT_EQ(PASQAL_QDMI_device_session_alloc(nullptr), QDMI_ERROR_INVALIDARGUMENT);
}

TEST(Session, NullValueProbesWithoutClearing) {
  auto *session = session_with({{QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "MOCK"}});
  const auto probe = [session](QDMI_Device_Session_Parameter param) {
    return PASQAL_QDMI_device_session_set_parameter(session, param, 0, nullptr);
  };
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1), QDMI_SUCCESS);
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE), QDMI_SUCCESS);
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_CHILDDEVICE), QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(probe(QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2), QDMI_ERROR_NOTSUPPORTED);
  EXPECT_TRUE(session->is_mock());
  PASQAL_QDMI_device_session_free(session);
}

TEST(Session, UnknownBackendIsRejected) {
  auto *session = session_with({});
  EXPECT_EQ(set_str(session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "FRESNL"),
            QDMI_ERROR_NOTSUPPORTED);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Session, InitWithoutCredentialsIsDenied) {
  auto *session = session_with({{QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "FRESNEL"},
                                {QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE, "/nonexistent"}});
  EXPECT_EQ(PASQAL_QDMI_device_session_init(session), QDMI_ERROR_PERMISSIONDENIED);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Session, SecondInitAndLateParametersAreBadState) {
  auto *session = mock_session();
  EXPECT_EQ(PASQAL_QDMI_device_session_init(session), QDMI_ERROR_BADSTATE);
  EXPECT_EQ(set_str(session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "EMU_FREE"),
            QDMI_ERROR_BADSTATE);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Job, CheckErrorKeepsJobStatus) {
  auto *session = session_with({{QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, "FRESNEL"},
                                {QDMI_DEVICE_SESSION_PARAMETER_BASEURL, "http://127.0.0.1:1"},
                                {QDMI_DEVICE_SESSION_PARAMETER_TOKEN, "token"}});
  ASSERT_EQ(PASQAL_QDMI_device_session_init(session), QDMI_SUCCESS);
  PASQAL_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(PASQAL_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);
  pasqal::Program program;
  program.id = "job";
  program.status = QDMI_JOB_STATUS_RUNNING;
  job->programs.push_back(program);
  job->status = QDMI_JOB_STATUS_RUNNING;
  QDMI_Job_Status status{};
  EXPECT_EQ(PASQAL_QDMI_device_job_check(job, &status), QDMI_ERROR_FATAL);
  EXPECT_EQ(job->status, QDMI_JOB_STATUS_RUNNING);
  PASQAL_QDMI_device_job_free(job);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Job, ShotsBeyondCloudRangeAreRejected) {
  auto *session = mock_session();
  PASQAL_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(PASQAL_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);
  EXPECT_EQ(set_shots(job, static_cast<size_t>(INT32_MAX) + 1), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(set_shots(job, 100), QDMI_SUCCESS);
  PASQAL_QDMI_device_job_free(job);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Job, MockLifecycleReturnsHistogram) {
  auto *session = mock_session();
  PASQAL_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(PASQAL_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_wait(job, 0), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_ERROR_BADSTATE); // no program yet
  const auto format = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(job, format, 1, nullptr, nullptr),
            QDMI_ERROR_NOTSUPPORTED);
  const std::string program =
      R"({"version": "1", "register": [{"name": "q0", "x": 0, "y": 0},
                                       {"name": "q1", "x": 5, "y": 0}]})";
  const size_t program_size = program.size() + 1;
  const void *program_data = program.c_str();
  ASSERT_EQ(PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_CUSTOM1, 1,
                                               &program_size, &program_data),
            QDMI_SUCCESS);
  ASSERT_EQ(set_shots(job, 50), QDMI_SUCCESS);
  size_t size = 0;
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &size),
            QDMI_ERROR_BADSTATE); // not finished
  ASSERT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_ERROR_BADSTATE);
  ASSERT_EQ(PASQAL_QDMI_device_job_wait(job, 0), QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_wait(job, 1), QDMI_SUCCESS);
  ASSERT_EQ(PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &size),
            QDMI_SUCCESS);
  std::string keys(size, '\0');
  ASSERT_EQ(PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_HIST_KEYS, size, keys.data(),
                                               nullptr),
            QDMI_SUCCESS);
  EXPECT_STREQ(keys.c_str(), "00");
  size_t count = 0;
  ASSERT_EQ(PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_HIST_VALUES, sizeof(count),
                                               &count, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(count, 50);
  EXPECT_EQ(PASQAL_QDMI_device_job_cancel(job), QDMI_ERROR_INVALIDARGUMENT);
  PASQAL_QDMI_device_job_free(job);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Specs, MockSpecsMapTrapsZoneAndChannels) {
  const auto specs =
      pasqal::DeviceSpecs::from_pulser("MOCK", QDMI_DEVICE_STATUS_IDLE, pasqal::MOCK_SPECS);
  EXPECT_EQ(specs.qubits_num, 4);
  EXPECT_EQ(specs.min_atom_distance, 5000U);
  ASSERT_EQ(specs.sites.size(), 5);
  EXPECT_EQ(specs.sites[0].position, (std::pair<std::int64_t, std::int64_t>{-2500, -2500}));
  EXPECT_TRUE(specs.sites[4].is_zone);
  EXPECT_EQ(specs.sites[4].position, (std::pair<std::int64_t, std::int64_t>{-10000, -10000}));
  EXPECT_EQ(specs.sites[4].zone_extent, 20000U);
  EXPECT_EQ(specs.operations.size(), 2);
}

TEST(Specs, SelectsTheNamedDeviceElseTheFirst) {
  const std::string entries =
      R"([{"device_type": "FRESNEL_CAN1", "specs": "can1"}, {"device_type": "FRESNEL", "specs": "fresnel"}])";
  EXPECT_EQ(pasqal::select_device_specs(entries, "fresnel"), "fresnel");
  EXPECT_EQ(pasqal::select_device_specs(entries, "EMU_FREE"), "can1");
  EXPECT_TRUE(pasqal::DeviceSpecs::offline("EMU").pulser_specs.empty());
}

TEST(Session, MockHasNoPulserSpecs) {
  auto *session = mock_session();
  size_t size = 0;
  EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                session, PASQAL_QDMI_DEVICE_PROPERTY_PULSERSPECS, 0, nullptr, &size),
            QDMI_ERROR_NOTSUPPORTED);
  PASQAL_QDMI_device_session_free(session);
}

TEST(Specs, DeviceWithoutLayoutHasIndexOnlySites) {
  const auto specs = pasqal::DeviceSpecs::from_pulser(
      "EMU", QDMI_DEVICE_STATUS_IDLE,
      R"({"max_atom_num": 3, "min_atom_distance": 4.5, "max_radial_distance": null,
          "channels": [{"id": "raman_local", "addressing": "Local"}]})");
  EXPECT_EQ(specs.min_atom_distance, 4500U);
  ASSERT_EQ(specs.sites.size(), 3);
  for (const auto &site : specs.sites) {
    EXPECT_FALSE(site.position.has_value());
    EXPECT_FALSE(site.is_zone);
  }
}

TEST(Specs, ZoneIsNotCountedAsQubit) {
  const auto specs = pasqal::DeviceSpecs::from_pulser(
      "EMU", QDMI_DEVICE_STATUS_IDLE,
      R"({"max_atom_num": null, "min_atom_distance": 4, "max_radial_distance": 20,
          "channels": [{"id": "rydberg_global", "addressing": "Global"}]})");
  EXPECT_EQ(specs.qubits_num, 0);
  ASSERT_EQ(specs.sites.size(), 1);
  EXPECT_TRUE(specs.sites[0].is_zone);
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
  EXPECT_EQ(status_of(pasqal::CloudError(401, "")), QDMI_ERROR_PERMISSIONDENIED);
  EXPECT_EQ(status_of(pasqal::CloudError(403, "")), QDMI_ERROR_PERMISSIONDENIED);
  EXPECT_EQ(status_of(pasqal::CloudError(503, "")), QDMI_ERROR_FATAL);
  EXPECT_EQ(status_of(pasqal::CloudError(0, "connection refused")), QDMI_ERROR_FATAL);
  EXPECT_EQ(status_of(pasqal::AuthError("rejected")), QDMI_ERROR_PERMISSIONDENIED);
  EXPECT_EQ(status_of(pasqal::TimeoutError("expired")), QDMI_ERROR_TIMEOUT);
}

TEST(Cloud, TokenUsability) {
  EXPECT_FALSE(pasqal::is_token_usable("", 0));
  EXPECT_TRUE(pasqal::is_token_usable("opaque-token", 0));
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOjF9.s", 100)); // exp 1
  EXPECT_TRUE(pasqal::is_token_usable("h.eyJleHAiOjk5OTk5OTk5OTl9.s", 100));
  EXPECT_TRUE(pasqal::is_token_usable("h.eyJleHAiOiI5OTk5OTk5OTk5In0.s", 100)); // string exp
  EXPECT_FALSE(pasqal::is_token_usable("h.eyJleHAiOjk5OTk5OTk5OTl9.s", 9999999995)); // grace
  EXPECT_TRUE(pasqal::is_token_usable("h.eyJzdWIiOiJ4In0.s", 100));                   // no exp
  EXPECT_FALSE(pasqal::is_token_usable("h.!!!.s", 100));
}

TEST(Cloud, DeviceTypesAreCaseInsensitive) {
  EXPECT_TRUE(pasqal::is_device_type("emu_free"));
  EXPECT_TRUE(pasqal::is_device_type("FRESNEL_CAN1"));
  EXPECT_FALSE(pasqal::is_device_type("MOCK"));
}

TEST(Cloud, ExpiredDeadlineStopsStatusRequest) {
  pasqal::CloudConfig config;
  config.base_url = "http://127.0.0.1:1";
  config.token = "token";
  pasqal::CloudClient cloud(config);
  EXPECT_THROW(cloud.job_state("job", std::chrono::steady_clock::now() - std::chrono::seconds(1)),
               pasqal::TimeoutError);
}

TEST(Cloud, ExpiredDeadlineStopsTokenRefresh) {
  pasqal::CloudConfig config;
  config.auth_endpoint = "http://127.0.0.1:1";
  config.username = "user";
  config.password = "password";
  pasqal::CloudClient cloud(config);
  EXPECT_THROW(cloud.job_state("job", std::chrono::steady_clock::now() - std::chrono::seconds(1)),
               pasqal::TimeoutError);
}

TEST_F(ProgramListTest, CopiesAndReplacesProgramsAtomically) {
  size_t count = 0;
  EXPECT_EQ(PASQAL_QDMI_device_job_query_property(job, QDMI_DEVICE_JOB_PROPERTY_PROGRAMSNUM,
                                                  sizeof(count), &count, nullptr), QDMI_ERROR_BADSTATE);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, 0, 0, nullptr, nullptr), QDMI_ERROR_BADSTATE);
  ASSERT_EQ(set_programs(job, {ONE_ATOM, TWO_ATOMS}), QDMI_SUCCESS);
  EXPECT_EQ(source(0), ONE_ATOM);
  EXPECT_EQ(source(1), TWO_ATOMS);
  EXPECT_EQ(set_programs(job, {TWO_ATOMS, "not JSON"}), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(source(0), ONE_ATOM);
  EXPECT_EQ(source(1), TWO_ATOMS);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_CUSTOM1, 3,
                                                nullptr, nullptr), QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_query_property(job, QDMI_DEVICE_JOB_PROPERTY_PROGRAMSNUM,
                                                  sizeof(count), &count, nullptr), QDMI_SUCCESS);
  EXPECT_EQ(count, 2U);
  ASSERT_EQ(set_programs(job, {TWO_ATOMS}), QDMI_SUCCESS);
  EXPECT_EQ(source(0), TWO_ATOMS);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program(job, 1, 0, nullptr, nullptr), QDMI_ERROR_OUTOFRANGE);
}

TEST_F(ProgramListTest, RejectsInvalidTextAndRemovedParameters) {
  const void *data = ONE_ATOM.c_str();
  size_t size = ONE_ATOM.size();
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_CUSTOM1, 1,
                                                &size, &data), QDMI_ERROR_INVALIDARGUMENT);
  const std::string trailing = ONE_ATOM + std::string("\0extra", 6);
  data = trailing.c_str();
  size = trailing.size() + 1;
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_CUSTOM1, 1,
                                                &size, &data), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(job, QDMI_PROGRAM_FORMAT_CUSTOM1, 0,
                                                nullptr, nullptr), QDMI_ERROR_INVALIDARGUMENT);
  for (const int reserved : {0, 1}) {
    EXPECT_EQ(PASQAL_QDMI_device_job_set_parameter(
                  job, static_cast<QDMI_Device_Job_Parameter>(reserved), 0, nullptr), QDMI_ERROR_NOTSUPPORTED);
  }
  for (const int reserved : {6, 9}) {
    EXPECT_EQ(PASQAL_QDMI_device_job_set_programs(
                  job, static_cast<QDMI_Program_Format>(reserved), 1, nullptr, nullptr), QDMI_ERROR_NOTSUPPORTED);
  }
  for (const int reserved : {8, 9}) {
    EXPECT_EQ(PASQAL_QDMI_device_session_query_device_property(
                  session, static_cast<QDMI_Device_Property>(reserved), 0, nullptr, nullptr), QDMI_ERROR_NOTSUPPORTED);
  }
}

TEST_F(ProgramListTest, ReturnsIndexedResultsAndStatuses) {
  ASSERT_EQ(set_programs(job, {ONE_ATOM, TWO_ATOMS}), QDMI_SUCCESS);
  ASSERT_EQ(set_shots(job, 7), QDMI_SUCCESS);
  ASSERT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_SUCCESS);
  for (size_t index = 0; index < 2; ++index) {
    QDMI_Job_Status status{};
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, index, &status), QDMI_SUCCESS);
    EXPECT_EQ(status, QDMI_JOB_STATUS_DONE);
    char keys[3]{};
    EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, index, QDMI_JOB_RESULT_HIST_KEYS,
                                                 sizeof(keys), keys, nullptr), QDMI_SUCCESS);
    EXPECT_EQ(std::string(keys), std::string(index + 1, '0'));
    size_t count = 0;
    EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, index, QDMI_JOB_RESULT_HIST_VALUES,
                                                 sizeof(count), &count, nullptr), QDMI_SUCCESS);
    EXPECT_EQ(count, 7U);
  }
  QDMI_Job_Status status{};
  EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, 2, &status), QDMI_ERROR_OUTOFRANGE);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, 2, QDMI_JOB_RESULT_HIST_KEYS,
                                               0, nullptr, nullptr), QDMI_ERROR_OUTOFRANGE);
  EXPECT_EQ(set_programs(job, {ONE_ATOM}), QDMI_ERROR_BADSTATE);
}

TEST_F(ProgramListTest, KeepsSuccessfulResultsAfterSubmissionFailure) {
  ASSERT_EQ(set_programs(job, {ONE_ATOM, "{}", TWO_ATOMS}), QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_ERROR_FATAL);
  QDMI_Job_Status status{};
  ASSERT_EQ(PASQAL_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_FAILED);
  const std::vector expected{QDMI_JOB_STATUS_DONE, QDMI_JOB_STATUS_FAILED, QDMI_JOB_STATUS_CANCELED};
  for (size_t index = 0; index < expected.size(); ++index) {
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, index, &status), QDMI_SUCCESS);
    EXPECT_EQ(status, expected[index]);
  }
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, 0, QDMI_JOB_RESULT_HIST_KEYS,
                                               0, nullptr, nullptr), QDMI_SUCCESS);
  EXPECT_EQ(PASQAL_QDMI_device_job_get_results(job, 1, QDMI_JOB_RESULT_HIST_KEYS,
                                               0, nullptr, nullptr), QDMI_ERROR_BADSTATE);
}

TEST_F(ProgramListTest, CancelsAllUnsubmittedPrograms) {
  ASSERT_EQ(set_programs(job, {ONE_ATOM, TWO_ATOMS}), QDMI_SUCCESS);
  ASSERT_EQ(PASQAL_QDMI_device_job_cancel(job), QDMI_SUCCESS);
  QDMI_Job_Status status{};
  for (size_t index = 0; index < 2; ++index) {
    EXPECT_EQ(PASQAL_QDMI_device_job_get_program_status(job, index, &status), QDMI_SUCCESS);
    EXPECT_EQ(status, QDMI_JOB_STATUS_CANCELED);
  }
  EXPECT_EQ(PASQAL_QDMI_device_job_submit(job), QDMI_ERROR_BADSTATE);
}
