<!-- Entries in each category are sorted by merge time, with the latest PRs appearing first. -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on a mixture of [Keep a Changelog] and [Common Changelog].
This project adheres to [Semantic Versioning], with the exception that minor
releases may include breaking changes.

## [Unreleased]

## [1.0.0] - 2026-10-27

### Added

- ✨ Add a Pulser remote connection for `QPUBackend` ([#2])
  ([**@awennersteen**]).
- 🎉 Initial commit ([**@awennersteen**], [**@burgholzer**]).

### Changed

- 📚 Document installation from PyPI and link to the hosted documentation ([#5])
  ([**@burgholzer**]).
- ⚡️ Serialize repeated concrete Pulser sequences once per batch ([#3])
  ([**@burgholzer**]).
- ⚡️ Avoid rescanning batches during indexed status and result reads ([#3])
  ([**@burgholzer**]).
- ⬆️ Require uv 0.13.4 for development ([#1]) ([**@denialhaag**]).

### Fixed

- 🐛 Propagate Windows macro protection to native unit tests ([#5])
  ([**@burgholzer**]).
- 🐛 Enable publishing from public GitHub releases ([#5]) ([**@burgholzer**]).

<!-- Versions -->

[Unreleased]: https://github.com/munich-quantum-software/pasqal-qdmi-device/compare/v1.0.0...main
[1.0.0]: https://github.com/munich-quantum-software/pasqal-qdmi-device/releases/tag/v1.0.0

<!-- Pull requests -->

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
