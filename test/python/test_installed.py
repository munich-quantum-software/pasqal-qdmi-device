# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Exercise a wheel-installed provider through released Core and official SDK."""

from __future__ import annotations

import gc
import importlib.metadata
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

import pytest
from mqt.core.qdmi import CustomProperty, Job, ProgramFormat, builtin_driver
from pasqal_cloud.authentication import TokenProvider
from pasqal_cloud.endpoints import Endpoints
from pasqal_cloud.pasqal_cloud_client import PasqalCloudClient

from pasqal.qdmi import PASQAL_QDMI_LIBRARY_PATH

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "pasqal_boundary", ROOT / "test/test_pasqal_integration.py"
)
assert _spec is not None and _spec.loader is not None
boundary = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(boundary)
boundary.LIBRARY = PASQAL_QDMI_LIBRARY_PATH


@pytest.fixture
def cloud():
    case = boundary.CloudCase()
    case.setUp()
    yield case
    case.doCleanups()


def open_device(cloud):
    return builtin_driver.open_device(
        "pasqal.emu-free", base_url=cloud.base, token="token", custom3="project"
    )


class LocalToken(TokenProvider):
    def get_token(self):
        return "token"


def test_catalogue_discovery_is_offline_and_from_installed_wheel(tmp_path):
    code = """
import sys
from mqt.core.qdmi import builtin_driver
assert 'pasqal.qdmi' not in sys.modules
assert 'pasqal.emu-free' in builtin_driver.registered_device_ids()
assert 'pasqal.qdmi' not in sys.modules
from pasqal.qdmi import PASQAL_QDMI_LIBRARY_PATH
assert PASQAL_QDMI_LIBRARY_PATH.is_file()
assert 'site-packages' in str(PASQAL_QDMI_LIBRARY_PATH)
"""
    subprocess.run([sys.executable, "-I", "-c", code], cwd=tmp_path, check=True)
    assert importlib.metadata.version("mqt-core") == "4.1.1"


def test_core_sdk_ordering_and_lifetime(cloud):
    device = open_device(cloud)
    assert device.id == "pasqal.emu-free"
    assert device.qubits_num() == 12
    assert device.sites() == device.operations() == []
    assert device.supported_program_formats() == [ProgramFormat.CUSTOM1]
    assert (
        json.loads(device.query_custom_property(CustomProperty.CUSTOM1, str))[0]["device_type"]
        == "FRESNEL"
    )
    job = device.submit_job(boundary.PROGRAMS, ProgramFormat.CUSTOM1, num_shots=9)
    assert job.num_programs == 2
    assert job.wait(timeout=2)
    assert job.check() == Job.Status.DONE
    assert [job.get_counts(i) for i in range(2)] == boundary.EXPECTED
    assert [json.loads(job.get_program(i)) for i in range(2)] == [
        json.loads(p) for p in boundary.PROGRAMS
    ]
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


def test_core_submit_error_does_not_retry(cloud):
    cloud.drop_submission = True
    with pytest.raises(RuntimeError):
        open_device(cloud).submit_job(boundary.PROGRAMS, ProgramFormat.CUSTOM1, num_shots=9)
    assert len(cloud.submissions) == 1


def test_independent_pulser_oracle():
    import numpy as np
    from pulser import Sequence
    from pulser.backend import EmulationConfig
    from pulser.backend.default_observables import Occupation
    from pulser_simulation import QutipBackendV2

    for source, expected in zip(boundary.PROGRAMS, ([1, 0, 0], [0, 0, 1]), strict=True):
        sequence = Sequence.from_abstract_repr(source)
        assert list(sequence.register.qubit_ids) == ["z", "a", "m"]
        result = QutipBackendV2(sequence, config=EmulationConfig(observables=[Occupation()])).run()
        np.testing.assert_allclose(result.get_result("occupation", 1.0), expected, atol=1e-5)
