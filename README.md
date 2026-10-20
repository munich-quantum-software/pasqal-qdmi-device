# Pasqal QDMI Device

MQSC-maintained QDMI device for submitting native Pulser programs to Pasqal, a full-stack neutral-atom quantum computing company. MQT Core supplies Python bindings, discovery and job/result handling; Pulser constructs the pulse programs.

Version 1.0 targets released QDMI 1.4.0 and MQT Core 4.1.1 or later. The validated backend is **EMU_FREE**, an unbilled emulator supporting up to 12 atoms. Runtime metadata is authoritative. Hardware execution has not been validated.

## Install and check offline

```sh
uv build
uv venv build/consumer
uv pip install --python build/consumer/bin/python dist/pasqal_qdmi-*.whl
build/consumer/bin/python -c "from mqt.core.qdmi import builtin_driver; print(builtin_driver.registered_device_ids())"
uvx nox -s native tests docs
cmake -P test/install.cmake
```

On Windows the consumer interpreter is `build/consumer/Scripts/python.exe`. No cloud credentials are required for building, discovery, or normal tests. Source builds require CMake >=3.24, a C++20 compiler and TLS development libraries.

See [installation](docs/installation.md), [authentication](docs/authentication.md), [end-to-end usage](docs/usage.md), [capabilities](docs/capabilities.md), [development](docs/development.md) and [maintenance](docs/maintenance.md). Build the offline HTML documentation with `uvx nox -s docs`.

## License

This project is licensed under [Apache-2.0 WITH LLVM-exception](LICENSE). Builds include the applicable dependency license texts alongside installed artifacts. Copyright and SPDX headers are maintained by the `license-tools` pre-commit hook.

Live verification is a separate explicit command and only targets EMU_FREE.
