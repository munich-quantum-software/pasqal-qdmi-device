![Python](https://img.shields.io/badge/python-3.11%2B-blue?style=flat-square&logo=python)
![OS](https://img.shields.io/badge/os-linux%20%7C%20macos%20%7C%20windows-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-Apache--2.0%20WITH%20LLVM--exception-blue.svg?style=flat-square)](https://github.com/munich-quantum-software/pasqal-qdmi-device/blob/main/LICENSE)
[![CI](https://img.shields.io/github/actions/workflow/status/munich-quantum-software/pasqal-qdmi-device/ci.yml?branch=main&style=flat-square&logo=github&label=ci)](https://github.com/munich-quantum-software/pasqal-qdmi-device/actions/workflows/ci.yml)
[![Documentation](https://img.shields.io/readthedocs/pasqal-qdmi-device?logo=readthedocs&style=flat-square)](https://pasqal-qdmi-device.readthedocs.io/en/stable/)

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
      <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/munich-quantum-software/pasqal-qdmi-device/refs/heads/main/docs/_static/logo-pasqal-dark.svg">
      <img src="https://raw.githubusercontent.com/munich-quantum-software/pasqal-qdmi-device/refs/heads/main/docs/_static/logo-pasqal-light.svg" width="260" alt="Pasqal">
    </picture>
  </a>
</p>
<p align="center">
  <a href="https://pasqal-qdmi-device.readthedocs.io/en/stable/">
  <img width="30%" src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=read%20the%20docs" alt="Documentation" />
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

Install the Python package with the optional Core and Pulser integrations:

```console
uv pip install 'pasqal-qdmi[core,pulser]'
```

Building from source requires a C++20 compiler, CMake 3.24 or newer, and OpenSSL
3 development files on Linux. macOS uses Apple's curl and Windows uses Schannel.
CMake builds CPR and, outside macOS, curl; installed compression libraries are
detected automatically. A native installation does not require Python:

```console
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_PASQAL_QDMI_TESTS=OFF
cmake --build build --config Release
cmake --install build --prefix /path/to/install
```

See
[installation](https://pasqal-qdmi-device.readthedocs.io/en/stable/installation.html)
for wheel installation and platform support.

## Where to Start

| I want to...                                          | Read...                                                                                      |
| ----------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| install the package or build from source              | [Installation](https://pasqal-qdmi-device.readthedocs.io/en/stable/installation.html)        |
| configure my account and project                      | [Authentication](https://pasqal-qdmi-device.readthedocs.io/en/stable/authentication.html)    |
| choose a cloud system and understand its capabilities | [Device capabilities](https://pasqal-qdmi-device.readthedocs.io/en/stable/capabilities.html) |
| submit Pulser sequences and retrieve results          | [Usage](https://pasqal-qdmi-device.readthedocs.io/en/stable/usage.html)                      |
| diagnose authentication or job failures               | [Troubleshooting](https://pasqal-qdmi-device.readthedocs.io/en/stable/troubleshooting.html)  |
| develop and test the provider                         | [Development](https://pasqal-qdmi-device.readthedocs.io/en/stable/development.html)          |

## Contributors and Support

The Pasqal QDMI Device is developed by [MQSC](https://mq.sc) and
[Pasqal](https://www.pasqal.com/).

Please use
[GitHub Issues](https://github.com/munich-quantum-software/pasqal-qdmi-device/issues)
for bug reports and feature requests.

## License

The project is licensed under the Apache License 2.0 with LLVM exceptions; see
[LICENSE](https://github.com/munich-quantum-software/pasqal-qdmi-device/blob/main/LICENSE).
