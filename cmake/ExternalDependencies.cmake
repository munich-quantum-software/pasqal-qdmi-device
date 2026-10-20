include(FetchContent)
set(FETCH_PACKAGES "")

if(TARGET qdmi::qdmi)
  message(STATUS "QDMI is already available.")
else()
  # cmake-format: off
  set(QDMI_VERSION 1.4.0
          CACHE STRING "QDMI version")
  set(QDMI_REV "6db03d83ad602aaa9fe928b5c3fc5cce84b10061" # v1.4.0
          CACHE STRING "QDMI identifier (tag, branch or commit hash)")
  set(QDMI_REPO_OWNER "Munich-Quantum-Software-Stack"
          CACHE STRING "QDMI repository owner (change when using a fork)")
  set(INSTALL_QDMI
      OFF
      CACHE BOOL "Generate installation instructions for QDMI")
  # cmake-format: on
  FetchContent_Declare(
    qdmi
    GIT_REPOSITORY https://github.com/${QDMI_REPO_OWNER}/qdmi.git
    GIT_TAG ${QDMI_REV}
    FIND_PACKAGE_ARGS ${QDMI_VERSION})
  list(APPEND FETCH_PACKAGES qdmi)
endif()

if(NOT USE_INSTALLED_PASQAL_QDMI_DEVICE)
  set(JSON_VERSION
      3.12.0
      CACHE STRING "nlohmann_json version")
  set(JSON_URL
      https://github.com/nlohmann/json/releases/download/v${JSON_VERSION}/json.tar.xz
  )
  set(JSON_SystemInclude
      ON
      CACHE INTERNAL "Treat the library headers like system headers")
  FetchContent_Declare(nlohmann_json URL ${JSON_URL} FIND_PACKAGE_ARGS
                                         ${JSON_VERSION})
  list(APPEND FETCH_PACKAGES nlohmann_json)

  # cpr builds its own static libcurl against the system TLS library by default.
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CPR_BUILD_TESTS
      OFF
      CACHE BOOL "Disable CPR tests" FORCE)
  set(CPR_CURL_USE_LIBPSL
      OFF
      CACHE BOOL "Disable libpsl for CPR curl" FORCE)
  set(CPR_USE_SYSTEM_CURL
      OFF
      CACHE BOOL "Use system curl for CPR")
  set(BUILD_STATIC_CURL
      ON
      CACHE BOOL "Enable static curl for CPR" FORCE)
  FetchContent_Declare(
    curl
    URL https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.xz
    URL_HASH SHA256=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5)
  FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.15.1
    FIND_PACKAGE_ARGS 1.15.1)
  list(APPEND FETCH_PACKAGES cpr)
endif()

if(BUILD_PASQAL_QDMI_TESTS)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
  set(INSTALL_GTEST
      OFF
      CACHE BOOL "" FORCE)
  set(GTEST_VERSION
      1.18.0
      CACHE STRING "Google Test version")
  set(GTEST_URL
      https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz
  )
  FetchContent_Declare(googletest URL ${GTEST_URL} FIND_PACKAGE_ARGS
                                      ${GTEST_VERSION} NAMES GTest)
  list(APPEND FETCH_PACKAGES googletest)
endif()

# Build cpr and its bundled curl statically, without changing BUILD_SHARED_LIBS
# for a project that includes this one.
set(BUILD_SHARED_LIBS_PREV ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(${FETCH_PACKAGES})
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_PREV})
