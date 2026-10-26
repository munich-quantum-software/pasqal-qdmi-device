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

"""Pulser remote connection that runs sequences through the Pasqal QDMI device."""

from __future__ import annotations

import json
from typing import TYPE_CHECKING, Any, cast

from mqt.core.qdmi import CustomProperty, Job, ProgramFormat, builtin_driver
from pulser import Sequence
from pulser.backend.remote import (
    BatchStatus,
    JobParams,
    JobStatus,
    RemoteConnection,
    RemoteResults,
    RemoteResultsError,
)
from pulser.backend.results import Results
from pulser.json.abstract_repr.deserializer import deserialize_device

if TYPE_CHECKING:
    from collections.abc import Mapping

    from pulser.devices import Device

__all__ = ["QDMIConnection"]

JOB_STATUS = {
    Job.Status.CREATED: JobStatus.PENDING,
    Job.Status.SUBMITTED: JobStatus.PENDING,
    Job.Status.QUEUED: JobStatus.PENDING,
    Job.Status.RUNNING: JobStatus.RUNNING,
    Job.Status.DONE: JobStatus.DONE,
    Job.Status.CANCELED: JobStatus.CANCELED,
    Job.Status.FAILED: JobStatus.ERROR,
}


class QDMIConnection(RemoteConnection):
    """Submit Pulser sequences as one closed QDMI batch.

    Each Pulser job is one program of the batch; its job ID is the batch ID followed by `#<index>`. Results are
    returned in Pulser's register order, and a batch can be retrieved from a new connection by its ID.

    Examples:
        >>> from pulser import QPUBackend
        >>> connection = QDMIConnection("pasqal.emu-free")
        >>> device = connection.fetch_available_devices()["FRESNEL_CAN1"]
        >>> results = QPUBackend(sequence, connection).run([{"runs": 100}], wait=True)
    """

    def __init__(self, device_id: str = "pasqal.emu-free", **session: str) -> None:
        """Open a QDMI device session.

        Args:
            device_id (str): QDMI device ID, e.g. "pasqal.emu-free" or "pasqal.fresnel".
            **session (str): Session parameters for `builtin_driver.open_device`.
        """
        self.device = builtin_driver.open_device(device_id, **session)
        self._jobs: dict[str, Job] = {}

    def fetch_available_devices(self) -> dict[str, Device]:
        """Return the Pulser devices of the cloud's public specifications, keyed by device type."""
        # This device always provides the specifications, and the cloud lists only physical devices.
        specs = json.loads(cast("str", self.device.query_custom_property(CustomProperty.CUSTOM1, str)))
        return {s["device_type"]: cast("Device", deserialize_device(s["specs"])) for s in specs}

    def submit(
        self,
        sequence: Sequence,
        wait: bool = False,  # ruff: ignore[boolean-default-value-positional-argument, boolean-type-hint-positional-argument] - Pulser interface
        open: bool = False,  # ruff: ignore[builtin-argument-shadowing, boolean-default-value-positional-argument, boolean-type-hint-positional-argument] - Pulser interface
        batch_id: str | None = None,
        **kwargs: Any,  # ruff: ignore[any-type] - Pulser interface
    ) -> RemoteResults:
        """Submit one program per job parameter set with a common run count.

        Args:
            sequence (Sequence): Sequence to build with each job's variables.
            wait (bool): Wait for the batch to finish.
            open (bool): Unsupported; QDMI batches are closed on submission.
            batch_id (str | None): Unsupported; QDMI batches cannot be extended.
            **kwargs (Any): Nonempty `job_params`; other submission arguments are unsupported.

        Returns:
            (RemoteResults): Results of the submitted batch.

        Raises:
            NotImplementedError: If an open/existing batch or unsupported submission argument is requested.
            ValueError: If job parameters are empty or ask for different run counts.
        """
        if open or batch_id is not None:
            msg = "QDMI batches are closed on submission."
            raise NotImplementedError(msg)
        job_params: list[JobParams] | None = kwargs.pop("job_params", None)
        if kwargs:
            msg = f"Unsupported Pulser submission arguments: {', '.join(kwargs)}."
            raise NotImplementedError(msg)
        if not job_params:
            msg = "At least one job parameter set is required."
            raise ValueError(msg)
        if len(runs := {params["runs"] for params in job_params}) != 1:
            msg = "QDMI batches need the same run count for every program."
            raise ValueError(msg)
        if sequence.is_parametrized() or sequence.is_register_mappable():
            programs = [
                self._add_measurement_to_sequence(sequence.build(**params.get("variables", {}))).to_abstract_repr()
                for params in job_params
            ]
        else:
            programs = [self._add_measurement_to_sequence(sequence).to_abstract_repr()] * len(job_params)
        job = self.device.submit_job(programs, ProgramFormat.CUSTOM1, runs.pop())
        self._jobs[job.id] = job
        if wait:
            try:
                job.wait()
            except BaseException as error:
                error.add_note(f"Pasqal batch {job.id} was submitted; retrieve it instead of resubmitting.")
                raise
        return RemoteResults(job.id, self)

    def _job(self, batch_id: str) -> Job:
        if batch_id not in self._jobs:
            self._jobs[batch_id] = self.device.retrieve_job_by_id(batch_id)
        return self._jobs[batch_id]

    def _get_job_ids(self, batch_id: str) -> list[str]:
        return [f"{batch_id}#{i}" for i in range(self._job(batch_id).num_programs)]

    def _get_batch_status(self, batch_id: str) -> BatchStatus:
        return BatchStatus[JOB_STATUS[self._job(batch_id).check()].name]

    def _query_job_progress(
        self, batch_id: str, job_ids: list[str] | None = None
    ) -> Mapping[str, tuple[JobStatus, Results | None]]:
        job = self._job(batch_id)
        selected = None if job_ids is None else set(job_ids)
        progress: dict[str, tuple[JobStatus, Results | None]] = {}
        for i, job_id in enumerate(self._get_job_ids(batch_id)):
            if selected is not None and job_id not in selected:
                continue
            # This device always reports program status.
            status = JOB_STATUS[cast("Job.Status", job.get_program_status(i))]
            result = None
            if status is JobStatus.DONE:
                sequence = Sequence.from_abstract_repr(job.get_program(i))
                # QDMI keys put atom 0 on the right; Pulser counts follow register order from the left.
                result = Results.from_final_bitstrings(
                    atom_order=tuple(sequence.get_register(include_mappable=False).qubit_ids),
                    total_duration=sequence.get_duration(include_fall_time=True),
                    final_bitstrings={bits[::-1]: count for bits, count in job.get_counts(i).items()},
                )
            progress[job_id] = status, result
        return progress

    def _fetch_result(self, batch_id: str, job_ids: list[str] | None) -> tuple[Results, ...]:
        progress = self._query_job_progress(batch_id, job_ids)
        results = []
        for job_id in progress if job_ids is None else job_ids:
            status, result = progress[job_id]
            if result is None:
                msg = f"No results for job {job_id}: {status}."
                raise RemoteResultsError(msg)
            results.append(result)
        return tuple(results)

    def supports_open_batch(self) -> bool:  # ruff: ignore[no-self-use] - Pulser interface
        """Report that QDMI batches cannot stay open.

        Returns:
            (bool): Always False.
        """
        return False
