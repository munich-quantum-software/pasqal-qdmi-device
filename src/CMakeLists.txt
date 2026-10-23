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

include(GNUInstallDirs)

generate_prefixed_qdmi_headers(${QDMI_PREFIX})

file(GLOB_RECURSE PUBLIC_HEADER_FILES "${CMAKE_CURRENT_BINARY_DIR}/include/*.h")
list(APPEND PUBLIC_HEADER_FILES "${PROJECT_SOURCE_DIR}/include/pasqal-qdmi-device/constants.h")

add_subdirectory(internal)

add_library(${QDMI_TARGET_NAME} SHARED)
set_target_properties(
  ${QDMI_TARGET_NAME} PROPERTIES VERSION ${PROJECT_VERSION}
                                 SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
target_link_libraries(${QDMI_TARGET_NAME} PRIVATE pasqal::qdmi_device_internals)
configure_qdmi_exports(TARGET ${QDMI_TARGET_NAME} INTERFACE device PREFIX ${QDMI_PREFIX})
configure_qdmi_device_target(TARGET ${QDMI_TARGET_NAME} ID ${PASQAL_QDMI_DEVICE_ID} PREFIX
                             ${QDMI_PREFIX})

target_sources(
  ${QDMI_TARGET_NAME}
  PUBLIC FILE_SET
         public_headers
         TYPE
         HEADERS
         BASE_DIRS
         ${CMAKE_CURRENT_BINARY_DIR}/include
         ${PROJECT_SOURCE_DIR}/include
         FILES
         ${PUBLIC_HEADER_FILES})

# Keep catalogue paths relative so installed prefixes and wheels are relocatable.
set(PASQAL_QDMI_CATALOG_NAME "${QDMI_TARGET_NAME}.qdmi.json")
set(PASQAL_QDMI_LIBRARY_FILE "$<TARGET_FILE_NAME:${QDMI_TARGET_NAME}>")
configure_file("${PROJECT_SOURCE_DIR}/cmake/${QDMI_TARGET_NAME}.qdmi.json.in"
               "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}.qdmi.json.gen.in" @ONLY)
set(PASQAL_QDMI_CATALOG "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${PASQAL_QDMI_CATALOG_NAME}")
file(
  GENERATE
  OUTPUT "${PASQAL_QDMI_CATALOG}"
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}.qdmi.json.gen.in")
add_custom_command(
  TARGET ${QDMI_TARGET_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PASQAL_QDMI_CATALOG}"
          "$<TARGET_FILE_DIR:${QDMI_TARGET_NAME}>/${PASQAL_QDMI_CATALOG_NAME}")
set_target_properties(${QDMI_TARGET_NAME} PROPERTIES QDMI_MANIFEST_NAME ${PASQAL_QDMI_CATALOG_NAME})
set_property(
  TARGET ${QDMI_TARGET_NAME}
  APPEND
  PROPERTY EXPORT_PROPERTIES QDMI_MANIFEST_NAME)

set(${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_DATADIR}/cmake/${QDMI_TARGET_NAME}"
    CACHE INTERNAL "Config install directory")

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${QDMI_TARGET_NAME}-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
  INSTALL_DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
  NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)
write_basic_package_version_file(
  "${QDMI_TARGET_NAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion)

if(WIN32)
  set(PASQAL_QDMI_CATALOG_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
else()
  set(PASQAL_QDMI_CATALOG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}")
endif()

if(INSTALL_PASQAL_QDMI_DEVICE)
  # Copy notices from the dependencies actually bundled into this build.
  set(LICENSE_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/licenses/${QDMI_TARGET_NAME}")
  if(SKBUILD)
    set(LICENSE_INSTALL_DIR "${SKBUILD_METADATA_DIR}/licenses")
  endif()
  install(
    FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${LICENSE_INSTALL_DIR}"
    COMPONENT ${QDMI_TARGET_NAME}_Runtime)
  set(LICENSE_DEPENDENCIES cpr nlohmann_json curl)
  set(LICENSE_FILENAMES LICENSE LICENSE.MIT COPYING)
  foreach(DEPENDENCY FILENAME IN ZIP_LISTS LICENSE_DEPENDENCIES LICENSE_FILENAMES)
    FetchContent_GetProperties(${DEPENDENCY})
    if(${DEPENDENCY}_SOURCE_DIR)
      install(
        FILES "${${DEPENDENCY}_SOURCE_DIR}/${FILENAME}"
        DESTINATION "${LICENSE_INSTALL_DIR}/${DEPENDENCY}"
        COMPONENT ${QDMI_TARGET_NAME}_Runtime)
    endif()
  endforeach()

  # Wheel repair bundles these system libraries; retain their package notices.
  if(SKBUILD AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(DEPENDENCY ssl brotli zstd nghttp2)
      file(
        GLOB NOTICES
        LIST_DIRECTORIES FALSE
        "/usr/share/licenses/*${DEPENDENCY}*/LICENSE*"
        "/usr/share/licenses/*${DEPENDENCY}*/COPYING*" "/usr/share/doc/*${DEPENDENCY}*/copyright")
      foreach(NOTICE IN LISTS NOTICES)
        get_filename_component(PACKAGE_DIRECTORY "${NOTICE}" DIRECTORY)
        get_filename_component(PACKAGE_NAME "${PACKAGE_DIRECTORY}" NAME)
        install(
          FILES "${NOTICE}"
          DESTINATION "${LICENSE_INSTALL_DIR}/system/${PACKAGE_NAME}"
          COMPONENT ${QDMI_TARGET_NAME}_Runtime)
      endforeach()
    endforeach()
  endif()

  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/${QDMI_TARGET_NAME}-config-version.cmake"
    DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
    COMPONENT ${QDMI_TARGET_NAME}_Development)

  # cmake-format: off
  install(
    TARGETS ${QDMI_TARGET_NAME}
    EXPORT ${QDMI_TARGET_NAME}-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Runtime
            NAMELINK_COMPONENT ${QDMI_TARGET_NAME}_Development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Development
    FILE_SET public_headers
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            COMPONENT ${QDMI_TARGET_NAME}_Development)
  # cmake-format: on

  install(
    FILES "${PASQAL_QDMI_CATALOG}"
    DESTINATION ${PASQAL_QDMI_CATALOG_INSTALL_DIR}
    COMPONENT ${QDMI_TARGET_NAME}_Runtime)

  install(
    EXPORT ${QDMI_TARGET_NAME}-targets
    FILE ${QDMI_TARGET_NAME}-targets.cmake
    DESTINATION ${${QDMI_PREFIX}_QDMI_CONFIG_INSTALL_DIR}
    COMPONENT ${QDMI_TARGET_NAME}_Development)
endif()
