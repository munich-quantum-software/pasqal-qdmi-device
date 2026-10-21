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

"""Additional transport failure checks using the C ABI and local HTTP only."""

from __future__ import annotations

import ctypes
import json
import unittest
from typing import TYPE_CHECKING
from unittest.mock import patch
from urllib.parse import urlparse

from test_pasqal_integration import PROGRAMS, CloudCase, CloudHandler

if TYPE_CHECKING:
    from collections.abc import Callable


class TransportHTTPTests(CloudCase):
    """Inject malformed responses and transport failures into real HTTP calls."""

    def setUp(self) -> None:
        """Override individual routes while retaining the shared fixture server."""
        super().setUp()
        self.overrides: dict[tuple[str, str], tuple[object, int]] = {}
        for method in ("GET", "POST"):
            original = getattr(CloudHandler, "do_" + method)

            def handler(
                request: CloudHandler,
                original: Callable[[CloudHandler], None] = original,
                method: str = method,
            ) -> None:
                path = urlparse(request.path).path
                override = self.overrides.get((method, path))
                if override is None:
                    return original(request)
                self.requests.append((method, request.path, request.headers.get("Authorization")))
                if method == "POST":
                    request.rfile.read(int(request.headers.get("Content-Length", 0)))
                    if path == "/auth":
                        self.auth_calls += 1
                body, status = override
                data = body if isinstance(body, bytes) else json.dumps(body).encode()
                request.send_response(status)
                request.send_header("Content-Type", "application/json")
                request.send_header("Content-Length", str(len(data)))
                request.end_headers()
                request.wfile.write(data)
                return None

            replacement = patch.object(CloudHandler, "do_" + method, handler)
            replacement.start()
            self.addCleanup(replacement.stop)

    def check(self, job: ctypes.c_void_p) -> int | None:
        """Return a status-query error code without asserting success."""
        return self.api.job_check(job, ctypes.byref(ctypes.c_int()))

    def test_public_device_metadata_needs_no_credentials(self) -> None:
        """Use public metadata without sending a bearer token."""
        self.session(token=None)
        self.assertEqual(self.auth_calls, 0)
        self.assertTrue(all(auth is None for _, _, auth in self.requests))

    def test_public_hardware_specs_select_the_requested_backend(self) -> None:
        """Select matching public hardware constraints without authentication."""
        self.device["device_type"] = "FRESNEL_SA1"
        self.device["is_emulator"] = False
        self.overrides["GET", "/core-fast/api/v1/devices/public-specs"] = (
            {
                "data": [
                    {"device_type": "FRESNEL", "specs": "wrong"},
                    {"device_type": "FRESNEL_SA1", "specs": "selected"},
                ]
            },
            200,
        )
        session = self.session(token=None, backend="FRESNEL_SA1")
        size = ctypes.c_size_t()
        self.assertEqual(
            self.api.session_query_device_property(session, 999999995, 0, None, ctypes.byref(size)),
            0,
        )
        specs = ctypes.create_string_buffer(size.value)
        self.assertEqual(
            self.api.session_query_device_property(session, 999999995, size.value, specs, None),
            0,
        )
        self.assertEqual(json.loads(specs.value), [{"device_type": "FRESNEL_SA1", "specs": "selected"}])
        self.assertEqual(self.auth_calls, 0)
        self.assertTrue(all(auth is None for _, _, auth in self.requests))

    def test_missing_public_specs_do_not_block_backend_metadata(self) -> None:
        """Keep backend metadata usable when public pulse constraints are absent."""
        self.device["device_type"] = "PRIVATE_QPU"
        self.device["is_emulator"] = False
        self.session(token=None, backend="PRIVATE_QPU")
        self.assertFalse(any("/devices/specs/" in path for _, path, _ in self.requests))

    def test_missing_public_device_uses_authenticated_metadata(self) -> None:
        """Fall back to account metadata when the public catalogue has no match."""
        self.overrides["GET", "/core-fast/api/v1/devices"] = ({"data": []}, 200)
        self.session()
        self.assertIn(
            ("GET", "/core-fast/api/v2/devices?device_type=EMU_FREE", "Bearer token"),
            self.requests,
        )

    def test_public_metadata_does_not_authorize_submission(self) -> None:
        """Require execution credentials even after successful public discovery."""
        job = self.job(self.session(token=None))
        self.assertEqual(self.api.job_submit(job), -8)
        self.assertEqual(self.submissions, [])

    def test_persistent_get_401_stops_after_one_refresh(self) -> None:
        """Stop after one refresh when a safe read still rejects authentication."""
        job = self.job(self.session(credentials=True))
        self.assertEqual(self.api.job_submit(job), 0)
        path = "/core-fast/api/v2/batches/batch-1"
        self.errors[path] = 401
        self.assertEqual(self.check(job), -8)
        self.assertEqual(self.auth_calls, 1)
        self.assertEqual(sum(request[1] == path for request in self.requests), 2)
        self.assertEqual(len(self.submissions), 1)

    def test_rejected_refresh_is_contained_at_c_boundary(self) -> None:
        """Convert refresh failures into a QDMI error without leaking exceptions."""
        job = self.job(self.session(credentials=True))
        self.assertEqual(self.api.job_submit(job), 0)
        self.reject_bearer = self.auth_error = True
        self.assertEqual(self.check(job), -8)
        self.assertEqual(self.auth_calls, 1)

    def test_post_401_is_not_retried_or_reauthenticated(self) -> None:
        """Do not retry a submission rejected by the service."""
        job = self.job(self.session(credentials=True))
        self.submit_status = 401
        self.assertEqual(self.api.job_submit(job), -8)
        self.assertEqual(self.api.job_submit(job), -10)
        self.assertEqual(self.auth_calls, 0)
        self.assertEqual(len(self.submissions), 1)

    def test_malformed_auth_responses_prevent_submission(self) -> None:
        """Reject malformed tokens and expiry values before submitting jobs."""
        for body in (
            b'{"access_token":"secret-SENTINEL"',
            {"access_token": 7},
            {"access_token": ""},
            {"access_token": "secret-SENTINEL\r\nInjected: true"},
            {"access_token": "opaque", "expires_in": -1},
            {"access_token": "opaque", "expires_in": True},
        ):
            with self.subTest(body=body):
                self.overrides["POST", "/auth"] = (body, 200)
                job = self.job(self.session(token=None, credentials=True))
                self.assertNotEqual(self.api.job_submit(job), 0)
                self.assertEqual(self.submissions, [])

    def test_unknown_expiry_is_not_cached_forever(self) -> None:
        """Refresh a token whose validity interval is unspecified."""
        self.overrides["POST", "/auth"] = ({"access_token": "opaque"}, 200)
        job = self.job(self.session(token=None, credentials=True))
        self.assertEqual(self.api.job_submit(job), 0)
        before = self.auth_calls
        self.assertEqual(self.status(job), 4)
        self.assertGreater(self.auth_calls, before)

    def test_raw_job_recovers_shared_sequence_without_expanding_batch(self) -> None:
        """Retrieve one legacy job using its batch-level shared sequence."""
        self.overrides["GET", "/core-fast/api/v2/jobs/job-z"] = (
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
        self.overrides["GET", "/core-fast/api/v2/batches/batch-1"] = (
            {"data": {"id": "batch-1", "job_ids": self.job_ids, "sequence_builder": PROGRAMS[0]}},
            200,
        )
        job = self.retrieve(self.session(), "job-z")
        size = ctypes.c_size_t()
        self.assertEqual(self.api.job_get_program(job, 0, 0, None, ctypes.byref(size)), 0)
        source = ctypes.create_string_buffer(size.value)
        self.assertEqual(self.api.job_get_program(job, 0, size.value, source, None), 0)
        self.assertEqual(source.value.decode(), PROGRAMS[0])
        count = ctypes.c_size_t()
        self.assertEqual(self.api.job_query_property(job, 5, ctypes.sizeof(count), ctypes.byref(count), None), 0)
        self.assertEqual(count.value, 1)
        self.assertEqual(self.job_id(job), "job-z")
        self.assertEqual(self.counts(job, 0), {"001": 7, "100": 2})

    def test_raw_job_shared_sequence_rejects_wrong_counter_width(self) -> None:
        """Validate recovered result width against the shared register."""
        self.overrides["GET", "/core-fast/api/v2/jobs/job-z"] = (
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
        self.overrides["GET", "/core-fast/api/v2/batches/batch-1"] = (
            {"data": {"id": "batch-1", "job_ids": self.job_ids, "sequence_builder": PROGRAMS[0]}},
            200,
        )
        self.counters["job-z"] = {"10": 9}
        job = ctypes.c_void_p()
        code = self.api.session_retrieve_device_job_by_id(self.session(), b"job-z", ctypes.byref(job))
        if job.value:
            self.addCleanup(self.api.job_free, job)
        self.assertNotEqual(code, 0)
        self.assertIsNone(job.value)

    def test_batch_queue_position_is_not_presented_as_jobs_ahead(self) -> None:
        """Leave the output untouched when queue-position semantics are unknown."""
        self.position = 7
        self.statuses = dict.fromkeys(self.job_ids, "PENDING")
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        position = ctypes.c_size_t(99)
        self.assertEqual(self.api.job_query_property(job, 4, ctypes.sizeof(position), ctypes.byref(position), None), -9)
        self.assertEqual(position.value, 99)

    def test_signed_download_rejects_other_protocols_and_bad_json(self) -> None:
        """Reject unsafe result URLs and malformed downloads, then allow recovery."""
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        link_path = "/core-fast/api/v1/jobs/job-z/results_link"
        self.overrides["GET", link_path] = ({"data": {"results_link": "file:///etc/hosts"}}, 200)
        self.assertNotEqual(self.api.job_get_results(job, 0, 1, 0, None, None), 0)
        self.overrides.clear()
        self.overrides["GET", "/results/job-z"] = (b'{"counter": {"100": 9', 200)
        self.assertNotEqual(self.api.job_get_results(job, 0, 1, 0, None, None), 0)
        self.overrides.clear()
        self.assertEqual(self.counts(job, 0), {"001": 7, "100": 2})

    def test_canceling_requires_terminal_confirmation(self) -> None:
        """Report cancellation only after a terminal provider status."""
        self.statuses = dict.fromkeys(self.job_ids, "RUNNING")
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        self.assertEqual(self.api.job_cancel(job), -1)
        self.assertNotEqual(self.status(job), 5)
        self.statuses = dict.fromkeys(self.job_ids, "CANCELED")
        self.assertEqual(self.status(job), 5)
        self.assertEqual(self.cancellations, ["/core-fast/api/v2/batches/batch-1/cancel"])

    def test_absent_queue_capability_differs_from_malformed_queue(self) -> None:
        """Distinguish missing queue support from invalid published job counts."""
        session = self.session()
        path = "/core-fast/api/v1/devices/EMU_FREE/queue-size"
        count = ctypes.c_size_t()
        for body in ({"data": None}, {"data": {}}, {"data": {"device_queues": None}}):
            with self.subTest(body=body):
                self.overrides["GET", path] = (body, 200)
                self.assertEqual(
                    self.api.session_query_device_property(
                        session, 17, ctypes.sizeof(count), ctypes.byref(count), None
                    ),
                    -9,
                )
        for queues in ([{"number_of_jobs": -1}], [{"number_of_jobs": 2**64}]):
            with self.subTest(queues=queues):
                self.overrides["GET", path] = ({"data": {"device_queues": queues}}, 200)
                self.assertNotEqual(
                    self.api.session_query_device_property(
                        session, 17, ctypes.sizeof(count), ctypes.byref(count), None
                    ),
                    0,
                )


if __name__ == "__main__":
    unittest.main()
