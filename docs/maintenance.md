# Maintenance and onboarding

Routine checks are deterministic and offline. The source uses pinned QDMI 1.4.0 and versioned CPR/curl/JSON dependencies; `uv.lock` records Python dependencies. Shared MQT workflow files are pinned to commits. Revalidate installed artifacts after dependency or ABI changes.

```sh
uvx nox -s native tests docs lint
cmake -P test/install.cmake
uv build
```

The CMake install probe relocates the prefix and builds a fresh native consumer. Python tests should use a clean installed wheel and real Core discovery. Native conformance and focused HTTP tests exercise errors, buffers, handles, auth expiry, malformed responses, cancellation races and lost submissions. Test-only ctypes probes verify the C boundary; they are not a second production API.

CI includes Linux/macOS/Windows native checks, a Linux Debug build with address and undefined-behavior sanitizers, Linux x86_64/ARM64 and macOS/Windows wheel builds, source distribution, lint and documentation. Repaired wheels run the installed-provider and HTTP boundary suites. The required aggregate check is **Required offline checks**. Local success does not establish another platform or a later commit's status.

## Read the Docs onboarding

After the repository becomes public, authorize the Read the Docs GitHub App for `munich-quantum-software/pasqal-qdmi-device`, import project `pasqal-qdmi-device`, select default branch `main` and configuration `.readthedocs.yaml`. No Pasqal credentials are needed for documentation.

The HTML build uses locked Sphinx/Furo/MyST dependencies and never executes examples or imports the native library. Inspect local rendering with Chrome DevTools in addition to a warning-as-error build. See [Read the Docs project setup](https://docs.readthedocs.com/platform/stable/intro/add-project.html).

## Explicit cloud workflow

The `Free emulator` workflow is manual, main-only, targets literal EMU_FREE and runs in protected GitHub environment `pasqal-emu-free`. Configure these only after reviewing the workflow:

| Name | Kind | Purpose |
| --- | --- | --- |
| `PASQAL_EMAIL` | Environment secret | Pasqal account login |
| `PASQAL_PASSWORD` | Environment secret | Password-realm authentication and refresh lifecycle |
| `PASQAL_PROJECT_ID` | Environment variable | Authorized project identifier, resolved to UUID if needed |

The account must have access to that project. This is not a claim that Pasqal offers a provider-enforced free-only account scope. The launcher enforces EMU_FREE, verifies emulator/unbilled metadata, limits the workload and records sanitized results. The workflow has `contents: read`; no GitHub PAT, static Pasqal token, hardware permission or routine-CI secret is needed.

## Releases and licensing

`cd.yml` builds artifacts on manual dispatch and when a GitHub release is published. Only a published release from a public repository with `PUBLICATION_APPROVED=true` can upload to PyPI. The publishing job uses environment `pypi`; allow release tags matching `v*` in that environment and enable required reviewers where the account supports them. Manual dispatch only builds artifacts. Keep publication disabled and preserve repository visibility until publication is explicitly authorized.

Configure a PyPI trusted publisher for project `pasqal-qdmi`, GitHub owner `munich-quantum-software`, repository `pasqal-qdmi-device`, workflow `cd.yml`, environment `pypi`. The publishing job uses `id-token: write` and `contents: read`; no `PYPI_API_TOKEN` is needed. A pending publisher does not reserve a package name. See [PyPI trusted publishing](https://docs.pypi.org/trusted-publishers/creating-a-project-through-oidc/).

Project sources use Apache-2.0 WITH LLVM-exception. The `license-tools` hook updates headers from `.license-tools-config.json` and preserves inherited copyright holders. Shared implementation files list MQSC first and Pasqal second; upstream QDMI and QRMI attribution remains intact.

There is no manually maintained `licenses` directory or project `NOTICE`. CMake installs required dependency license texts from the dependencies used by the build. Wheels carry these beside the project license in their distribution metadata. Recheck the repaired wheel contents and license texts when changing dependencies or platform images; additional bundled libraries can impose additional notice requirements.

Builds that supply external dependencies through `find_package` keep those dependencies under the package maintainer's control. Include their notices if redistributing them. On a Linux wheel build outside the supported images, set `PASQAL_OPENSSL_LICENSE` if the system's OpenSSL license is in a different location.

Renovate may be enabled for the selected repository through the organization's existing GitHub App. Optional coverage hosting can be added later without making offline CI depend on an external account. Do not request unused service secrets.
