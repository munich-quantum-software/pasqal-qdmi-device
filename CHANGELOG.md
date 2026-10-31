<!-- Entries in each category are sorted by merge time, with the latest PRs appearing first. -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on a mixture of [Keep a Changelog] and [Common Changelog].
This project adheres to [Semantic Versioning], with the exception that minor
releases may include breaking changes.

## [Unreleased]

### Added

- Expose regular QDMI sites for the selected backend's atom capacity, indexed by
  program register order.

### Changed

- ♻️ Rename the `core` extra to `mqt-core` ([#14]) ([**@denialhaag**]).

## [1.0.0] - 2026-10-27

### Added

- ✨ Add a Pulser remote connection for `QPUBackend` ([#2])
  ([**@awennersteen**]).

### Changed

- 📚 Document installation from PyPI and link to the hosted documentation ([#5])
  ([**@burgholzer**]).
- ⚡️ Serialize repeated concrete Pulser sequences once per batch ([#3])
  ([**@burgholzer**]).
- ⚡️ Avoid rescanning batches during indexed status and result reads ([#3])
  ([**@burgholzer**]).
- ⬆️ Require uv 0.13.4 for development ([#1]) ([**@denialhaag**]).

### Fixed

- 🐛 Limit concurrent cloud tests across CI runs to respect EMU_FREE capacity
  ([#9]) ([**@burgholzer**]).
- 🐛 Use GitHub CI for credentialed PR documentation builds ([#5])
  ([**@burgholzer**]).
- 🐛 Propagate Windows macro protection to native unit tests ([#5])
  ([**@burgholzer**]).
- 🐛 Enable PyPI uploads when a GitHub release is published ([#5])
  ([**@burgholzer**]).

<!-- Versions -->

[Unreleased]: https://github.com/munich-quantum-software/pasqal-qdmi-device/compare/v1.0.0...main
[1.0.0]: https://github.com/munich-quantum-software/pasqal-qdmi-device/releases/tag/v1.0.0

<!-- Pull requests -->

[#14]: https://github.com/munich-quantum-software/pasqal-qdmi-device/pull/14
[#9]: https://github.com/munich-quantum-software/pasqal-qdmi-device/pull/9
[#5]: https://github.com/munich-quantum-software/pasqal-qdmi-device/pull/5
[#3]: https://github.com/munich-quantum-software/pasqal-qdmi-device/pull/3
[#2]: https://github.com/munich-quantum-software/pasqal-qdmi-device/pull/2
[#1]: https://github.com/munich-quantum-software/pasqal-qdmi-device/pull/1

<!-- Contributors -->

[**@awennersteen**]: https://github.com/awennersteen
[**@burgholzer**]: https://github.com/burgholzer
[**@denialhaag**]: https://github.com/denialhaag

<!-- General links -->

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Common Changelog]: https://common-changelog.org/
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
