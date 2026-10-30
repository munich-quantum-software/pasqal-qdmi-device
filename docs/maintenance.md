# Maintenance

Use CMake presets for native checks and Nox for Python tests and documentation.
CMake pins native dependencies; `uv.lock` records the Python development
environment.

```console
cmake --preset release
cmake --build --preset release
ctest --preset release
uvx nox -s tests minimums
uvx nox --non-interactive -s docs
cmake -P test/install.cmake
```

The `debug`, `coverage`, `lint` and Linux `sanitizers` presets use separate
build directories. Native test presets exclude the `integration` label. Python
tests and minimum-version sessions exclude `live` tests, including in a
credentialed shell. CI runs native tests on Linux, macOS and Windows, with
Python tests and minimums in a separate Linux job. Wheel and release builds test
local fixtures.

Trusted CI calls `reusable-live-tests.yml` after offline native, Python and
wheel checks. It queries EMU_FREE metadata, runs one Python batch of two
three-atom, 100-shot programs, and executes the documentation example. The test
verifies that the selected backend is an unbilled emulator before submitting.
Fork PRs skip this workflow. Test and documentation batches share the Linux
EMU_FREE concurrency slot across runs.

For an explicitly intended free-cloud check, configure the credentials below and
use `uvx nox -s live-tests`. Keep hardware and billed emulators outside this
workflow.

For branch protection, require the GitHub Actions `🚦 Check` status from CI. It
aggregates the native, Python, coverage, Python lint, packaging and live checks,
allowing only skips selected by change detection or fork safety. Keep separately
required external checks, such as pre-commit.ci and Read the Docs. Draft Python
checks cover 3.14; ready PRs use all supported versions.

## Credentials

Live tests and executable examples require these three values:

| Name                | Value                                        |
| ------------------- | -------------------------------------------- |
| `PASQAL_EMAIL`      | Pasqal Cloud account email                   |
| `PASQAL_PASSWORD`   | Account password                             |
| `PASQAL_PROJECT_ID` | Full project UUID, not an abbreviated prefix |

- **GitHub:** repository
  **Settings → Secrets and variables → Actions → New repository secret**. Only
  the reusable live-test workflow receives these three secrets.
- **Local development:** export the same variables securely. Applications can
  also use the protected auth file described in
  [authentication](authentication.md).

The account must have access to the project and EMU_FREE. No copied access
token, client secret or saved job ID is required. Other cloud emulators may
consume credits and are not used by the test suite.

## Documentation

Build HTML with `uvx nox --non-interactive -s docs`. It installs the provider,
MQT Core and Pulser and renders the end-to-end example without execution. Use
`uvx nox --non-interactive -s docs -- --live` to execute the EMU_FREE example.
Successful notebook executions are cached; errors and Sphinx warnings fail the
build. Clear `docs/_build/.jupyter_cache` to force execution after changing
credentials. A fresh execution submits one small EMU_FREE batch.

Read the Docs builds the default branch and pull requests with
`.readthedocs.yaml`, Python 3.14 and the same Nox command. These builds need no
cloud credentials. The reusable live workflow executes notebooks for trusted
changes. Inspect rendered pages with Chrome DevTools after layout changes.

## Releases

Release Drafter updates the draft GitHub release when changes reach `main`.
Review its notes against `CHANGELOG.md` before publishing.

Update the version in `pyproject.toml` and `CMakeLists.txt`, and move the
changelog's unreleased entries into a dated release section. Validate the final
built wheels before tagging a release. The release workflow builds the source
distribution and platform wheels; PyPI publishing uses the `pypi` environment
and trusted publishing. Publishing a GitHub release triggers the PyPI upload;
manual workflow runs only build and test the artifacts.

## Licensing

Project sources use Apache-2.0 WITH LLVM-exception. The `license-tools` hook
maintains MQSC and Pasqal copyright headers. Files adapted from upstream
projects retain their original notices as well.

Build artifacts include `LICENSE` and the license texts of bundled CPR, curl and
JSON. Linux wheels also contain OpenSSL 3 under Apache-2.0; the LLVM exception
applies only to the device. Check bundled libraries and their notices when
changing dependencies or wheel images.
