# Copyright (c) 2026 Munich Quantum Software Company GmbH
# Copyright (c) 2025 - 2026 Pasqal SAS
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

import gc
import json
import subprocess
import sys
from typing import TYPE_CHECKING

import numpy as np
import pytest
import test_pasqal_integration as boundary
from mqt.core.qdmi import CustomProperty, Job, ProgramFormat, builtin_driver
from pasqal_cloud.authentication import TokenProvider
from pasqal_cloud.endpoints import Endpoints
from pasqal_cloud.pasqal_cloud_client import PasqalCloudClient
from pulser import Sequence
from pulser.backend import EmulationConfig
from pulser.backend.default_observables import Occupation
from pulser_simulation import QutipBackendV2

from pasqal.qdmi import PASQAL_QDMI_LIBRARY_PATH

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path

    from mqt.core.qdmi import Device

boundary.LIBRARY = PASQAL_QDMI_LIBRARY_PATH


@pytest.fixture
def cloud() -> Iterator[boundary.CloudCase]:
    """Yield deterministic Pasqal responses for a single test."""
    case = boundary.CloudCase()
    case.setUp()
    yield case
    case.doCleanups()


def open_device(cloud: boundary.CloudCase) -> Device:
    """Return the installed provider connected to the local fixture server."""
    return builtin_driver.open_device(
        "pasqal.emu-free",
        base_url=cloud.base,
        token="token",  # ruff: ignore[hardcoded-password-func-arg] - local fixture credential
        custom3="project",
    )


class LocalToken(TokenProvider):
    """Use a deterministic token with the SDK's local fixture endpoint."""

    def get_token(self) -> str:  # ruff: ignore[no-self-use] - SDK interface override
        """Return the fixture token without authentication."""
        return "token"


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
for device_id in expected:
    assert builtin_driver.open_device(device_id, custom1='MOCK').qubits_num() > 0
assert 'pasqal.qdmi' not in sys.modules
from pasqal.qdmi import PASQAL_QDMI_LIBRARY_PATH
assert PASQAL_QDMI_LIBRARY_PATH.is_file()
assert 'site-packages' in str(PASQAL_QDMI_LIBRARY_PATH)
"""
    subprocess.run([sys.executable, "-I", "-c", code], cwd=tmp_path, check=True)  # ruff: ignore[subprocess-without-shell-equals-true] - fixed interpreter and source


def test_core_sdk_ordering_and_lifetime(cloud: boundary.CloudCase) -> None:
    """Match SDK counts while preserving program order and Core ownership."""
    device = open_device(cloud)
    assert device.id == "pasqal.emu-free"
    assert device.qubits_num() == 12
    assert device.sites() == device.operations() == []
    assert device.supported_program_formats() == [ProgramFormat.CUSTOM1]
    specs = device.query_custom_property(CustomProperty.CUSTOM1, str)
    assert specs is not None
    assert json.loads(specs)[0]["device_type"] == "FRESNEL"
    job = device.submit_job(boundary.PROGRAMS, ProgramFormat.CUSTOM1, num_shots=9)
    assert job.num_programs == 2
    assert job.wait(timeout=2)
    assert job.check() == Job.Status.DONE
    assert [job.get_counts(i) for i in range(2)] == boundary.EXPECTED
    assert [json.loads(job.get_program(i)) for i in range(2)] == [json.loads(p) for p in boundary.PROGRAMS]
    saved_id = job.id
    del device
    gc.collect()
    assert job.get_counts(0) == boundary.EXPECTED[0]
    restored = open_device(cloud).retrieve_job_by_id(saved_id)
    assert [restored.get_counts(i) for i in range(2)] == boundary.EXPECTED
    assert len(cloud.submissions) == 1
    sdk = PasqalCloudClient(
        project_id="project",
        token_provider=LocalToken(),
        endpoints=Endpoints(core=cloud.base + "/core-fast", account=cloud.base + "/account"),
    )
    for i, cloud_id in enumerate(cloud.job_ids):
        official = sdk.get_job(cloud_id).result
        assert official is not None
        assert {bits[::-1]: count for bits, count in official.items()} == restored.get_counts(i)


def test_core_submit_error_does_not_retry(cloud: boundary.CloudCase) -> None:
    """A lost submission response must not create a second remote batch."""
    cloud.drop_submission = True
    with pytest.raises(RuntimeError):
        open_device(cloud).submit_job(boundary.PROGRAMS, ProgramFormat.CUSTOM1, num_shots=9)
    assert len(cloud.submissions) == 1


def test_independent_pulser_oracle() -> None:
    """Verify fixture atom order independently with Pulser simulation."""
    for source, expected in zip(boundary.PROGRAMS, ([1, 0, 0], [0, 0, 1]), strict=True):
        sequence = Sequence.from_abstract_repr(source)
        assert list(sequence.register.qubit_ids) == ["z", "a", "m"]
        result = QutipBackendV2(sequence, config=EmulationConfig(observables=[Occupation()])).run()
        np.testing.assert_allclose(result.get_result("occupation", 1.0), expected, atol=1e-5)
