# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Reproducible offline checks; live execution is a separate explicit script."""

import sys
from pathlib import Path

import nox

nox.options.default_venv_backend = "none"
nox.options.sessions = ["tests", "docs"]


@nox.session
def tests(session: nox.Session) -> None:
    """Test the installed package and local HTTP boundaries."""
    session.run("uv", "run", "--locked", "--group", "test", "pytest", *session.posargs)


@nox.session
def native(session: nox.Session) -> None:
    """Build and run CTest with the installed Python test environment."""
    session.run("uv", "sync", "--locked", "--group", "test")
    python = Path(".venv") / ("Scripts/python.exe" if sys.platform == "win32" else "bin/python")
    session.run(
        "cmake",
        "-S",
        ".",
        "-B",
        "build/native",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DPython3_EXECUTABLE={python.resolve()}",
        *session.posargs,
    )
    session.run("cmake", "--build", "build/native", "--config", "Release", "--parallel", "2")
    session.run("ctest", "--test-dir", "build/native", "-C", "Release", "--output-on-failure")


@nox.session
def docs(session: nox.Session) -> None:
    """Build documentation without installing the device or executing examples."""
    session.run(
        "uv",
        "run",
        "--locked",
        "--only-group",
        "docs",
        "sphinx-build",
        "-n",
        "-W",
        "--keep-going",
        "-b",
        "html",
        "docs",
        "docs/_build/html",
        *session.posargs,
    )


@nox.session
def lint(session: nox.Session) -> None:
    """Run configured source and workflow checks."""
    session.run(
        "uv",
        "run",
        "--locked",
        "--only-group",
        "lint",
        "prek",
        "run",
        "--all-files",
        *session.posargs,
    )
