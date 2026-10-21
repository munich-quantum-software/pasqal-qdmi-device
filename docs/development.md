# Development

These practices adapt the relevant
[MQT Core development policy](https://github.com/munich-quantum-toolkit/core/blob/main/docs/development.md)
and
[AI contribution policy](https://github.com/munich-quantum-toolkit/core/blob/main/docs/ai_usage.md)
to this provider.

## Scope and design

Repository policy and configuration take precedence over incidental existing
patterns. Keep changes focused and preserve unrelated work. Follow a value from
its producer through shared helpers to its consumer before changing types,
ownership or error behavior.

Use C++20 and standard-library facilities before adding abstractions or
dependencies. Keep C++ exceptions inside the C ABI; return QDMI status codes at
that boundary. Python users should use MQT Core's existing discovery and
bindings. Pulse construction belongs in Pulser.

The cloud API is a trust boundary: validate identifiers, response shapes, counts
and ordering. Preserve accepted job identifiers. A failed read must never
trigger a repeated submission. Document unsupported capabilities instead of
guessing semantics.

## Code and documentation

Use the configured Ruff, clang-format and cmake-format hooks. Add direct
includes, prefer `#pragma once`, and keep implementation details private. Use
`//` for ordinary C++ comments and `///` for Doxygen documentation. Start
documentation with a short summary; explain contracts, ownership and constraints
only where the signature is insufficient. Use Google-style Python docstrings.

Write concise, direct prose. Documentation describes the current API and useful
examples, not the history of a review. Remove obsolete helpers, workarounds and
suppressions when their reason no longer applies. Keep necessary workarounds
narrow and explain their removal condition.

Run `uvx nox -s lint` before submitting a change. The `license-tools` hook
maintains copyright and SPDX headers according to `.license-tools-config.json`;
preserve inherited notices and human attribution. Generated third-party headers
retain their upstream license.

## Verification

Use the commands in [maintenance](maintenance.md). Add focused regression tests
for nontrivial behavior; test observable contracts rather than duplicating
implementation details. Formatting, prose and other low-impact changes do not
need new tests.

Before changing a failing check, distinguish an implementation defect from stale
output, dependency mismatch or service failure. Do not remove coverage without
checking its caller, invariant and history. Use independently derived
expectations for ordering and sampled outcomes, with tolerances that make false
failures unlikely.

Routine tests use local HTTP servers and need no cloud credentials. Executable
examples belong in the MyST-NB documentation. The EMU_FREE notebook runs only
when its documented credential environment variables are supplied. Keep one-off
experiments, reports and machine setup local. Record performance claims only
with representative measurements.

## Commits and contributions

Start commit subjects with a gitmoji followed by an imperative description. Aim
for 50 characters and keep subjects within 72 characters, with no final period.
Use a blank line before a body explaining the reason and constraints. For
example:

```text
🐛 Preserve batch IDs after failed result reads

Keep accepted submissions retrievable when the cloud read fails.

Assisted-by: Codex
```

Sign every commit and annotated tag with the configured signing key. Verify
commits with `git verify-commit HEAD` before pushing. Preserve legitimate human
`Co-authored-by` trailers; use `Assisted-by` for AI assistance.

Humans remain accountable for contributed code and public communication.
Understand, review and verify AI-assisted changes. A scoped authorization covers
routine implementation work; it does not authorize unrelated cloud jobs,
publication or unsolicited messages. Do not automate issues marked as beginner
exercises or submit repetitive, unreviewed output.

Agent-authored or agent-edited issue, pull-request and review bodies begin with
`🤖 *AI text below* 🤖`; titles are exempt. Describe material AI assistance in
pull requests. Follow the existing template, assign submitted pull requests to
`@burgholzer`, and use appropriate existing labels. Use the GitHub REST API for
pull-request edits instead of `gh pr edit`.
