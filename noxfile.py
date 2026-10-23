#!/usr/bin/env -S uv run --script --quiet
# Copyright (c) 2026 Munich Quantum Software Company GmbH
# Copyright (c) 2026 Pasqal SAS
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# /// script
# dependencies = ["nox"]
# ///

"""Development checks for the native device and its installed package."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path

import nox

nox.needs_version = ">=2026.10.20"
nox.options.default_venv_backend = "uv"

PYTHON_ALL_VERSIONS = ["3.11", "3.12", "3.13", "3.14", "3.15"]


@nox.session(reuse_venv=True, default=True)
def lint(session: nox.Session) -> None:
    """Run the shared pre-commit checks."""
    if shutil.which("prek") is None:
        session.install("prek")
    session.run("prek", "run", "--all-files", *session.posargs, external=True)


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, default=True)
def tests(session: nox.Session) -> None:
    """Test the installed package on EMU_FREE and local HTTP boundaries."""
    session.run(
        "uv",
        "run",
        "--locked",
        "--no-dev",
        "--group",
        "test",
        "pytest",
        *session.posargs,
        env={"UV_PROJECT_ENVIRONMENT": session.virtualenv.location},
    )


@nox.session(venv_backend="none", default=False)
def native(session: nox.Session) -> None:
    """Build and run CTest; pass --sanitizers for Linux ASan and UBSan checks."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", choices=("Release", "Debug"), default="Release")
    parser.add_argument("--sanitizers", action="store_true")
    args, cmake_args = parser.parse_known_args(session.posargs)
    build_dir = Path("build") / ("sanitizers" if args.sanitizers else "native")
    if args.sanitizers:
        flags = "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all"
        cmake_args.extend([f"-DCMAKE_C_FLAGS={flags}", f"-DCMAKE_CXX_FLAGS={flags}"])
    session.run(
        "cmake",
        "-S",
        ".",
        "-B",
        str(build_dir),
        f"-DCMAKE_BUILD_TYPE={args.config}",
        *cmake_args,
    )
    session.run("cmake", "--build", str(build_dir), "--config", args.config, "--parallel", "2")
    session.run(
        "ctest",
        "--test-dir",
        str(build_dir),
        "-C",
        args.config,
        "--output-on-failure",
        "--timeout",
        "600",
    )


@nox.session(python="3.14", reuse_venv=True, default=True)
def docs(session: nox.Session) -> None:
    """Build documentation and execute its notebook examples."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-b", dest="builder", default="html", help="Sphinx builder")
    args, posargs = parser.parse_known_args(session.posargs)
    session.run(
        "uv",
        "run",
        "--locked",
        "--no-dev",
        "--group",
        "docs",
        "sphinx-build",
        "-n",
        "-W",
        "--keep-going",
        "-b",
        args.builder,
        "docs",
        f"docs/_build/{args.builder}",
        *posargs,
        env={"UV_PROJECT_ENVIRONMENT": session.virtualenv.location},
    )


if __name__ == "__main__":
    nox.main()
