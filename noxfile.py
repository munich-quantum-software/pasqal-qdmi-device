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

"""Python tests, lint and documentation builds."""

from __future__ import annotations

import argparse
import os
import shutil
import tempfile

import nox

nox.needs_version = ">=2026.10.20"
nox.options.default_venv_backend = "uv"

PYTHON_ALL_VERSIONS = ["3.11", "3.12", "3.13", "3.14", "3.15"]

if os.environ.get("CI", None):
    nox.options.error_on_missing_interpreters = True


@nox.session(reuse_venv=True, default=True)
def lint(session: nox.Session) -> None:
    """Run the shared pre-commit checks."""
    if shutil.which("prek") is None:
        session.install("prek")
    session.run("prek", "run", "--all-files", *session.posargs, external=True)


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, default=True)
def tests(session: nox.Session) -> None:
    """Test the installed package against local HTTP fixtures."""
    _run_tests(session)


def _run_tests(session: nox.Session, *, minimums: bool = False, live: bool = False) -> None:
    offline_env = {"PASQAL_EMAIL": "", "PASQAL_PASSWORD": "", "PASQAL_PROJECT_ID": "", "PASQAL_AUTHFILE": ""}
    session.run(
        "uv",
        "sync",
        *(["--resolution=lowest-direct"] if minimums else ["--locked"]),
        "--no-dev",
        "--group",
        "test",
        env={"UV_PROJECT_ENVIRONMENT": session.virtualenv.location, **offline_env},
    )
    session.run(
        "pytest",
        *session.posargs,
        "--cov-config=pyproject.toml",
        "-m",
        "live" if live else "not live",
        *(["-n", "0"] if live else []),
        env={} if live else offline_env,
    )


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, default=True)
def minimums(session: nox.Session) -> None:
    """Test minimum direct dependency versions."""
    with tempfile.TemporaryDirectory() as directory:
        lockfile = shutil.move("uv.lock", f"{directory}/uv.lock")
        try:
            _run_tests(session, minimums=True)
            session.run("uv", "tree", "--frozen", env={"UV_PROJECT_ENVIRONMENT": session.virtualenv.location})
        finally:
            shutil.move(lockfile, "uv.lock")


@nox.session(name="live-tests", python="3.14", reuse_venv=True, default=False)
def live_tests(session: nox.Session) -> None:
    """Run the EMU_FREE integration tests."""
    if not all(os.environ.get(name) for name in ("PASQAL_EMAIL", "PASQAL_PASSWORD", "PASQAL_PROJECT_ID")):
        session.error("EMU_FREE tests require Pasqal credentials")
    _run_tests(session, live=True)


@nox.session(python="3.14", reuse_venv=True, default=True)
def docs(session: nox.Session) -> None:
    """Build documentation.

    Pass ``--live`` to execute provider-backed notebooks.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--live", action="store_true", help="Execute provider-backed notebook examples")
    parser.add_argument("-b", dest="builder", default="html", help="Build target (default: html)")
    args, posargs = parser.parse_known_args(session.posargs)

    session.run(
        "uv",
        "sync",
        "--locked",
        "--no-dev",
        "--group",
        "docs",
        *(("--reinstall-package", "pasqal-qdmi") if args.live else ()),
        "--config-settings-package",
        f"pasqal-qdmi:wheel.cmake={'true' if args.live else 'false'}",
        env={"UV_PROJECT_ENVIRONMENT": session.virtualenv.location},
    )
    session.run(
        "cmake",
        "-S",
        ".",
        "-B",
        "build/docs",
        "-DBUILD_PASQAL_QDMI_TESTS=OFF",
        "-DBUILD_PASQAL_QDMI_DOCS=ON",
    )
    session.run(
        "cmake",
        "--build",
        "build/docs",
        "--target",
        "pasqal-qdmi-device-docs",
    )
    session.run(
        "sphinx-build",
        "-T",
        "-W",
        f"-b={args.builder}",
        "docs",
        f"docs/_build/{args.builder}",
        *posargs,
        env={"PASQAL_QDMI_LIVE_DOCS": "1" if args.live else "0"},
    )


if __name__ == "__main__":
    nox.main()
