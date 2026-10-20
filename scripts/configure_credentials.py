#!/usr/bin/env python3
# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Store local Pasqal credentials without shell history, echo, or repository files."""

from __future__ import annotations

import argparse
import getpass
import os
import tempfile
from pathlib import Path


def store(directory: Path, name: str, value: str) -> None:
    if not value or "\n" in value or "\r" in value:
        raise ValueError(f"{name} must be one nonempty line")
    if directory.is_symlink():
        raise ValueError("credential directory must not be a symbolic link")
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    directory.chmod(0o700)
    fd, path = tempfile.mkstemp(prefix=".credential-", dir=directory)
    try:
        with os.fdopen(fd, "w") as stream:
            stream.write(value + "\n")
        os.chmod(path, 0o600)
        os.replace(path, directory / name)
    finally:
        Path(path).unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--directory", type=Path, default=Path.home() / ".config/codex/pasqal-qdmi-device"
    )
    parser.add_argument("--email")
    parser.add_argument("--project")
    args = parser.parse_args()
    for name, value in (("email", args.email), ("project-id", args.project)):
        if value is not None:
            store(args.directory, name, value)
        elif not (args.directory / name).is_file():
            parser.error(f"provide --{'project' if name == 'project-id' else name}")
    if not os.isatty(0):
        parser.error("run in an interactive local terminal; password input must not be echoed")
    store(args.directory, "password", getpass.getpass("Pasqal password (hidden): "))
    print("Credential files saved with restricted permissions. No token is stored.")


if __name__ == "__main__":
    main()
