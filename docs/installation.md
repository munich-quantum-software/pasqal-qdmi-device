# Installation

The package is prepared in a private repository and is not advertised as publicly available on PyPI. Install a locally built or approved CI wheel. Python 3.11 or later and MQT Core 4.1.1 or later are required.

```sh
uv build
uv venv build/consumer
uv pip install --python build/consumer/bin/python dist/pasqal_qdmi-*.whl
build/consumer/bin/python -c "from mqt.core.qdmi import builtin_driver; print(builtin_driver.registered_device_ids())"
```

Use `build/consumer/Scripts/python.exe` on Windows. The installed IDs are `pasqal.default` and `pasqal.emu-free`; both select EMU_FREE. Listing IDs reads the installed manifest without authenticating or submitting work. Install `pulser-core==1.9.0` to construct sequences; the optional `cloud` extra also includes the official `pasqal-cloud==0.24.0` SDK for comparison.

Wheel builds target Linux x86_64/ARM64 with glibc >=2.34, macOS ARM64 >=13.3, and Windows x86_64. Linux x86_64 wheels require an x86-64-v2 CPU because the build image's OpenSSL package may use that baseline. The Linux image supplies OpenSSL 3 for the bundled curl; macOS wheels use Apple's system curl and TLS trust store.

## Native CMake installation

A source build needs a C++20 compiler, CMake >=3.24, a build tool and TLS development headers. Bundled curl requires OpenSSL >=3 on Linux, typically provided by `libssl-dev`. CMake fetches pinned released QDMI, CPR/curl, JSON and test dependencies when installed versions are unavailable.

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --parallel 2
ctest --test-dir build/native -C Release --output-on-failure
cmake --install build/native --config Release --prefix "$PWD/build/install"
```

An installed consumer uses `find_package(pasqal-qdmi-device 1.0 REQUIRED CONFIG)` and links target `pasqal-qdmi-device`. Set `CMAKE_PREFIX_PATH` to the installed prefix. Headers include `pasqal_qdmi/device.h` and `pasqal-qdmi-device/constants.h`. The manifest sits beside the shared library and uses a relative library filename. The relocation check is `cmake -P test/install.cmake`.

The wheel carries the same native library, headers, manifest and CMake package under `pasqal/qdmi/data`. `pasqal.qdmi` exposes their paths; all runtime operations use the existing `mqt.core.qdmi` API. No separate production ctypes bindings are installed.
