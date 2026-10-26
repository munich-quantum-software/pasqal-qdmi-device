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

The Pulser backend, `pasqal.qdmi.pulser`, needs both extras. The base package
has no Python runtime dependencies. Native C/C++ applications can use the
installed QDMI library without Core or the Pasqal Python SDK.

Wheel builds target Linux x86_64/ARM64 with manylinux_2_28 (glibc >=2.28), macOS
ARM64 >=13.3, and Windows x86_64. CPR is linked statically on every platform.
Linux and Windows also link curl statically, using installed OpenSSL 3 on Linux
and Schannel on Windows. macOS uses Apple's system curl. Certificate
verification uses the host's CA bundle on Linux and the native trust stores on
macOS and Windows. The curl build detects installed zlib, Brotli, Zstd and
HTTP/2 libraries automatically. Only supported response encodings are advertised
to servers.

Native and source installations receive OpenSSL fixes through the system package
manager. Portable Linux wheels bundle OpenSSL and other non-platform shared
libraries; those copies receive security updates through new device releases.
macOS and Windows use operating-system TLS in both native builds and wheels.

## Native CMake installation

A source build needs a C++20 compiler, CMake >=3.24, and OpenSSL >=3.0
development files on Linux. For example:

```sh
# Debian / Ubuntu
sudo apt-get install libssl-dev zlib1g-dev
# Fedora / RHEL 9 or later
sudo dnf install openssl-devel zlib-devel
```

macOS and Windows need only the C++ build tools and CMake; TLS uses the
operating system. Compression libraries are optional. CMake builds pinned CPR
and curl sources, using Apple's curl on macOS, and does not build OpenSSL or
compression libraries. QDMI, JSON and test dependencies are fetched when
installed versions are unavailable.

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
