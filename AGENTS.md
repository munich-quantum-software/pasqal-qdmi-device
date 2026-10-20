# Repository guidance

Read [docs/development.md](docs/development.md) for coding, testing, commit and
AI contribution policy. Read [docs/capabilities.md](docs/capabilities.md) before
changing a QDMI or Pasqal boundary; [docs/maintenance.md](docs/maintenance.md)
contains validation and release commands.

- Preserve existing work and keep changes within the authorized task. Complete
  reversible implementation and checks without routine confirmation questions.
  Ask when a consequential requirement or external action lacks authorization.
- Trace the producer, shared helper and consumer before changing a contract.
  Reuse the standard library and existing dependencies. Prefer deletion and the
  smallest complete fix over new abstractions.
- Preserve validation, failure handling, ownership and deterministic ordering.
  Never retry a cloud submission automatically.
- Test observable contracts and regressions. Diagnose a failed check before
  changing code or weakening an assertion. Stop repeating passing checks unless
  a change or concrete concern justifies it.
- Keep machine configuration, exploratory scripts and task reports outside the
  tracked repository. Never print or commit credentials or authentication
  responses. Cloud execution needs explicit authorization; ordinary CI is offline.
- Document current contracts, limitations and useful examples. Keep review
  history and implementation plans out of product documentation.
- Use gitmoji-led imperative commit subjects and signed commits. Preserve human
  attribution; record AI assistance with `Assisted-by`, never `Co-authored-by`.
- Report checks actually run, remaining limits and external setup separately.
  Local checks do not establish hosted CI, publication or hardware validation.
