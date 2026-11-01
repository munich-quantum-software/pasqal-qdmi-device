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

"""Execute the installed provider on Pasqal's free cloud emulator."""

from __future__ import annotations

import gc
import json
import os
from functools import partialmethod

import pytest
import requests
from cloud import PROGRAMS
from mqt.core.qdmi import CustomProperty, Job, ProgramFormat, builtin_driver
from pasqal_cloud.pasqal_cloud_client import PasqalCloudClient


@pytest.mark.integration
def test_cloud_submission_and_retrieval(monkeypatch: pytest.MonkeyPatch) -> None:
    """Check real batching, atom order, ownership and fresh-session SDK parity."""
    device = builtin_driver.open_device("pasqal.emu-free")
    metadata_json = device.query_custom_property(CustomProperty.CUSTOM2, str)
    assert metadata_json is not None
    metadata = json.loads(metadata_json)
    assert metadata["device_type"] == "EMU_FREE"
    assert metadata["is_emulator"] is True
    assert metadata["bill_usage"] is False
    assert device.qubits_num() == metadata["max_atom_num"]
    assert device.qubits_num() >= 3
    assert len(device.sites()) == device.qubits_num()
    assert [site.index() for site in device.regular_sites()] == list(range(device.qubits_num()))
    assert device.operations() == []
    assert device.supported_program_formats() == [ProgramFormat.CUSTOM1]
    assert device.query_custom_property(CustomProperty.CUSTOM1, str)

    job = device.submit_job(PROGRAMS, ProgramFormat.CUSTOM1, num_shots=100)
    print(f"EMU_FREE batch: {job.id}", flush=True)
    assert job.wait(timeout=300), f"Retrieve pending job {job.id}; do not resubmit"
    assert job.check() == Job.Status.DONE
    assert job.num_programs == 2
    counts = [job.get_counts(i) for i in range(2)]
    for actual, expected in zip(counts, ("001", "100"), strict=True):
        assert sum(actual.values()) == 100
        # Isolated resonant pi pulses; the independent Pulser test checks occupations.
        assert actual.get(expected, 0) >= 98
    del device
    gc.collect()
    assert job.get_counts(0) == counts[0]

    restored = builtin_driver.open_device("pasqal.emu-free").retrieve_job_by_id(job.id)
    assert [restored.get_counts(i) for i in range(2)] == counts
    assert [json.loads(restored.get_program(i)) for i in range(2)] == [json.loads(p) for p in PROGRAMS]
    # Bound SDK authentication and result downloads, which currently omit timeouts.
    monkeypatch.setattr(requests.Session, "request", partialmethod(requests.Session.request, timeout=30))
    sdk = PasqalCloudClient(
        username=os.environ["PASQAL_EMAIL"],
        password=os.environ["PASQAL_PASSWORD"],
        project_id=os.environ["PASQAL_PROJECT_ID"],
    )
    official = sdk.get_batch(job.id.removeprefix("pasqal-batch:"))
    official_counts = []
    for result in official.ordered_jobs:
        counter = result.result
        assert counter is not None
        official_counts.append({bits[::-1]: count for bits, count in counter.items()})
    assert official_counts == counts
