# Installation

The Python package includes the native library and its device catalogue. It
requires Python 3.11 or later; MQT Core, Pulser and the official Pasqal SDK are
optional. Install from a source checkout with the integrations you need:

```console
uv pip install '.[core,pulser]'
```

To build a wheel for another environment:

```sh
uv build
uv venv build/consumer
uv pip install --python build/consumer/bin/python dist/pasqal_qdmi-*.whl 'mqt-core>=4.1.1'
build/consumer/bin/python -c "from mqt.core.qdmi import builtin_driver; print(builtin_driver.registered_device_ids())"
```

Use `build/consumer/Scripts/python.exe` on Windows. Listing installed IDs reads
the manifest without authenticating or loading the provider. See the
[device catalogue](capabilities.md#device-catalogue) for the available systems.

| Extra    | Purpose                                                                           |
| -------- | --------------------------------------------------------------------------------- |
| `core`   | Shared QDMI Python API through MQT Core >=4.1.1                                   |
| `pulser` | Build sequences with Pulser >=1.9                                                 |
| `cloud`  | Official Pasqal Cloud SDK >=0.24 (also installs its compatible Pulser dependency) |

The base package has no Python runtime dependencies. Native C/C++ applications
can use the installed QDMI library without Core or the Pasqal Python SDK.

Wheel builds target Linux x86_64/ARM64 with glibc >=2.34, macOS ARM64 >=13.3,
and Windows x86_64. Linux x86_64 wheels require an x86-64-v2 CPU because the
build image's OpenSSL package may use that baseline. The Linux image supplies
OpenSSL 3 for the bundled curl; macOS wheels use Apple's system curl and TLS
trust store.

## Native CMake installation

A source build needs a C++20 compiler, CMake >=3.24, a build tool and TLS
development headers. Bundled curl requires OpenSSL >=3 on Linux, typically
provided by `libssl-dev`. CMake fetches pinned released QDMI, CPR/curl, JSON and
test dependencies when installed versions are unavailable.

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --parallel 2
ctest --test-dir build/native -C Release --output-on-failure
cmake --install build/native --config Release --prefix "$PWD/build/install"
```

An installed consumer uses
`find_package(pasqal-qdmi-device 1.0 REQUIRED CONFIG)` and links target
`pasqal-qdmi-device`. Set `CMAKE_PREFIX_PATH` to the installed prefix. Headers
include `pasqal_qdmi/device.h` and `pasqal-qdmi-device/constants.h`. The
manifest sits beside the shared library and uses a relative library filename.
The relocation check is `cmake -P test/install.cmake`.

The wheel carries the same native library, headers, manifest and CMake package
under `pasqal/qdmi/data`. `pasqal.qdmi` exposes their paths; Python applications
use the optional `mqt.core.qdmi` API.
