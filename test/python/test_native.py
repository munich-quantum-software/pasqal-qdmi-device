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

"""Verify native metadata, ordering, ownership and recovery contracts."""

from __future__ import annotations

import ctypes
import time

import pytest
from cloud import EXPECTED, PROGRAMS, Cloud


def test_backend_is_resolved_from_cloud_metadata(cloud: Cloud) -> None:
    """Resolve the configured backend name before requesting metadata."""
    cloud.device["device_type"] = "EMU_TN"
    session = cloud.session(backend="emu_tn")
    capacity = ctypes.c_size_t()
    assert (
        cloud.api.session_query_device_property(session, 4, ctypes.sizeof(capacity), ctypes.byref(capacity), None) == 0
    )
    assert capacity.value == cloud.device["max_atom_num"]
    assert ("GET", "/core-fast/api/v1/devices?device_type=EMU_TN", None) in cloud.requests


def test_capacity_is_backend_not_first_qpu(cloud: Cloud) -> None:
    """Read capacity from the selected backend and expose no invented sites."""
    session = cloud.session()
    capacity = ctypes.c_size_t()
    assert (
        cloud.api.session_query_device_property(session, 4, ctypes.sizeof(capacity), ctypes.byref(capacity), None) == 0
    )
    assert capacity.value == 12
    for prop in (5, 6):
        size = ctypes.c_size_t(99)
        assert cloud.api.session_query_device_property(session, prop, 0, None, ctypes.byref(size)) == 0
        assert size.value == 0


def test_one_batch_preserves_order_and_fresh_retrieval(cloud: Cloud) -> None:
    """Preserve submission order across batching and fresh-session retrieval."""
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    assert len(cloud.submissions) == 1
    assert cloud.submissions[0]["jobs"] == [{"sequence": p, "runs": 9} for p in PROGRAMS]
    assert cloud.submissions[0]["sequence_builder"] is None
    assert not cloud.submissions[0]["open"]
    assert [cloud.counts(job, i) for i in range(2)] == EXPECTED
    fresh = cloud.retrieve(cloud.session(), cloud.job_id(job))
    assert [cloud.counts(fresh, i) for i in range(2)] == EXPECTED
    assert cloud.api.job_submit(fresh) == -10
    assert len(cloud.submissions) == 1
    downloads = [auth for _, path, auth in cloud.requests if path.startswith("/results/")]
    assert downloads
    assert set(downloads) == {None}


def test_accepted_batch_survives_followup_error(cloud: Cloud) -> None:
    """Retain an accepted batch ID when the first status read fails."""
    job = cloud.job(cloud.session())
    cloud.errors["/core-fast/api/v2/batches/batch-1"] = 503
    assert cloud.api.job_submit(job) == 0
    assert "batch-1" in cloud.job_id(job)
    assert cloud.api.job_check(job, ctypes.byref(ctypes.c_int())) != 0
    cloud.errors.clear()
    assert cloud.status(job) == 4
    assert len(cloud.submissions) == 1


def test_lost_submission_response_is_never_retried(cloud: Cloud) -> None:
    """Reject resubmission after an ambiguous connection loss."""
    job = cloud.job(cloud.session())
    cloud.drop_submission = True
    assert cloud.api.job_submit(job) != 0
    assert cloud.api.job_submit(job) == -10
    assert len(cloud.submissions) == 1


@pytest.mark.parametrize("response", [{}, {"id": ""}, {"id": "../invalid"}])
def test_invalid_post_response_is_not_retried(cloud: Cloud, response: dict[str, str]) -> None:
    """Reject resubmission when the returned batch identifier is invalid."""
    job = cloud.job(cloud.session())
    cloud.submit_response = response
    before = len(cloud.submissions)
    assert cloud.api.job_submit(job) != 0
    assert cloud.api.job_submit(job) == -10
    assert len(cloud.submissions) == before + 1


def test_partial_batch_metadata_cannot_swap_or_omit_programs(cloud: Cloud) -> None:
    """Reject incomplete metadata instead of changing result association."""
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    cloud.job_ids = ["job-z"]
    assert cloud.api.job_check(job, ctypes.byref(ctypes.c_int())) != 0


def test_successful_sibling_remains_accessible(cloud: Cloud) -> None:
    """Keep completed results available when another program fails."""
    cloud.statuses["job-a"] = "ERROR"
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    assert cloud.status(job) == 6
    assert cloud.counts(job, 0) == EXPECTED[0]
    assert cloud.api.job_get_results(job, 1, 1, 0, None, None) == -10


@pytest.mark.parametrize(
    "counter",
    [
        {"100": -1},
        {"100": 1.5},
        {"100": True},
        {"100": 2**64},
        {"10": 9},
        {"x00": 9},
        {"100": 8},
        {},
        {"100": 9, "001": 1},
    ],
)
def test_malformed_counts_fail_and_can_be_refetched(cloud: Cloud, counter: object) -> None:
    """Reject invalid histogram values without poisoning subsequent reads."""
    cloud.counters["job-z"] = counter
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    assert cloud.api.job_get_results(job, 0, 1, 0, None, None) != 0
    cloud.counters["job-z"] = {"100": 7, "001": 2}
    assert cloud.counts(job, 0) == EXPECTED[0]


def test_canceling_is_not_canceled_and_completion_can_win(cloud: Cloud) -> None:
    """Require cancellation confirmation and retain completed results."""
    cloud.statuses = dict.fromkeys(cloud.job_ids, "RUNNING")
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    assert cloud.api.job_cancel(job) == -1
    assert cloud.status(job) != 5
    cloud.statuses = dict.fromkeys(cloud.job_ids, "DONE")
    assert cloud.status(job) == 4
    assert cloud.counts(job, 0) == EXPECTED[0]


def test_deadline_bounds_slow_http(cloud: Cloud) -> None:
    """Apply the wait deadline to a delayed HTTP response."""
    cloud.statuses = dict.fromkeys(cloud.job_ids, "RUNNING")
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    cloud.status_delay = 2
    start = time.monotonic()
    assert cloud.api.job_wait(job, 1) == -11
    assert time.monotonic() - start < 1.5


def test_expired_bearer_refreshes_only_once(cloud: Cloud) -> None:
    """Refresh an expired bearer once without duplicating the submission."""
    cloud.reject_bearer = True
    job = cloud.job(cloud.session(credentials=True))
    assert cloud.api.job_submit(job) == 0
    assert cloud.status(job) == 4
    assert cloud.auth_calls == 1
    assert len(cloud.submissions) == 1


def test_opaque_token_expiry_is_honored(cloud: Cloud) -> None:
    """Refresh opaque tokens when their declared lifetime elapses."""
    cloud.expires_in = 1
    job = cloud.job(cloud.session(token=None, credentials=True))
    assert cloud.api.job_submit(job) == 0
    before = cloud.auth_calls
    time.sleep(1.05)
    assert cloud.status(job) == 4
    assert cloud.auth_calls > before


def test_queue_length_sums_jobs_not_shots(cloud: Cloud) -> None:
    """Sum waiting jobs across provider queue categories."""
    session = cloud.session()
    count = ctypes.c_size_t()
    assert cloud.api.session_query_device_property(session, 17, ctypes.sizeof(count), ctypes.byref(count), None) == 0
    assert count.value == 5


@pytest.mark.parametrize("result", [0, 3, 4])
def test_unsupported_results_do_not_fabricate_shots(cloud: Cloud, result: int) -> None:
    """Reject unavailable chronological samples and probability results."""
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    assert cloud.api.job_get_results(job, 0, result, 0, None, None) == -9
