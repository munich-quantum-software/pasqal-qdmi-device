# Maintenance and onboarding

Routine checks are deterministic and offline. The source uses pinned QDMI 1.4.0 and versioned CPR/curl/JSON dependencies; `uv.lock` records Python dependencies. Shared MQT workflow files are pinned to commits. Revalidate installed artifacts after dependency or ABI changes.

```sh
uvx nox -s native tests docs lint
cmake -P test/install.cmake
uv build
```

The CMake install probe relocates the prefix and builds a fresh native consumer. Python tests should use a clean installed wheel and real Core discovery. Native conformance and focused HTTP tests exercise errors, buffers, handles, auth expiry, malformed responses, cancellation races and lost submissions. Test-only ctypes probes verify the C boundary; they are not a second production API.

CI includes Linux/macOS/Windows native checks, Linux x86_64/ARM64 and macOS/Windows wheel builds, source distribution, lint and documentation. Hosted checks remain pending until a specific commit finishes them. The required aggregate check is **Required offline checks**. Local success does not establish another platform or a later commit's status.

## Read the Docs onboarding

Private repository hosting requires **Read the Docs Business**. After the repository and `.readthedocs.yaml` are ready, a repository administrator must install/authorize the Read the Docs GitHub App for `munich-quantum-software/pasqal-qdmi-device`, import project `pasqal-qdmi-device`, select default branch `main` and configuration `.readthedocs.yaml`, and keep documentation and PR previews private. No Pasqal credentials are needed for documentation.

The HTML build uses locked Sphinx/Furo/MyST dependencies and never executes examples or imports the native library. Local rendered inspection uses Chrome DevTools in addition to a warning-as-error build. Read the Docs onboarding and its first hosted build are external setup, not implied by a local successful build. See [private repository setup](https://docs.readthedocs.com/platform/latest/guides/creating-project-private-repository.html).

## Explicit cloud workflow

The `Free emulator` workflow is manual, main-only, targets literal EMU_FREE and runs in protected GitHub environment `pasqal-emu-free`. Configure these only after reviewing the workflow:

| Name | Kind | Purpose |
| --- | --- | --- |
| `PASQAL_EMAIL` | Environment secret | Pasqal account login |
| `PASQAL_PASSWORD` | Environment secret | Password-realm authentication and refresh lifecycle |
| `PASQAL_PROJECT_ID` | Environment variable | Authorized project identifier, resolved to UUID if needed |

The account must have access to that project. This is not a claim that Pasqal offers a provider-enforced free-only account scope. The launcher enforces EMU_FREE, verifies emulator/unbilled metadata, limits the workload and records sanitized results. The workflow has `contents: read`; no GitHub PAT, static Pasqal token, hardware permission or routine-CI secret is needed.

## Release preparation and licensing gate

`cd.yml` is manual and defaults to building artifacts only. Public PyPI upload additionally requires the explicit `publish` input, repository variable `PUBLICATION_APPROVED=true`, the `main` branch and approval of environment `pypi`. Leave publication disabled until the user authorizes publication **and** the inherited licensing evidence is complete. Do not change repository visibility as part of preparing a release.

After clearance, configure a PyPI trusted publisher for project `pasqal-qdmi`, GitHub owner `munich-quantum-software`, repository `pasqal-qdmi-device`, workflow `cd.yml`, environment `pypi`. The publishing job uses `id-token: write` and `contents: read`; no `PYPI_API_TOKEN` is needed. A pending publisher does not reserve a package name. See [PyPI trusted publishing](https://docs.pypi.org/trusted-publishers/creating-a-project-through-oidc/).

The original archive omitted license/authorship evidence for most code. Existing QDMI Apache/LLVM-exception notices and QRMI Pasqal/IBM attribution must remain. MQSC's new infrastructure does not establish a grant for the entire inherited draft. Preserve separate holder lines, MQSC first and Pasqal second where appropriate, without inventing rights holders or years. Review `LICENSE`, `NOTICE` and bundled dependency notices before public redistribution.

Renovate may be enabled for the selected repository through the organization's existing GitHub App. Optional coverage hosting can be added later without making offline CI depend on an external account. Do not request unused service secrets.
