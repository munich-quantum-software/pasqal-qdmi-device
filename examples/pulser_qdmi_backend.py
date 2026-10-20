from __future__ import annotations

import ctypes
import warnings
import weakref
from contextlib import ExitStack
from pathlib import Path
from typing import Any, Mapping

import pulser
from pulser import Pulse, Sequence
from pulser.backend.remote import (
    BatchStatus,
    JobParams,
    JobStatus,
    RemoteConnection,
    RemoteResults,
    RemoteResultsError,
)
from pulser.backend.results import Results
from pulser.register import Register

QDMI_SUCCESS = 0
QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1 = 999_999_995
QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM = 2
QDMI_DEVICE_JOB_PROPERTY_ID = 0
QDMI_PROGRAM_FORMAT_CUSTOM1 = 999_999_995
QDMI_JOB_RESULT_HIST_KEYS = 1
QDMI_JOB_RESULT_HIST_VALUES = 2
QDMI_JOB_STATUS_CREATED = 0
QDMI_JOB_STATUS_SUBMITTED = 1
QDMI_JOB_STATUS_QUEUED = 2
QDMI_JOB_STATUS_RUNNING = 3
QDMI_JOB_STATUS_DONE = 4
QDMI_JOB_STATUS_CANCELED = 5
QDMI_JOB_STATUS_FAILED = 6

JOB_STATUS_MAP = {
    QDMI_JOB_STATUS_CREATED: JobStatus.PENDING,
    QDMI_JOB_STATUS_SUBMITTED: JobStatus.PENDING,
    QDMI_JOB_STATUS_QUEUED: JobStatus.PENDING,
    QDMI_JOB_STATUS_RUNNING: JobStatus.RUNNING,
    QDMI_JOB_STATUS_DONE: JobStatus.DONE,
    QDMI_JOB_STATUS_CANCELED: JobStatus.CANCELED,
    QDMI_JOB_STATUS_FAILED: JobStatus.ERROR,
}

BATCH_STATUS_MAP = {
    QDMI_JOB_STATUS_CREATED: BatchStatus.PENDING,
    QDMI_JOB_STATUS_SUBMITTED: BatchStatus.PENDING,
    QDMI_JOB_STATUS_QUEUED: BatchStatus.PENDING,
    QDMI_JOB_STATUS_RUNNING: BatchStatus.RUNNING,
    QDMI_JOB_STATUS_DONE: BatchStatus.DONE,
    QDMI_JOB_STATUS_CANCELED: BatchStatus.CANCELED,
    QDMI_JOB_STATUS_FAILED: BatchStatus.ERROR,
}


class QDMILibrary:
    def __init__(self, path: str | Path) -> None:
        self.lib = ctypes.CDLL(str(path))
        void_pp = ctypes.POINTER(ctypes.c_void_p)

        self.lib.PASQAL_QDMI_device_session_alloc.argtypes = [void_pp]
        self.lib.PASQAL_QDMI_device_session_alloc.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_session_set_parameter.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_size_t,
            ctypes.c_void_p,
        ]
        self.lib.PASQAL_QDMI_device_session_set_parameter.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_session_init.argtypes = [ctypes.c_void_p]
        self.lib.PASQAL_QDMI_device_session_init.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_session_free.argtypes = [ctypes.c_void_p]
        self.lib.PASQAL_QDMI_device_session_free.restype = None

        self.lib.PASQAL_QDMI_device_session_create_device_job.argtypes = [
            ctypes.c_void_p,
            void_pp,
        ]
        self.lib.PASQAL_QDMI_device_session_create_device_job.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_set_parameter.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_size_t,
            ctypes.c_void_p,
        ]
        self.lib.PASQAL_QDMI_device_job_set_parameter.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_set_programs.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_size_t),
            void_pp,
        ]
        self.lib.PASQAL_QDMI_device_job_set_programs.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_get_program_status.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_int),
        ]
        self.lib.PASQAL_QDMI_device_job_get_program_status.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_query_property.argtypes = [
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self.lib.PASQAL_QDMI_device_job_query_property.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_submit.argtypes = [ctypes.c_void_p]
        self.lib.PASQAL_QDMI_device_job_submit.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_check.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_int),
        ]
        self.lib.PASQAL_QDMI_device_job_check.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_wait.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        self.lib.PASQAL_QDMI_device_job_wait.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_get_results.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_int,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_size_t),
        ]
        self.lib.PASQAL_QDMI_device_job_get_results.restype = ctypes.c_int
        self.lib.PASQAL_QDMI_device_job_free.argtypes = [ctypes.c_void_p]
        self.lib.PASQAL_QDMI_device_job_free.restype = None


class QDMIPulserConnection(RemoteConnection):
    def __init__(
        self,
        backend: str = "EMU_FREE",
        library_path: str | Path | None = None,
    ) -> None:
        root = Path(__file__).resolve().parents[1]
        path = library_path or root / "build/src/libpasqal-qdmi-device.so"
        self._qdmi = QDMILibrary(path)
        self._backend = backend
        self._session = ctypes.c_void_p()
        self._jobs: dict[str, ctypes.c_void_p] = {}
        self._job_sequences: dict[str, list[Sequence]] = {}
        self._job_ids = {}
        self._resources = ExitStack()
        self._finalizer = weakref.finalize(self, self._resources.close)
        try:
            self._check(self._qdmi.lib.PASQAL_QDMI_device_session_alloc(ctypes.byref(self._session)))
            self._resources.callback(self._qdmi.lib.PASQAL_QDMI_device_session_free, self._session)
            self._set_session_string(QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1, backend)
            self._check(self._qdmi.lib.PASQAL_QDMI_device_session_init(self._session))
        except BaseException:
            self.close()
            raise

    def close(self) -> None:
        """Free native jobs and their session."""
        self._finalizer()
        self._session.value = None
        self._jobs.clear()
        self._job_sequences.clear()
        self._job_ids.clear()

    def __enter__(self) -> QDMIPulserConnection:
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        self.close()

    def submit(
        self,
        sequence: Sequence,
        wait: bool = False,
        open: bool = True,
        batch_id: str | None = None,
        **kwargs: Any,
    ) -> RemoteResults:
        if batch_id is not None:
            raise NotImplementedError("QDMI Pulser connection does not append to existing batches.")

        job_params = kwargs.get("job_params") or [{"runs": 100}]
        shots = int(job_params[0].get("runs", 100))
        if any(int(params.get("runs", 100)) != shots for params in job_params):
            raise ValueError("QDMI program lists require the same run count for every program.")
        sequences = []
        for params in job_params:
            built = (
                sequence.build(**params.get("variables", {}))
                if sequence.is_parametrized() or sequence.is_register_mappable()
                else sequence
            )
            sequences.append(self._add_measurement_to_sequence(built))
        programs = [seq.to_abstract_repr().encode() + b"\0" for seq in sequences]
        sizes = (ctypes.c_size_t * len(programs))(*(len(program) for program in programs))
        pointers = (ctypes.c_void_p * len(programs))(
            *(ctypes.cast(ctypes.c_char_p(program), ctypes.c_void_p) for program in programs)
        )

        job = ctypes.c_void_p()
        with ExitStack() as cleanup:
            self._check(
                self._qdmi.lib.PASQAL_QDMI_device_session_create_device_job(
                    self._session,
                    ctypes.byref(job),
                )
            )
            cleanup.callback(self._qdmi.lib.PASQAL_QDMI_device_job_free, job)
            self._set_job_usize(job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, shots)
            self._check(
                self._qdmi.lib.PASQAL_QDMI_device_job_set_programs(
                    job, QDMI_PROGRAM_FORMAT_CUSTOM1, len(programs), sizes, pointers
                )
            )
            code = self._qdmi.lib.PASQAL_QDMI_device_job_submit(job)
            job_id = self._query_job_id(job)
            self._resources.enter_context(cleanup.pop_all())
        self._jobs[job_id] = job
        self._job_ids[job_id] = (
            [job_id] if len(programs) == 1 else [f"{job_id}#{i}" for i in range(len(programs))]
        )
        self._job_sequences[job_id] = sequences
        results = RemoteResults(job_id, self, job_ids=self._job_ids[job_id])
        if code != QDMI_SUCCESS:
            raise QDMISubmissionError(code, results)

        if wait:
            self._wait(job_id)
        return results

    def _fetch_result(self, batch_id: str, job_ids: list[str] | None) -> tuple[Results, ...]:
        indices = {job_id: index for index, job_id in enumerate(self._job_ids[batch_id])}
        ids = job_ids or list(indices)
        results = []
        for job_id in ids:
            try:
                status, result = self._query_program_progress(batch_id, indices[job_id])
            except RuntimeError as error:
                raise RemoteResultsError(f"Could not fetch results for job {job_id}: {error}") from error
            if status in {JobStatus.PENDING, JobStatus.RUNNING}:
                raise RemoteResultsError(f"Results are not ready for job {job_id}: {status}.")
            if result is None:
                raise RemoteResultsError(f"No results found for job {job_id}.")
            results.append(result)
        return tuple(results)

    def _query_job_progress(self, batch_id: str) -> Mapping[str, tuple[JobStatus, Results | None]]:
        progress = {}
        for index, job_id in enumerate(self._job_ids[batch_id]):
            try:
                progress[job_id] = self._query_program_progress(batch_id, index)
            except RuntimeError as error:
                warnings.warn(
                    f"Could not fetch progress for job {job_id}: {error}", RuntimeWarning, stacklevel=2
                )
        return progress

    def _query_program_progress(self, batch_id: str, index: int) -> tuple[JobStatus, Results | None]:
        """Query one program's status and available result.

Args:
    batch_id (str): ID of the containing batch.
    index (int): Program's index in submission order.

Returns:
    status (JobStatus): Current program status.
    result (Results | None): Completed result when available.
        """
        job = self._jobs[batch_id]
        status = ctypes.c_int()
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_get_program_status(job, index, ctypes.byref(status))
        )
        result = self._result(batch_id, job, index) if status.value == QDMI_JOB_STATUS_DONE else None
        return JOB_STATUS_MAP[status.value], result

    def _get_batch_status(self, batch_id: str) -> BatchStatus:
        return BATCH_STATUS_MAP[self._check_status(self._jobs[batch_id])]

    def _get_job_ids(self, batch_id: str) -> list[str]:
        return self._job_ids[batch_id]

    def supports_open_batch(self) -> bool:
        return False

    def _set_session_string(self, parameter: int, value: str) -> None:
        raw = value.encode() + b"\0"
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_session_set_parameter(
                self._session,
                parameter,
                len(raw),
                ctypes.c_char_p(raw),
            )
        )

    def _set_job_usize(self, job: ctypes.c_void_p, parameter: int, value: int) -> None:
        raw = ctypes.c_size_t(value)
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_set_parameter(
                job,
                parameter,
                ctypes.sizeof(raw),
                ctypes.byref(raw),
            )
        )

    def _query_job_id(self, job: ctypes.c_void_p) -> str:
        size = ctypes.c_size_t()
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_query_property(
                job,
                QDMI_DEVICE_JOB_PROPERTY_ID,
                0,
                None,
                ctypes.byref(size),
            )
        )
        buf = ctypes.create_string_buffer(size.value)
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_query_property(
                job,
                QDMI_DEVICE_JOB_PROPERTY_ID,
                size.value,
                ctypes.cast(buf, ctypes.c_void_p),
                None,
            )
        )
        return buf.value.decode()

    def _check_status(self, job: ctypes.c_void_p) -> int:
        status = ctypes.c_int()
        self._check(self._qdmi.lib.PASQAL_QDMI_device_job_check(job, ctypes.byref(status)))
        return status.value

    def _wait(self, batch_id: str) -> None:
        job = self._jobs[batch_id]
        self._check(self._qdmi.lib.PASQAL_QDMI_device_job_wait(job, 0))

    def _result(self, batch_id: str, job: ctypes.c_void_p, index: int) -> Results:
        keys = self._result_keys(job, index)
        values = self._result_values(job, index)
        sequence = self._job_sequences[batch_id][index]
        return Results.from_final_bitstrings(
            atom_order=tuple(sequence.get_register(include_mappable=True).qubit_ids),
            total_duration=sequence.get_duration(include_fall_time=True),
            final_bitstrings=dict(zip(keys, values)),
        )

    def _result_keys(self, job: ctypes.c_void_p, index: int) -> list[str]:
        size = ctypes.c_size_t()
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_get_results(
                job,
                index,
                QDMI_JOB_RESULT_HIST_KEYS,
                0,
                None,
                ctypes.byref(size),
            )
        )
        buf = ctypes.create_string_buffer(size.value)
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_get_results(
                job,
                index,
                QDMI_JOB_RESULT_HIST_KEYS,
                size.value,
                ctypes.cast(buf, ctypes.c_void_p),
                None,
            )
        )
        return buf.value.decode().split(",")

    def _result_values(self, job: ctypes.c_void_p, index: int) -> list[int]:
        size = ctypes.c_size_t()
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_get_results(
                job,
                index,
                QDMI_JOB_RESULT_HIST_VALUES,
                0,
                None,
                ctypes.byref(size),
            )
        )
        count = size.value // ctypes.sizeof(ctypes.c_size_t)
        values = (ctypes.c_size_t * count)()
        self._check(
            self._qdmi.lib.PASQAL_QDMI_device_job_get_results(
                job,
                index,
                QDMI_JOB_RESULT_HIST_VALUES,
                size.value,
                ctypes.cast(values, ctypes.c_void_p),
                None,
            )
        )
        return list(values)

    @staticmethod
    def _check(code: int) -> None:
        if code != QDMI_SUCCESS:
            raise RuntimeError(f"QDMI call failed with status {code}.")


class QDMISubmissionError(RuntimeError):
    """Keep access to accepted programs after a partial submission failure."""

    def __init__(self, code: int, results: RemoteResults) -> None:
        """
Args:
    code (int): QDMI submission error code.
    results (RemoteResults): Handle for checking accepted programs and their results.
        """
        super().__init__(f"QDMI submission failed with status {code}; batch ID: {results.batch_id}.")
        self.results = results


def build_sequence() -> Sequence:
    reg = Register(
        {
            "q0": (-2.5, -2.5),
            "q1": (2.5, -2.5),
            "q2": (-2.5, 2.5),
            "q3": (2.5, 2.5),
        }
    ).with_automatic_layout(pulser.DigitalAnalogDevice)
    seq = Sequence(reg, pulser.DigitalAnalogDevice)
    seq.declare_channel("rydberg", "rydberg_global")
    seq.add(Pulse.ConstantPulse(100, 2, 2, 0), "rydberg")
    seq.measure("ground-rydberg")
    return seq


if __name__ == "__main__":
    with QDMIPulserConnection("EMU_FREE") as connection:
        backend = pulser.QPUBackend(build_sequence(), connection)
        remote_results = backend.run([JobParams(runs=100, variables={})], wait=True)
        print("batch_id:", remote_results.batch_id)
        print("batch_status:", remote_results.get_batch_status())
        print("results:", remote_results.results)
