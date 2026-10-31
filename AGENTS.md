# Pasqal QDMI Device Agent Guide

Read [the development policy](docs/development.md) for code, testing and
contribution guidance. Read [capabilities](docs/capabilities.md) before changing
a QDMI or Pasqal boundary. [Maintenance](docs/maintenance.md) owns build,
release and documentation setup.

## Repository layout

- `include/pasqal-qdmi-device/` and `src/` contain the C++ implementation and
  Pasqal-specific constants; `cmake/` contains build support and the catalogue.
- `python/pasqal/qdmi/` exposes installed package paths and Core discovery.
- `test/` contains native tests, local HTTP fixtures and installed-package
  tests.
- `docs/` contains Sphinx documentation and executable MyST-NB examples.
- Keep generated output in `build/` and `docs/_build/`, outside commits.

## Working principles

- Inspect the worktree first and preserve unrelated changes. Complete authorized
  work and relevant checks without routine confirmation questions.
- Trace producers, shared helpers and consumers before changing contracts. Reuse
  C++20, existing dependencies and MQT Core's shared Python API.
- Keep C++ exceptions inside the QDMI C boundary. Preserve validation,
  ownership, result ordering and accepted job identifiers. Never retry a cloud
  submission automatically.
- Test observable behavior and concrete regressions. Diagnose failed checks
  before changing code or weakening an assertion.
- Use short sentences and established terms. Document the current design,
  contracts and useful examples; keep review history out of product docs.
- Use `///` for Doxygen and `//` for ordinary C++ comments. Follow configured
  formatting, lint, typing and license-header checks.
- Keep exploratory scripts, machine setup and task reports outside the checkout.
  Never print or commit credentials or authentication responses.
- Explicit live tests and notebook execution use EMU_FREE, the free cloud
  emulator. Keep workloads small and waits bounded. Local HTTP fixtures test
  failures. Other emulators may be billed; hardware and paid execution need
  authorization.
- Use signed, gitmoji-led commits. Preserve human attribution; record AI
  assistance with `Assisted-by`, never `Co-authored-by`. Follow the development
  policy for pull requests.
- Separate local checks, hosted CI, publication and hardware validation in
  reports.

## Validation

Run focused checks while developing and `uvx nox -s lint` before submission. Use
`uvx nox -s tests-3.14` for one Python version and `uvx nox -s tests` for the
supported matrix. Configure and build with the `release` CMake preset, then run
`ctest --preset release`. Build documentation with
`uvx nox --non-interactive -s docs`; see [maintenance](docs/maintenance.md) for
explicit EMU_FREE validation.
