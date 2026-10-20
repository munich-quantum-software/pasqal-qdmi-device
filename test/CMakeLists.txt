# ------------------------------------------------------------------------------
# Non-Functional Tests
# ------------------------------------------------------------------------------

generate_device_defs_executable(${QDMI_PREFIX} TARGET ${QDMI_TARGET_NAME})

# ------------------------------------------------------------------------------
# Functional Tests
# ------------------------------------------------------------------------------

set(QDMI_TEST_TARGET_NAME "${QDMI_TARGET_NAME}-test")

# test_pasqal_device.cpp is the upstream QDMI device template test, generated with the PASQAL
# prefix. It initializes a session without parameters, so the backend is selected through
# PASQAL_QDMI_BACKEND.
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
  TEST_SUFFIX .mock
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  PROPERTIES ENVIRONMENT "PASQAL_QDMI_BACKEND=MOCK" DISCOVERY_TIMEOUT 60)

if(BUILD_PASQAL_QDMI_LIVE_TESTS)
  gtest_discover_tests(
    ${QDMI_TEST_TARGET_NAME}
    TEST_SUFFIX .live
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES ENVIRONMENT "PASQAL_QDMI_BACKEND=EMU_FREE" LABELS live DISCOVERY_TIMEOUT 60)
endif()

if(NOT USE_INSTALLED_PASQAL_QDMI_DEVICE)
  add_executable(${QDMI_TARGET_NAME}-unit-test unit/test_pasqal_cloud.cpp unit/test_pasqal_unit.cpp)
  target_link_libraries(
    ${QDMI_TARGET_NAME}-unit-test PRIVATE GTest::gtest_main pasqal::qdmi_device_internals
                                          qdmi::qdmi_project_warnings)
  target_compile_features(${QDMI_TARGET_NAME}-unit-test PRIVATE cxx_std_20)
  gtest_discover_tests(${QDMI_TARGET_NAME}-unit-test DISCOVERY_TIMEOUT 60)
endif()

find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(Python3_Interpreter_FOUND)
  foreach(suite pasqal_integration transport_http)
    add_test(
      NAME ${suite}
      COMMAND
        ${CMAKE_COMMAND} -E env "PASQAL_QDMI_LIBRARY=$<TARGET_FILE:${QDMI_TARGET_NAME}>"
        "PASQAL_AUTHFILE=/nonexistent" "PYTHONDONTWRITEBYTECODE=1" ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_${suite}.py)
  endforeach()
endif()
