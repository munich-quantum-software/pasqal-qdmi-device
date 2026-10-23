![Python](https://img.shields.io/badge/python-3.11%2B-blue?style=flat-square&logo=python)
![OS](https://img.shields.io/badge/os-linux%20%7C%20macos%20%7C%20windows-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-Apache--2.0%20WITH%20LLVM--exception-blue.svg?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/badge/CI-GitHub_Actions-blue?style=flat-square&logo=github)](https://github.com/munich-quantum-software/pasqal-qdmi-device/actions/workflows/ci.yml)

# Pasqal QDMI Device

An implementation of the
[Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI)
for Pasqal neutral-atom quantum processors and emulators.

The library exposes Pasqal Cloud through the vendor-neutral QDMI C interface. It
submits [Pulser](https://pulser.readthedocs.io/) pulse sequences, queries device
capabilities and queue information, and retrieves measurement results. Python
applications can use the shared [MQT Core](https://mqt.readthedocs.io/) QDMI
API.

<!-- rumdl-disable MD033 -->
<p align="center">
  <a href="https://www.pasqal.com/">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="docs/_static/logo-pasqal-dark.svg">
      <img src="docs/_static/logo-pasqal-light.svg" width="260" alt="Pasqal">
    </picture>
  </a>
</p>
<!-- rumdl-enable MD033 -->

## Key Features

- A stable generic device, `pasqal.default`, for runtime configuration.
- An installed catalogue of stable IDs for Pasqal Cloud systems.
- Native Pulser programs and ordered batches through QDMI 1.4.
- Device metadata, queue length, indexed counts, job retrieval and cancellation.
- Password-realm authentication with automatic token refresh.
- A native C++ library and optional Python integration through MQT Core.

The provider accepts built, measured Pulser sequences. It does not compile gate
circuits into pulses. EMU_FREE is validated end to end; access to other
emulators and quantum processors depends on your Pasqal account and region.

## Getting Started

Build and install the Python package with the optional Core and Pulser
integrations:

```console
uv pip install '.[core,pulser]'
```

Building from source requires a C++20 compiler, CMake 3.24 or newer, Perl and
Make (NMake in a Developer Command Prompt on Windows; Windows x64 also needs
NASM). CMake builds OpenSSL and curl from source. A native installation does not
require Python:

```console
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_PASQAL_QDMI_TESTS=OFF
cmake --build build --config Release
cmake --install build --prefix /path/to/install
```

See [installation](docs/installation.md) for wheel installation and platform
support.

## Where to Start

| I want to...                                          | Read...                                     |
| ----------------------------------------------------- | ------------------------------------------- |
| install the package or build from source              | [Installation](docs/installation.md)        |
| configure my account and project                      | [Authentication](docs/authentication.md)    |
| choose a cloud system and understand its capabilities | [Device capabilities](docs/capabilities.md) |
| submit Pulser sequences and retrieve results          | [Usage](docs/usage.md)                      |
| diagnose authentication or job failures               | [Troubleshooting](docs/troubleshooting.md)  |
| develop and test the provider                         | [Development](docs/development.md)          |

## Contributors and Support

The Pasqal QDMI Device is developed by [MQSC](https://mq.sc) and
[Pasqal](https://www.pasqal.com/).

Please use
[GitHub Issues](https://github.com/munich-quantum-software/pasqal-qdmi-device/issues)
for bug reports and feature requests.

## License

The project is licensed under the Apache License 2.0 with LLVM exceptions; see
[LICENSE](LICENSE).
