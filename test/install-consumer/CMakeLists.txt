# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.24)
project(pasqal-installed-consumer LANGUAGES C)
find_package(pasqal-qdmi-device 0.2 REQUIRED CONFIG)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE pasqal-qdmi-device)
enable_testing()
add_test(NAME installed-consumer COMMAND consumer)
if(WIN32)
  add_custom_command(
    TARGET consumer
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:pasqal-qdmi-device>
            $<TARGET_FILE_DIR:consumer>)
endif()
