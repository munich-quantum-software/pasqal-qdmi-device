# Maintenance

Routine tests and documentation examples run offline unless cloud credentials
are explicitly supplied to the documentation build. CMake pins released native
dependencies; `uv.lock` records the Python development environment. Shared MQT
workflows are pinned to commits.

```console
uvx nox -s lint tests-3.14 docs
uvx nox -s native
cmake -P test/install.cmake
uv build
```

The C++ suite checks the native QDMI contract. Pytest exercises cloud transport
against local HTTP fixtures. The install probe relocates the prefix and builds a
fresh C consumer. Wheel tests exercise installed discovery, Core integration and
result ordering, including comparison with the official SDK and an independent
Pulser simulation. CI covers Linux, macOS and Windows, plus
address/undefined-behavior sanitizers on Linux. Cloud credentials are not
needed.

## Documentation

Build HTML with `uvx nox --non-interactive -s docs`. The session installs the
provider, MQT Core and Pulser, then executes the MyST notebooks. Successful
executions are cached; notebook failures and Sphinx warnings fail the build.
Inspect rendered pages with Chrome DevTools after layout changes.

Read the Docs uses Python 3.14 and the same Nox session. Import
`pasqal-qdmi-device` with `.readthedocs.yaml`; no Pasqal credentials are needed
for the offline examples. To execute [the free-emulator notebook](cloud.md), add
`PASQAL_EMAIL`, `PASQAL_PASSWORD` and the full project UUID as
`PASQAL_PROJECT_ID` in the project's
[private environment variables](https://docs.readthedocs.com/platform/stable/environment-variables.html).
Read the Docs withholds private variables from pull-request builds.

The cloud example targets literal EMU_FREE, checks its unbilled emulator
metadata and submits at most one two-atom, 20-shot job per execution. It prints
the job ID before a bounded wait. Set `PASQAL_JOB_ID` to reuse that job after a
failed or interrupted build. A clean notebook cache or changed code can trigger
execution again; remove the credentials to keep a build offline. Do not retry a
submission whose acceptance is unknown.

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
