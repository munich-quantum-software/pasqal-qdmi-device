# Copyright (c) 2026 Munich Quantum Software Company GmbH
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

# Copy licenses from the dependencies used by this build, without maintaining duplicate texts.
set(PASQAL_LICENSE_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/licenses/${QDMI_TARGET_NAME}")
if(SKBUILD)
  set(PASQAL_LICENSE_INSTALL_DIR "${SKBUILD_METADATA_DIR}/licenses")
endif()

install(
  FILES "${PROJECT_SOURCE_DIR}/LICENSE"
  DESTINATION "${PASQAL_LICENSE_INSTALL_DIR}"
  COMPONENT ${QDMI_TARGET_NAME}_Runtime)
if(cpr_SOURCE_DIR)
  install(
    FILES "${cpr_SOURCE_DIR}/LICENSE"
    DESTINATION "${PASQAL_LICENSE_INSTALL_DIR}"
    RENAME LICENSE-cpr
    COMPONENT ${QDMI_TARGET_NAME}_Runtime)
endif()
if(nlohmann_json_SOURCE_DIR)
  install(
    FILES "${nlohmann_json_SOURCE_DIR}/LICENSE.MIT"
    DESTINATION "${PASQAL_LICENSE_INSTALL_DIR}"
    RENAME LICENSE-nlohmann-json
    COMPONENT ${QDMI_TARGET_NAME}_Runtime)
endif()

if(NOT CPR_USE_SYSTEM_CURL)
  FetchContent_GetProperties(curl SOURCE_DIR PASQAL_CURL_SOURCE_DIR)
  if(PASQAL_CURL_SOURCE_DIR)
    install(
      FILES "${PASQAL_CURL_SOURCE_DIR}/COPYING"
      DESTINATION "${PASQAL_LICENSE_INSTALL_DIR}"
      RENAME LICENSE-curl
      COMPONENT ${QDMI_TARGET_NAME}_Runtime)
  endif()
  if(CURL_USE_OPENSSL)
    get_filename_component(PASQAL_OPENSSL_LIBRARY_DIR "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
    find_file(
      PASQAL_OPENSSL_LICENSE
      NAMES LICENSE.txt copyright
      HINTS "${OPENSSL_INCLUDE_DIR}/.." "${PASQAL_OPENSSL_LIBRARY_DIR}/.."
      PATHS /usr/share/licenses/openssl-libs /usr/share/doc/libssl-dev
      NO_DEFAULT_PATH)
    if(PASQAL_OPENSSL_LICENSE)
      file(REAL_PATH "${PASQAL_OPENSSL_LICENSE}" PASQAL_OPENSSL_LICENSE_FILE)
      install(
        FILES "${PASQAL_OPENSSL_LICENSE_FILE}"
        DESTINATION "${PASQAL_LICENSE_INSTALL_DIR}"
        RENAME LICENSE-openssl
        COMPONENT ${QDMI_TARGET_NAME}_Runtime)
    elseif(SKBUILD)
      message(FATAL_ERROR "Set PASQAL_OPENSSL_LICENSE to the license of the bundled OpenSSL")
    endif()
  endif()
endif()
