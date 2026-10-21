# Maintenance

Routine checks are deterministic and offline. CMake pins released native
dependencies; `uv.lock` records the Python development environment. Shared MQT
workflows are pinned to commits.

```console
uvx nox -s lint tests docs
uvx nox -s native
cmake -P test/install.cmake
uv build
```

The native suite checks the QDMI contract and cloud transport against local HTTP
fixtures. The install probe relocates the prefix and builds a fresh C consumer.
Wheel tests exercise installed discovery, Core integration and result ordering,
including comparison with the official SDK and an independent Pulser simulation.
CI covers Linux, macOS and Windows, plus address/undefined-behavior sanitizers
on Linux. Cloud credentials are not needed.

## Documentation

Build HTML with `uvx nox -s docs`. Sphinx treats warnings as errors, never
executes cloud examples and does not import the native library. Inspect rendered
pages with Chrome DevTools after layout changes.

When the repository becomes public, import `pasqal-qdmi-device` into Read the
Docs using `.readthedocs.yaml`. No Pasqal credentials are required.

## Releases

Update the version in `pyproject.toml` and `CMakeLists.txt`, and move the
changelog's unreleased entries into a dated release section. Validate the final
built wheels before tagging a release. The release workflow builds the source
distribution and platform wheels; PyPI publishing uses the `pypi` environment
and trusted publishing. Publication remains disabled while the repository is
private.

## Licensing

Project sources use Apache-2.0 WITH LLVM-exception. The `license-tools` hook
maintains MQSC and Pasqal copyright headers. Files adapted from upstream
projects retain their original notices as well.

There is no source `licenses` directory or project `NOTICE`. Build artifacts
carry notices for bundled dependencies, copied from the dependencies used by the
build. Check repaired wheels when changing dependencies or build images, since
repair may add shared libraries. Externally supplied native dependencies remain
the packager's responsibility when redistributed.
