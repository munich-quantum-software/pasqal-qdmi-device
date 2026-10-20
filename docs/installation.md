# Installation

The package is prepared in a private repository and is not advertised as publicly available on PyPI. Install a locally built or approved CI wheel. Python 3.11 or later and MQT Core 4.1.1 or later are required.

```sh
uv build
uv venv build/consumer
uv pip install --python build/consumer/bin/python dist/pasqal_qdmi-*.whl
build/consumer/bin/python -c "from mqt.core.qdmi import builtin_driver; print(builtin_driver.registered_device_ids())"
```

Use `build/consumer/Scripts/python.exe` on Windows. The installed IDs are `pasqal.default` and `pasqal.emu-free`; both select EMU_FREE. Listing IDs reads the installed manifest without authenticating or submitting work. Install `pulser-core==1.9.0` to construct sequences; the optional `cloud` extra also includes the official `pasqal-cloud==0.24.0` SDK for comparison.

## Native CMake installation

A source build needs a C++20 compiler, CMake >=3.24, a build tool and TLS development headers. On Linux with OpenSSL this typically includes `libssl-dev`. CMake fetches pinned released QDMI, CPR/curl, JSON and test dependencies when installed versions are unavailable.

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --parallel 2
ctest --test-dir build/native -C Release --output-on-failure
cmake --install build/native --config Release --prefix "$PWD/build/install"
```

An installed consumer uses `find_package(pasqal-qdmi-device 0.2 REQUIRED CONFIG)` and links target `pasqal-qdmi-device`. Set `CMAKE_PREFIX_PATH` to the installed prefix. Headers include `pasqal_qdmi/device.h` and `pasqal-qdmi-device/constants.h`. The manifest sits beside the shared library and uses a relative library filename. The relocation check is `cmake -P test/install.cmake`.

The wheel carries the same native library, headers, manifest and CMake package under `pasqal/qdmi/data`. `pasqal.qdmi` exposes their paths; all runtime operations use the existing `mqt.core.qdmi` API. No separate production ctypes bindings are installed.
