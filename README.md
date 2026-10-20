# Pasqal QDMI Device

MQSC-maintained QDMI device for submitting native Pulser programs to Pasqal, a full-stack neutral-atom quantum computing company. MQT Core supplies Python bindings, discovery and job/result handling; Pulser constructs the pulse programs.

This private `0.2.0` development package targets released QDMI 1.4.0 and MQT Core 4.1.1 or later. The initial validation target is **EMU_FREE**, currently an unbilled emulator supporting up to 12 atoms. Runtime metadata is authoritative. Emulator validation is not hardware validation.

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

See [installation](docs/installation.md), [authentication](docs/authentication.md), [end-to-end usage](docs/usage.md), [capabilities](docs/capabilities.md) and [maintenance](docs/maintenance.md). Build the offline HTML documentation with `uvx nox -s docs`.

## Publication status

The repository remains private. The supplied source archive omitted a root license and authorship evidence for most implementation files. Existing QDMI notices and documented upstream attribution are preserved; public distribution is blocked until the missing permissions are established. Read [LICENSE](LICENSE) and [NOTICE](NOTICE) before redistributing.

Live verification is a separate explicit command and only targets EMU_FREE. No documentation or test-success statement should be read as evidence that a hardware job was run.
