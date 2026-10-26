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

"""Exercise a wheel-installed provider through released Core and official SDK."""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import numpy as np
import pytest
from cloud import PROGRAMS, Cloud
from mqt.core.qdmi import ProgramFormat, builtin_driver
from pasqal_cloud.backends import RemoteMPSBackend
from pulser import Sequence
from pulser.backend import EmulationConfig
from pulser.backend.default_observables import Occupation
from pulser.backend.remote import RemoteResults
from pulser_simulation import QutipBackendV2

from pasqal.qdmi.pulser import QDMIConnection

if TYPE_CHECKING:
    from pathlib import Path

    from mqt.core.qdmi import Device


def open_device(cloud: Cloud) -> Device:
    """Return the installed provider connected to the local fixture server."""
    return builtin_driver.open_device(
        "pasqal.emu-free",
        base_url=cloud.base,
        token="token",  # ruff: ignore[hardcoded-password-func-arg] - local fixture credential
        custom3="project",
    )


def test_catalogue_discovery_is_offline_and_from_installed_wheel(tmp_path: Path) -> None:
    """Discover every catalogue entry without importing the provider module."""
    code = """
import sys
from mqt.core.qdmi import builtin_driver
assert 'pasqal.qdmi' not in sys.modules
expected = {
    'pasqal.default', 'pasqal.emu-free', 'pasqal.emu-fresnel', 'pasqal.emu-mps',
    'pasqal.emu-sv', 'pasqal.emu-tn', 'pasqal.fresnel', 'pasqal.fresnel-can1',
    'pasqal.fresnel-sa1',
}
assert expected <= set(builtin_driver.registered_device_ids())
assert 'pasqal.qdmi' not in sys.modules
assert 'pasqal.qdmi' not in sys.modules
from pasqal.qdmi import PASQAL_QDMI_CATALOG_PATH, PASQAL_QDMI_DEVICE_ID, PASQAL_QDMI_LIBRARY_PATH
assert PASQAL_QDMI_DEVICE_ID == "pasqal.default"
assert PASQAL_QDMI_DEVICE_ID in expected
assert PASQAL_QDMI_CATALOG_PATH.is_file()
assert PASQAL_QDMI_LIBRARY_PATH.is_file()
assert 'site-packages' in str(PASQAL_QDMI_LIBRARY_PATH)
"""
    subprocess.run([sys.executable, "-I", "-c", code], cwd=tmp_path, check=True)  # ruff: ignore[subprocess-without-shell-equals-true] - fixed interpreter and source


def test_core_submit_error_does_not_retry(cloud: Cloud) -> None:
    """A lost submission response must not create a second remote batch."""
    cloud.drop_submission = True
    with pytest.raises(RuntimeError):
        open_device(cloud).submit_job(PROGRAMS, ProgramFormat.CUSTOM1, num_shots=9)
    assert len(cloud.submissions) == 1


def test_independent_pulser_oracle() -> None:
    """Verify fixture atom order independently with Pulser simulation."""
    for source, expected in zip(PROGRAMS, ([1, 0, 0], [0, 0, 1]), strict=True):
        sequence = Sequence.from_abstract_repr(source)
        assert list(sequence.register.qubit_ids) == ["z", "a", "m"]
        result = QutipBackendV2(sequence, config=EmulationConfig(observables=[Occupation()])).run()
        np.testing.assert_allclose(result.get_result("occupation", 1.0), expected, atol=1e-5)


def test_pulser_connection_round_trip(cloud: Cloud) -> None:
    """Submit through Pulser and retrieve SDK-ordered counts from a fresh connection."""
    session = {"base_url": cloud.base, "token": "token", "custom3": "project"}
    sequence = Sequence.from_abstract_repr(PROGRAMS[0])
    results = QDMIConnection(**session).submit(sequence, job_params=[{"runs": 9}] * 2)
    assert results.batch_id == "pasqal-batch:batch-1"
    assert cloud.submissions[0]["jobs"] == [{"sequence": sequence.to_abstract_repr(), "runs": 9}] * 2
    fresh = RemoteResults(results.batch_id, QDMIConnection(**session))
    assert fresh.job_ids == ["pasqal-batch:batch-1#0", "pasqal-batch:batch-1#1"]
    assert [r.final_bitstrings for r in fresh.results] == list(cloud.counters.values())


def test_pulser_connection_rejects_unsupported_submissions(cloud: Cloud) -> None:
    """Reject emulator overrides and empty jobs before creating a batch."""
    connection = QDMIConnection(base_url=cloud.base, token="token", custom3="project")  # ruff: ignore[hardcoded-password-func-arg] - fixture
    sequence = Sequence.from_abstract_repr(PROGRAMS[0])
    with pytest.raises(NotImplementedError, match="Unsupported Pulser submission arguments"):
        RemoteMPSBackend(sequence, connection).run([{"runs": 9}])
    with pytest.raises(ValueError, match="job parameter"):
        connection.submit(sequence, job_params=[])
    assert cloud.submissions == []


def test_pulser_wait_failure_preserves_batch_id(cloud: Cloud) -> None:
    """Recover an accepted batch after waiting fails without submitting again."""
    session = {"base_url": cloud.base, "token": "token", "custom3": "project"}
    cloud.errors["/core-fast/api/v2/jobs/job-z"] = 404
    with pytest.raises(RuntimeError) as error:
        QDMIConnection(**session).submit(
            Sequence.from_abstract_repr(PROGRAMS[0]), wait=True, job_params=[{"runs": 9}] * 2
        )
    assert "pasqal-batch:batch-1" in " ".join(error.value.__notes__)
    cloud.errors.clear()
    recovered = RemoteResults("pasqal-batch:batch-1", QDMIConnection(**session))
    assert recovered.results[0].final_bitstrings == cloud.counters["job-z"]
    assert len(cloud.submissions) == 1


def test_pulser_selected_result_ignores_unrelated_read_failure(cloud: Cloud) -> None:
    """Read a completed program while another program's status is unavailable."""
    connection = QDMIConnection(base_url=cloud.base, token="token", custom3="project")  # ruff: ignore[hardcoded-password-func-arg] - fixture
    cloud.statuses = dict.fromkeys(cloud.job_ids, "RUNNING")
    results = connection.submit(Sequence.from_abstract_repr(PROGRAMS[0]), job_params=[{"runs": 9}] * 2)
    cloud.statuses["job-z"] = "DONE"
    cloud.errors["/core-fast/api/v2/jobs/job-a"] = 503
    selected = RemoteResults(results.batch_id, connection, [results.job_ids[0]])
    assert selected.results[0].final_bitstrings == cloud.counters["job-z"]
    assert not any(path.endswith("/jobs/job-a") for _, path, _ in cloud.requests)
