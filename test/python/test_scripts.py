# Copyright (c) 2026 Munich Quantum Software Company GmbH
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

"""Small checks for secure storage and deterministic live-validation inputs."""

from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / f"{name}.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.mark.skipif(os.name != "posix", reason="DGX credential permissions use POSIX modes")
def test_credentials_stay_private_and_preserve_password(tmp_path):
    config = load_script("configure_credentials")
    directory = tmp_path / "credentials"
    config.store(directory, "password", "  synthetic = password  ")
    assert directory.stat().st_mode & 0o777 == 0o700
    assert (directory / "password").stat().st_mode & 0o777 == 0o600
    assert (directory / "password").read_text() == "  synthetic = password  \n"
    with pytest.raises(ValueError, match="one nonempty line"):
        config.store(directory, "password", "injected\nline")
    assert (directory / "password").read_text() == "  synthetic = password  \n"


def test_live_programs_match_independent_oracle_fixtures():
    live = load_script("validate_emu_free")
    expected = [
        (ROOT / "test/fixtures" / name).read_text()
        for name in ("first-atom.json", "last-atom.json")
    ]
    assert [json.loads(p) for p in live.programs()] == [json.loads(p) for p in expected]


def test_live_deadline_preserves_receipt_without_resubmission(tmp_path, monkeypatch):
    import subprocess
    import sys
    from unittest.mock import Mock

    live = load_script("validate_emu_free")
    output = tmp_path / "receipt.json"

    def timed_out(*args, **kwargs):
        output.write_text(json.dumps({"job_id": "pasqal-batch:known", "phase": "waiting"}))
        raise subprocess.TimeoutExpired("validation", 600)

    run = Mock(side_effect=timed_out)
    monkeypatch.setattr(subprocess, "run", run)
    monkeypatch.setattr(
        sys, "argv", ["validate_emu_free.py", "--run-live", "--output", str(output)]
    )
    assert live.main() == 1
    run.assert_called_once()
    assert run.call_args.kwargs["timeout"] == 600
    assert json.loads(output.read_text())["job_id"] == "pasqal-batch:known"
    assert json.loads(output.read_text())["error_type"] == "OverallTimeout"
