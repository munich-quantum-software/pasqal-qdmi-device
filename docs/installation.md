# Installation

The Python package includes the native library and its device catalogue. It
requires Python 3.11 or later; MQT Core and Pulser are optional. The full Pulser
distribution includes the official Pasqal SDK and local simulation support.
Install from a source checkout with the integrations you need:

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

| Extra    | Purpose                                                                                 |
| -------- | --------------------------------------------------------------------------------------- |
| `core`   | Shared QDMI Python API through MQT Core >=4.1.1                                         |
| `pulser` | Full Pulser >=1.9.1 distribution, including the official cloud SDK and local simulation |

The base package has no Python runtime dependencies. Native C/C++ applications
can use the installed QDMI library without Core or the Pasqal Python SDK.

Wheel builds target Linux x86_64/ARM64 with manylinux_2_28 (glibc >=2.28), macOS
ARM64 >=13.3, and Windows x86_64. Native builds and wheels link CPR, its bundled
curl, and OpenSSL 3.5.9 statically on every platform. Certificate verification
uses the host's CA bundle on Linux and the native trust stores on macOS and
Windows.

## Native CMake installation

A source build needs a C++20 compiler, CMake >=3.24, Perl, and Make (NMake with
MSVC on Windows). Windows x64 builds also need NASM. CMake builds pinned OpenSSL
and CPR/curl sources; no system TLS or curl development packages are needed.
QDMI, JSON and test dependencies are fetched when installed versions are
unavailable.

Build one x86_64 or ARM64 architecture at a time; Linux builds use a native
toolchain. When embedding the project with `add_subdirectory`, configure it
before other dependencies create `OpenSSL::SSL` or `OpenSSL::Crypto` targets.

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

The wheel carries the native library and manifest under `pasqal/qdmi/data/lib`
or `lib64` (`bin` on Windows), with headers and CMake metadata elsewhere under
`pasqal/qdmi/data`. `pasqal.qdmi` exposes their paths; Python applications use
the optional `mqt.core.qdmi` API.
