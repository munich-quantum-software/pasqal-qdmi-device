# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.24)
get_filename_component(source "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(work "${source}/build/install-check")
function(run)
  execute_process(COMMAND ${ARGV} COMMAND_ERROR_IS_FATAL ANY)
endfunction()
string(
  RANDOM
  LENGTH 10
  ALPHABET 0123456789abcdef suffix)
set(prefix "${work}/prefix-${suffix}")
set(relocated "${work}/relocated-${suffix}")
run("${CMAKE_COMMAND}"
    -S
    "${source}"
    -B
    "${work}/producer"
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_PASQAL_QDMI_TESTS=OFF
    -DINSTALL_PASQAL_QDMI_DEVICE=ON)
run("${CMAKE_COMMAND}"
    --build
    "${work}/producer"
    --config
    Release
    --parallel
    2)
foreach(component Runtime Development)
  run("${CMAKE_COMMAND}"
      --install
      "${work}/producer"
      --config
      Release
      --prefix
      "${prefix}"
      --component
      "pasqal-qdmi-device_${component}")
endforeach()
file(RENAME "${prefix}" "${relocated}")
run("${CMAKE_COMMAND}"
    -S
    "${source}/test/install-consumer"
    -B
    "${work}/consumer-${suffix}"
    -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_PREFIX_PATH=${relocated}")
run("${CMAKE_COMMAND}"
    --build
    "${work}/consumer-${suffix}"
    --config
    Release
    --parallel
    2)
run("${CMAKE_CTEST_COMMAND}" --test-dir "${work}/consumer-${suffix}" -C Release --output-on-failure)
