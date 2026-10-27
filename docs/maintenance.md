# Maintenance

Ordinary tests and executable documentation use Pasqal's free EMU_FREE service.
CMake pins released native dependencies; `uv.lock` records the Python
development environment. Configure the credentials below before running the full
checks:

```console
uvx nox -s lint tests-3.14 docs
uvx nox -s native
cmake -P test/install.cmake
uv build
```

C++ conformance tests query live metadata. Python tests submit one batch of two
three-atom, 100-shot programs per interpreter/platform, then verify retrieval,
result ordering and parity with the official SDK. Local HTTP fixtures cover
failures that cannot be produced reliably against the service. Missing
credentials or failed cloud execution fail the normal Python suite.

CI covers Linux, macOS and Windows, including installed wheels, relocated native
consumers and Linux sanitizers. Fork pull requests run the Python tests without
the `integration` marker and omit credentialed documentation builds; upstream
credentials are never exposed to fork code. Normal trusted CI runs every test.
To select only local checks during development, use
`pytest -m "not integration"` and
`ctest --test-dir build/native -LE integration`.

## Credentials

Set the same three values in each environment:

| Name                | Value                                        |
| ------------------- | -------------------------------------------- |
| `PASQAL_EMAIL`      | Pasqal Cloud account email                   |
| `PASQAL_PASSWORD`   | Account password                             |
| `PASQAL_PROJECT_ID` | Full project UUID, not an abbreviated prefix |

- **GitHub:** repository
  **Settings → Secrets and variables → Actions → New repository secret**. CI and
  release wheel tests forward these three secrets.
- **Read the Docs:** project **Admin → Environment Variables**, with
  **Public unchecked**. GitHub secrets are not inherited by RtD.
- **Local development:** export the same variables securely. Applications can
  also use the protected auth file described in
  [authentication](authentication.md).

The account must have access to the project and EMU_FREE. No copied access
token, client secret or saved job ID is required. Other cloud emulators may
consume credits and are not used by the test suite.

## Documentation

Build HTML with `uvx nox --non-interactive -s docs`. It installs the provider,
MQT Core and Pulser and executes the end-to-end example. Successful notebook
executions are cached; errors and Sphinx warnings fail the build. Clear
`docs/_build/.jupyter_cache` to force execution after changing credentials. A
fresh execution submits one small EMU_FREE batch.

Import the project into Read the Docs with `.readthedocs.yaml`, add the three
private environment variables, and build the default branch. RtD uses Python
3.14 and the same Nox command. RtD pull-request builds are skipped because
private variables are unavailable there. Trusted GitHub CI executes the
notebooks for same-repository pull requests and uploads the `documentation` HTML
artifact for review. Inspect rendered pages with Chrome DevTools after layout
changes.

## Releases

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
