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
import json
import time
from typing import TYPE_CHECKING
from urllib.parse import parse_qs, urlparse

import pytest
from cloud import EXPECTED, PROGRAMS, Cloud, CloudHandler

if TYPE_CHECKING:
    from collections.abc import Callable
    from pathlib import Path


@pytest.fixture
def responses(cloud: Cloud, monkeypatch: pytest.MonkeyPatch) -> dict[tuple[str, str], tuple[object, int]]:
    """Return response overrides for the shared fixture server."""
    overrides: dict[tuple[str, str], tuple[object, int]] = {}
    for method in ("GET", "POST"):
        original = getattr(CloudHandler, "do_" + method)

        def handler(
            request: CloudHandler,
            original: Callable[[CloudHandler], None] = original,
            method: str = method,
        ) -> None:
            path = urlparse(request.path).path
            override = overrides.get((method, path))
            if override is None:
                return original(request)
            cloud.requests.append((method, request.path, request.headers.get("Authorization")))
            if method == "POST":
                request.rfile.read(int(request.headers.get("Content-Length", 0)))
                if path == "/auth":
                    cloud.auth_calls += 1
            body, status = override
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
            request.send_response(status)
            request.send_header("Content-Type", "application/json")
            request.send_header("Content-Length", str(len(data)))
            request.end_headers()
            request.wfile.write(data)
            return None

        monkeypatch.setattr(CloudHandler, "do_" + method, handler)

    return overrides


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


@pytest.mark.parametrize(("terminal_status", "qdmi_status"), [("DONE", 4), ("CANCELED", 5)])
def test_canceling_requires_terminal_confirmation(cloud: Cloud, terminal_status: str, qdmi_status: int) -> None:
    """Require cancellation confirmation and retain results if completion wins."""
    cloud.statuses = dict.fromkeys(cloud.job_ids, "RUNNING")
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    assert cloud.api.job_cancel(job) == -1
    assert cloud.status(job) != 5
    cloud.statuses = dict.fromkeys(cloud.job_ids, terminal_status)
    assert cloud.status(job) == qdmi_status
    assert cloud.cancellations == ["/core-fast/api/v2/batches/batch-1/cancel"]
    if terminal_status == "DONE":
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


def test_public_device_metadata_needs_no_credentials(cloud: Cloud) -> None:
    """Use public metadata without sending a bearer token."""
    cloud.session(token=None)
    assert cloud.auth_calls == 0
    assert all(auth is None for _, _, auth in cloud.requests)


@pytest.mark.parametrize("source", ["authfile", "environment", "session"])
def test_credentials_precedence(cloud: Cloud, monkeypatch: pytest.MonkeyPatch, tmp_path: Path, source: str) -> None:
    """Prefer explicit session credentials, then nonempty environment, then file."""
    authfile = tmp_path / "auth"
    authfile.write_text("username=authfile\npassword=authfile\nproject_id=authfile\n")
    monkeypatch.setenv("PASQAL_AUTHFILE", str(authfile))
    for name in ("PASQAL_EMAIL", "PASQAL_PASSWORD", "PASQAL_PROJECT_ID"):
        monkeypatch.setenv(name, "" if source == "authfile" else "environment")

    auth_payloads: list[dict[str, list[str]]] = []
    original = CloudHandler.do_POST

    def capture_auth(request: CloudHandler) -> None:
        if request.path == "/auth":
            data = request.rfile.read(int(request.headers.get("Content-Length", 0)))
            auth_payloads.append(parse_qs(data.decode()))
            request.reply({"access_token": "fixture", "expires_in": 3600})
        else:
            original(request)

    monkeypatch.setattr(CloudHandler, "do_POST", capture_auth)
    session = ctypes.c_void_p()
    assert cloud.api.session_alloc(ctypes.byref(session)) == 0
    cloud.cleanup.callback(cloud.api.session_free, session)
    params = [(0, cloud.base), (3, cloud.base + "/auth"), (999999995, "EMU_FREE")]
    if source == "session":
        params += [(4, source), (5, source), (999999997, source)]
    for param, value in params:
        data = value.encode() + b"\0"
        assert cloud.api.session_set_parameter(session, param, len(data), ctypes.c_char_p(data)) == 0
    assert cloud.api.session_init(session) == 0
    assert cloud.api.job_submit(cloud.job(session)) == 0
    assert auth_payloads[0]["username"] == [source]
    assert auth_payloads[0]["password"] == [source]
    assert cloud.submissions[0]["project_id"] == source


def test_public_hardware_specs_select_the_requested_backend(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]]
) -> None:
    """Select matching public hardware constraints without authentication."""
    cloud.device["device_type"] = "FRESNEL_SA1"
    cloud.device["is_emulator"] = False
    responses["GET", "/core-fast/api/v1/devices/public-specs"] = (
        {
            "data": [
                {"device_type": "FRESNEL", "specs": "wrong"},
                {"device_type": "FRESNEL_SA1", "specs": "selected"},
            ]
        },
        200,
    )
    session = cloud.session(token=None, backend="FRESNEL_SA1")
    size = ctypes.c_size_t()
    assert cloud.api.session_query_device_property(session, 999999995, 0, None, ctypes.byref(size)) == 0
    specs = ctypes.create_string_buffer(size.value)
    assert cloud.api.session_query_device_property(session, 999999995, size.value, specs, None) == 0
    assert json.loads(specs.value) == [{"device_type": "FRESNEL_SA1", "specs": "selected"}]
    assert cloud.auth_calls == 0
    assert all(auth is None for _, _, auth in cloud.requests)


def test_missing_public_specs_do_not_block_backend_metadata(cloud: Cloud) -> None:
    """Keep backend metadata usable when public pulse constraints are absent."""
    cloud.device["device_type"] = "PRIVATE_QPU"
    cloud.device["is_emulator"] = False
    cloud.session(token=None, backend="PRIVATE_QPU")
    assert not any("/devices/specs/" in path for _, path, _ in cloud.requests)


def test_missing_public_device_uses_authenticated_metadata(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]]
) -> None:
    """Fall back to account metadata when the public catalogue has no match."""
    responses["GET", "/core-fast/api/v1/devices"] = ({"data": []}, 200)
    cloud.session()
    assert ("GET", "/core-fast/api/v2/devices?device_type=EMU_FREE", "Bearer token") in cloud.requests


def test_public_metadata_does_not_authorize_submission(cloud: Cloud) -> None:
    """Require execution credentials even after successful public discovery."""
    job = cloud.job(cloud.session(token=None))
    assert cloud.api.job_submit(job) == -8
    assert cloud.submissions == []


def test_persistent_get_401_stops_after_one_refresh(cloud: Cloud) -> None:
    """Stop after one refresh when a safe read still rejects authentication."""
    job = cloud.job(cloud.session(credentials=True))
    assert cloud.api.job_submit(job) == 0
    path = "/core-fast/api/v2/batches/batch-1"
    cloud.errors[path] = 401
    assert cloud.api.job_check(job, ctypes.byref(ctypes.c_int())) == -8
    assert cloud.auth_calls == 1
    assert sum(request[1] == path for request in cloud.requests) == 2
    assert len(cloud.submissions) == 1


def test_rejected_refresh_is_contained_at_c_boundary(cloud: Cloud) -> None:
    """Convert refresh failures into a QDMI error without leaking exceptions."""
    job = cloud.job(cloud.session(credentials=True))
    assert cloud.api.job_submit(job) == 0
    cloud.reject_bearer = cloud.auth_error = True
    assert cloud.api.job_check(job, ctypes.byref(ctypes.c_int())) == -8
    assert cloud.auth_calls == 1


def test_post_401_is_not_retried_or_reauthenticated(cloud: Cloud) -> None:
    """Do not retry a submission rejected by the service."""
    job = cloud.job(cloud.session(credentials=True))
    cloud.submit_status = 401
    assert cloud.api.job_submit(job) == -8
    assert cloud.api.job_submit(job) == -10
    assert cloud.auth_calls == 0
    assert len(cloud.submissions) == 1


@pytest.mark.parametrize(
    "body",
    [
        b'{"access_token":"secret-SENTINEL"',
        {"access_token": 7},
        {"access_token": ""},
        {"access_token": "secret-SENTINEL\r\nInjected: true"},
        {"access_token": "opaque", "expires_in": -1},
        {"access_token": "opaque", "expires_in": True},
    ],
)
def test_malformed_auth_responses_prevent_submission(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]], body: object
) -> None:
    """Reject malformed tokens and expiry values before submitting jobs."""
    responses["POST", "/auth"] = (body, 200)
    job = cloud.job(cloud.session(token=None, credentials=True))
    assert cloud.api.job_submit(job) != 0
    assert cloud.submissions == []


def test_unknown_expiry_is_not_cached_forever(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]]
) -> None:
    """Refresh a token whose validity interval is unspecified."""
    responses["POST", "/auth"] = ({"access_token": "opaque"}, 200)
    job = cloud.job(cloud.session(token=None, credentials=True))
    assert cloud.api.job_submit(job) == 0
    before = cloud.auth_calls
    assert cloud.status(job) == 4
    assert cloud.auth_calls > before


def test_raw_job_recovers_shared_sequence_without_expanding_batch(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]]
) -> None:
    """Retrieve one legacy job using its batch-level shared sequence."""
    responses["GET", "/core-fast/api/v2/jobs/job-z"] = (
        {
            "data": {
                "id": "job-z",
                "batch_id": "batch-1",
                "sequence": None,
                "runs": 9,
                "status": "DONE",
            }
        },
        200,
    )
    responses["GET", "/core-fast/api/v2/batches/batch-1"] = (
        {"data": {"id": "batch-1", "job_ids": cloud.job_ids, "sequence_builder": PROGRAMS[0]}},
        200,
    )
    job = cloud.retrieve(cloud.session(), "job-z")
    size = ctypes.c_size_t()
    assert cloud.api.job_get_program(job, 0, 0, None, ctypes.byref(size)) == 0
    source = ctypes.create_string_buffer(size.value)
    assert cloud.api.job_get_program(job, 0, size.value, source, None) == 0
    assert source.value.decode() == PROGRAMS[0]
    count = ctypes.c_size_t()
    assert cloud.api.job_query_property(job, 5, ctypes.sizeof(count), ctypes.byref(count), None) == 0
    assert count.value == 1
    assert cloud.job_id(job) == "job-z"
    assert cloud.counts(job, 0) == {"001": 7, "100": 2}


def test_raw_job_shared_sequence_rejects_wrong_counter_width(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]]
) -> None:
    """Validate recovered result width against the shared register."""
    responses["GET", "/core-fast/api/v2/jobs/job-z"] = (
        {
            "data": {
                "id": "job-z",
                "batch_id": "batch-1",
                "sequence": None,
                "runs": 9,
                "status": "DONE",
            }
        },
        200,
    )
    responses["GET", "/core-fast/api/v2/batches/batch-1"] = (
        {"data": {"id": "batch-1", "job_ids": cloud.job_ids, "sequence_builder": PROGRAMS[0]}},
        200,
    )
    cloud.counters["job-z"] = {"10": 9}
    job = ctypes.c_void_p()
    code = cloud.api.session_retrieve_device_job_by_id(cloud.session(), b"job-z", ctypes.byref(job))
    if job.value:
        cloud.cleanup.callback(cloud.api.job_free, job)
    assert code != 0
    assert job.value is None


def test_batch_queue_position_is_not_presented_as_jobs_ahead(cloud: Cloud) -> None:
    """Leave the output untouched when queue-position semantics are unknown."""
    cloud.position = 7
    cloud.statuses = dict.fromkeys(cloud.job_ids, "PENDING")
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    position = ctypes.c_size_t(99)
    assert cloud.api.job_query_property(job, 4, ctypes.sizeof(position), ctypes.byref(position), None) == -9
    assert position.value == 99


def test_signed_download_rejects_other_protocols_and_bad_json(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]]
) -> None:
    """Reject unsafe result URLs and malformed downloads, then allow recovery."""
    job = cloud.job(cloud.session())
    assert cloud.api.job_submit(job) == 0
    link_path = "/core-fast/api/v1/jobs/job-z/results_link"
    responses["GET", link_path] = ({"data": {"results_link": "file:///etc/hosts"}}, 200)
    assert cloud.api.job_get_results(job, 0, 1, 0, None, None) != 0
    responses.clear()
    responses["GET", "/results/job-z"] = (b'{"counter": {"100": 9', 200)
    assert cloud.api.job_get_results(job, 0, 1, 0, None, None) != 0
    responses.clear()
    assert cloud.counts(job, 0) == {"001": 7, "100": 2}


@pytest.mark.parametrize("body", [{"data": None}, {"data": {}}, {"data": {"device_queues": None}}])
def test_absent_queue_capability(
    cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]], body: object
) -> None:
    """Report absent queue metadata as an unsupported capability."""
    session = cloud.session()
    responses["GET", "/core-fast/api/v1/devices/EMU_FREE/queue-size"] = (body, 200)
    count = ctypes.c_size_t()
    assert cloud.api.session_query_device_property(session, 17, ctypes.sizeof(count), ctypes.byref(count), None) == -9


@pytest.mark.parametrize("count", [-1, 2**64])
def test_malformed_queue_count(cloud: Cloud, responses: dict[tuple[str, str], tuple[object, int]], count: int) -> None:
    """Reject invalid published queue lengths."""
    session = cloud.session()
    responses["GET", "/core-fast/api/v1/devices/EMU_FREE/queue-size"] = (
        {"data": {"device_queues": [{"number_of_jobs": count}]}},
        200,
    )
    result = ctypes.c_size_t()
    assert cloud.api.session_query_device_property(session, 17, ctypes.sizeof(result), ctypes.byref(result), None) != 0
