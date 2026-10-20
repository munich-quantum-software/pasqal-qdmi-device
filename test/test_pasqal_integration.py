"""Exercise QDMI programs against a local HTTP server and optional Python clients."""

import ctypes as C
import gc
import importlib.util
import json
import os
from pathlib import Path
import sys
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import Mock, patch
import weakref


ROOT = Path(__file__).resolve().parents[1]
LIBRARY = Path(os.environ.get("PASQAL_QDMI_LIBRARY", ROOT / "build/src/libpasqal-qdmi-device.so"))
CUSTOM1 = 999_999_995
EXPIRED_TOKEN = "h.eyJleHAiOjF9.s"
ONE = json.dumps({"register": [{"name": "a", "x": 0, "y": 0}]})
TWO = json.dumps({"register": [{"name": "a", "x": 0, "y": 0}, {"name": "b", "x": 5, "y": 0}]})
SPECS = json.dumps({"max_atom_num": 4, "channels": [{"id": "rydberg_global", "addressing": "Global"}]})


class NativeAPI:
    def __init__(self):
        lib = C.CDLL(str(LIBRARY))
        handle, size, integer = C.c_void_p, C.c_size_t, C.c_int
        signatures = {
            "session_alloc": [C.POINTER(handle)],
            "session_set_parameter": [handle, integer, size, handle],
            "session_init": [handle],
            "session_free": [handle],
            "session_create_device_job": [handle, C.POINTER(handle)],
            "session_retrieve_device_job_by_id": [handle, C.c_char_p, C.POINTER(handle)],
            "job_set_programs": [handle, integer, size, C.POINTER(size), C.POINTER(handle)],
            "job_submit": [handle],
            "job_check": [handle, C.POINTER(integer)],
            "job_wait": [handle, size],
            "job_cancel": [handle],
            "job_get_program_status": [handle, size, C.POINTER(integer)],
            "job_get_program": [handle, size, size, handle, C.POINTER(size)],
            "job_get_results": [handle, size, integer, size, handle, C.POINTER(size)],
            "job_query_property": [handle, integer, size, handle, C.POINTER(size)],
            "job_free": [handle],
        }
        for name, args in signatures.items():
            function = getattr(lib, "PASQAL_QDMI_device_" + name)
            function.argtypes = args
            function.restype = None if name.endswith("_free") else integer
            setattr(self, name, function)


class CloudHandler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, category, body, status=200):
        state = self.server.state
        if category == "auth":
            state.auth_started.set()
        time.sleep(state.delays.get(category, 0))
        data = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except BrokenPipeError:
            pass  # A timeout test intentionally closes the connection.

    def do_GET(self):
        state = self.server.state
        state.requests.append(self.path)
        if "/devices/specs/" in self.path:
            self.reply("specs", {"data": {"device_type": "FRESNEL", "specs": SPECS}})
        elif "/devices?" in self.path:
            self.reply("devices", {"data": [{"availability": "ACTIVE"}]})
        elif self.path.endswith("/results_link"):
            job_id = self.path.split("/")[-2]
            self.reply("link", {"data": {"results_link": state.base + "/results/" + job_id}})
        elif self.path.startswith("/results/"):
            self.reply("download", {"counter": {"00": 7}}, state.errors.get(self.path, 200))
        else:
            job_id = self.path.split("/")[-1]
            self.reply("status", {"data": {"status": state.statuses.get(job_id, "RUNNING")}},
                       state.errors.get(job_id, 200))

    def do_POST(self):
        state = self.server.state
        data = self.rfile.read(int(self.headers["Content-Length"]))
        if self.path == "/auth":
            self.reply("auth", {"access_token": EXPIRED_TOKEN})
            return
        state.submissions.append(json.loads(data))
        index = len(state.submissions)
        self.reply("submit", {"data": {"jobs": [{"id": f"job{index}"}]}},
                   500 if index == state.fail_submission else 200)

    def do_PATCH(self):
        state = self.server.state
        job_id = self.path.split("/")[-2]
        state.cancellations.append(job_id)
        state.statuses[job_id] = "CANCELING"
        self.reply("cancel", {})


class CloudTests(unittest.TestCase):
    def setUp(self):
        self.api = NativeAPI()
        self.statuses, self.errors, self.delays = {}, {}, {}
        self.submissions, self.cancellations = [], []
        self.requests = []
        self.fail_submission = 0
        self.auth_started = threading.Event()
        server = ThreadingHTTPServer(("127.0.0.1", 0), CloudHandler)
        server.state = self
        self.base = f"http://127.0.0.1:{server.server_port}"
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)

    def session(self, token="token"):
        session = C.c_void_p()
        self.assertEqual(self.api.session_alloc(C.byref(session)), 0)
        self.addCleanup(self.api.session_free, session)
        for param, value in [(0, self.base), (1, token), (2, "/nonexistent"),
                             (3, self.base + "/auth"), (4, "user"), (5, "password"),
                             (CUSTOM1, "FRESNEL")]:
            data = value.encode() + b"\0"
            self.assertEqual(self.api.session_set_parameter(session, param, len(data), C.c_char_p(data)), 0)
        self.assertEqual(self.api.session_init(session), 0)
        return session

    def job(self, session, programs=(ONE, TWO)):
        job = C.c_void_p()
        self.assertEqual(self.api.session_create_device_job(session, C.byref(job)), 0)
        self.addCleanup(self.api.job_free, job)
        data = [program.encode() + b"\0" for program in programs]
        sizes = (C.c_size_t * len(data))(*(len(item) for item in data))
        pointers = (C.c_void_p * len(data))(*(C.cast(C.c_char_p(item), C.c_void_p) for item in data))
        self.assertEqual(self.api.job_set_programs(job, CUSTOM1, len(data), sizes, pointers), 0)
        return job

    def retrieve(self, session, job_id):
        job = C.c_void_p()
        self.assertEqual(self.api.session_retrieve_device_job_by_id(session, job_id.encode(), C.byref(job)), 0)
        self.addCleanup(self.api.job_free, job)
        return job

    def job_id(self, job):
        size = C.c_size_t()
        self.assertEqual(self.api.job_query_property(job, 0, 0, None, C.byref(size)), 0)
        data = C.create_string_buffer(size.value)
        self.assertEqual(self.api.job_query_property(job, 0, size.value, data, None), 0)
        return data.value.decode()

    def status(self, job, index=None):
        status = C.c_int()
        code = (self.api.job_check(job, C.byref(status)) if index is None else
                self.api.job_get_program_status(job, index, C.byref(status)))
        self.assertEqual(code, 0)
        return status.value

    def test_ordered_programs_and_retrieval(self):
        """Restore program order and expose successful results after a sibling fails."""
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        self.assertEqual([item["sequence_builder"] for item in self.submissions], [ONE, TWO])
        self.statuses.update(job1="DONE", job2="ERROR")
        self.assertEqual(self.status(job), 6)
        restored = self.retrieve(self.session(), self.job_id(job))
        count = C.c_size_t()
        self.assertEqual(self.api.job_query_property(restored, 5, C.sizeof(count), C.byref(count), None), 0)
        self.assertEqual(count.value, 2)
        self.assertEqual(self.status(restored, 0), 4)
        self.assertEqual(self.status(restored, 1), 6)
        self.assertEqual(self.api.job_get_program(restored, 0, 0, None, None), -9)
        keys = C.create_string_buffer(3)
        self.assertEqual(self.api.job_get_results(restored, 0, 1, 3, keys, None), 0)
        self.assertEqual(keys.value, b"00")
        self.assertEqual(self.api.job_get_results(restored, 1, 1, 0, None, None), -10)

    def test_partial_submission_remains_recoverable(self):
        """Keep accepted jobs active and preserve failed and unsubmitted indices."""
        self.fail_submission = 2
        job = self.job(self.session(), (ONE, TWO, ONE))
        self.assertEqual(self.api.job_submit(job), -1)
        self.assertEqual(len(self.submissions), 2)
        self.assertEqual(self.status(job), 3)
        restored = self.retrieve(self.session(), self.job_id(job))
        self.assertEqual([self.status(restored, i) for i in range(3)], [3, 6, 5])
        self.statuses["job1"] = "DONE"
        self.assertEqual(self.status(restored), 6)
        self.assertEqual(self.api.job_get_results(restored, 0, 1, 0, None, None), 0)

    def test_retrieve_list_with_unavailable_program(self):
        """Restore the list without polling and fetch siblings independently."""
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        self.statuses.update(job1="DONE", job2="DONE")
        session = self.session()
        for endpoint, http_code, qdmi_code in [("job1", 404, -5), ("job1", 503, -1),
                                               ("/results/job1", 503, -1)]:
            with self.subTest(endpoint=endpoint, http_code=http_code):
                self.errors = {endpoint: http_code}
                self.requests.clear()
                restored = self.retrieve(session, self.job_id(job))
                self.assertEqual(self.requests, [])
                count = C.c_size_t()
                self.assertEqual(self.api.job_query_property(restored, 5, C.sizeof(count), C.byref(count), None), 0)
                self.assertEqual(count.value, 2)
                self.assertEqual(self.api.job_get_results(restored, 1, 1, 0, None, None), 0)
                self.assertEqual(self.api.job_get_results(restored, 0, 1, 0, None, None), qdmi_code)
                self.errors.clear()
                self.assertEqual(self.api.job_get_results(restored, 0, 1, 0, None, None), 0)

    def test_retrieve_single_missing_job(self):
        """Keep the not-found error when retrieving a raw cloud job ID."""
        session = self.session()
        self.errors["missing"] = 404
        job = C.c_void_p()
        self.assertEqual(self.api.session_retrieve_device_job_by_id(session, b"missing", C.byref(job)), -5)
        self.assertIsNone(job.value)

    def test_results_refresh_without_prior_check(self):
        """Fetch one completed program while its sibling is still running."""
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        self.statuses["job2"] = "DONE"
        self.assertEqual(self.api.job_get_results(job, 1, 1, 0, None, None), 0)
        self.assertEqual(self.status(job), 3)

    def test_failed_submissions_have_distinct_ids(self):
        """Recover separate jobs even when neither received a cloud ID."""
        session = self.session()
        ids = []
        for _ in range(2):
            self.fail_submission = len(self.submissions) + 1
            job = self.job(session)
            self.assertEqual(self.api.job_submit(job), -1)
            ids.append(self.job_id(job))
            restored = self.retrieve(session, ids[-1])
            self.assertEqual([self.status(restored, i) for i in range(2)], [6, 5])
        self.assertNotEqual(*ids)

    def test_cancel_skips_completed_programs(self):
        """Cancel active programs and wait for the cloud's terminal outcome."""
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        self.statuses["job1"] = "DONE"
        self.assertEqual(self.status(job, 0), 4)
        self.assertEqual(self.api.job_cancel(job), 0)
        self.assertEqual(self.cancellations, ["job2"])
        self.assertNotIn(self.status(job), [4, 5, 6])
        self.statuses["job2"] = "CANCELED"
        self.assertEqual(self.status(job), 5)
        self.assertEqual(self.api.job_get_results(job, 0, 1, 0, None, None), 0)

    def test_poll_error_does_not_hide_sibling_results(self):
        """Keep polling other programs when one request fails."""
        job = self.job(self.session())
        self.assertEqual(self.api.job_submit(job), 0)
        self.errors["job1"] = 503
        self.statuses["job2"] = "DONE"
        status = C.c_int()
        self.assertEqual(self.api.job_check(job, C.byref(status)), -1)
        self.assertEqual(self.api.job_get_results(job, 1, 1, 0, None, None), 0)
        self.errors.clear()
        self.assertEqual(self.status(job, 0), 3)

    def test_wait_deadline_spans_requests(self):
        """Bound polling, token refresh, and multi-request result downloads."""
        for category in ["status", "auth", "download"]:
            with self.subTest(category=category):
                self.delays = {}
                session = self.session(EXPIRED_TOKEN if category == "auth" else "token")
                self.statuses["existing"] = "RUNNING"
                job = self.retrieve(session, "existing")
                self.delays = {category: 2}
                if category == "download":
                    self.statuses["existing"] = "DONE"
                    self.delays.update(status=0.2, link=0.2)
                start = time.monotonic()
                self.assertEqual(self.api.job_wait(job, 1), -11)
                self.assertLess(time.monotonic() - start, 1.5)
                self.delays = {}
                self.statuses["existing"] = "DONE"
                self.assertEqual(self.api.job_wait(job, 0), 0)

    def test_wait_deadline_includes_token_lock(self):
        """Bound waiting for another thread's token refresh."""
        session = self.session(EXPIRED_TOKEN)
        first = self.retrieve(session, "first")
        second = self.retrieve(session, "second")
        self.delays["auth"] = 2
        self.auth_started.clear()
        status = C.c_int()
        thread = threading.Thread(target=self.api.job_check, args=(first, C.byref(status)))
        thread.start()
        self.addCleanup(thread.join)
        self.assertTrue(self.auth_started.wait(1))
        start = time.monotonic()
        self.assertEqual(self.api.job_wait(second, 1), -11)
        self.assertLess(time.monotonic() - start, 1.5)


@unittest.skipUnless(importlib.util.find_spec("pulser"), "Pulser is not installed")
class AdapterTests(unittest.TestCase):
    def setUp(self):
        sys.path.insert(0, str(ROOT / "examples"))
        import pulser_qdmi_backend as backend
        self.backend = backend

    def test_available_results_with_lookup_errors(self):
        """Keep sibling results accessible and retry unavailable programs."""
        from pulser import Pulse, Sequence
        from pulser.devices import DigitalAnalogDevice
        from pulser.register import Register
        sequence = Sequence(Register({"q0": (0, 0), "q1": (5, 0)}), DigitalAnalogDevice)
        sequence.declare_channel("rydberg", "rydberg_global")
        sequence.add(Pulse.ConstantPulse(100, 2, 2, 0), "rydberg")
        for index, category, code in [(0, "status", 404), (1, "status", 503), (0, "download", 503)]:
            with self.subTest(index=index, category=category, code=code):
                cloud = CloudTests()
                cloud.setUp()
                self.addCleanup(cloud.doCleanups)
                native = self.backend.QDMILibrary(LIBRARY)
                initialize = native.lib.PASQAL_QDMI_device_session_init

                def initialize_local(session):
                    for param, value in [(0, cloud.base), (1, "token"), (2, "/nonexistent")]:
                        data = value.encode() + b"\0"
                        self.assertEqual(native.lib.PASQAL_QDMI_device_session_set_parameter(
                            session, param, len(data), C.c_char_p(data)), 0)
                    return initialize(session)

                native.lib.PASQAL_QDMI_device_session_init = initialize_local
                with patch.object(self.backend, "QDMILibrary", return_value=native):
                    connection = self.backend.QDMIPulserConnection("FRESNEL")
                with connection:
                    results = connection.submit(sequence, job_params=[{"runs": 7}] * 3)
                    cloud.statuses.update(job1="DONE", job2="DONE", job3="DONE")
                    failed_id = f"job{index + 1}"
                    cloud.errors[failed_id if category == "status" else f"/results/{failed_id}"] = code
                    with self.assertWarnsRegex(RuntimeWarning, "QDMI call failed"):
                        available = results.get_available_results()
                    expected_ids = [job_id for i, job_id in enumerate(results.job_ids) if i != index]
                    self.assertEqual(list(available), expected_ids)
                    self.assertTrue(all(result.final_bitstrings == {"00": 7} for result in available.values()))
                    with self.assertRaises(self.backend.RemoteResultsError):
                        _ = results.results
                    cloud.requests.clear()
                    subset = self.backend.RemoteResults(results.batch_id, connection, job_ids=expected_ids)
                    self.assertEqual([result.final_bitstrings for result in subset.results], [{"00": 7}] * 2)
                    self.assertFalse(any(failed_id in path for path in cloud.requests))
                    cloud.errors.clear()
                    self.assertEqual([result.final_bitstrings for result in results.results], [{"00": 7}] * 3)

    def test_parameter_sweep(self):
        """Build each variable assignment and return results in input order."""
        from pulser import Pulse, Sequence
        from pulser.devices import DigitalAnalogDevice
        from pulser.register import RegisterLayout
        layout = RegisterLayout([(0, 0), (5, 0), (10, 0), (15, 0)])
        sequence = Sequence(layout.make_mappable_register(2), DigitalAnalogDevice)
        sequence.declare_channel("rydberg", "rydberg_global")
        duration = sequence.declare_variable("duration", dtype=int)
        sequence.add(Pulse.ConstantPulse(duration, 2, 2, 0), "rydberg")
        params = [{"runs": 7, "variables": {"duration": duration, "qubits": {"q0": 0, "q1": 2}}}
                  for duration in [100, 200]]
        with self.backend.QDMIPulserConnection("MOCK", LIBRARY) as connection:
            results = connection.submit(sequence, job_params=params, wait=True)
            self.assertEqual([result.final_bitstrings for result in results.results], [{"00": 7}] * 2)
            self.assertEqual([result.total_duration for result in results.results], [100, 200])
            built = connection._job_sequences[results.batch_id]
            self.assertEqual([item.get_duration() for item in built], [100, 200])
            self.assertTrue(all(not item.is_parametrized() and not item.is_register_mappable() for item in built))
            self.assertTrue(sequence.is_parametrized())
            self.assertTrue(sequence.is_register_mappable())

    def test_partial_submission_results(self):
        """Expose completed results through the submission error's handle."""
        sequence = self.backend.build_sequence()
        source = sequence.to_abstract_repr()
        with self.backend.QDMIPulserConnection("MOCK", LIBRARY) as connection:
            with patch.object(type(sequence), "to_abstract_repr", side_effect=[source, "{}", source]):
                with self.assertRaises(self.backend.QDMISubmissionError) as caught:
                    connection.submit(sequence, job_params=[{"runs": 7}] * 3)
            results = caught.exception.results
            self.assertEqual(results.get_batch_status(), self.backend.BatchStatus.ERROR)
            available = results.get_available_results()
            self.assertEqual(list(available), results.job_ids[:1])
            self.assertEqual(available[results.job_ids[0]].final_bitstrings, {"0000": 7})
            with self.assertRaises(self.backend.RemoteResultsError):
                _ = results.results

    def test_cleanup(self):
        """Free native jobs and sessions on close, failed setup, and collection."""
        for mode in ["close", "gc", "parameter", "constructor"]:
            with self.subTest(mode=mode):
                native = self.backend.QDMILibrary(LIBRARY)
                native.lib.PASQAL_QDMI_device_job_free = Mock(wraps=native.lib.PASQAL_QDMI_device_job_free)
                native.lib.PASQAL_QDMI_device_session_free = Mock(wraps=native.lib.PASQAL_QDMI_device_session_free)
                with patch.object(self.backend, "QDMILibrary", return_value=native):
                    if mode == "constructor":
                        with self.assertRaises(RuntimeError):
                            self.backend.QDMIPulserConnection("UNKNOWN")
                    else:
                        connection = self.backend.QDMIPulserConnection("MOCK")
                        if mode == "parameter":
                            with self.assertRaises(RuntimeError):
                                connection.submit(self.backend.build_sequence(), job_params=[{"runs": 2**40}])
                        else:
                            connection.submit(self.backend.build_sequence())
                        if mode == "gc":
                            ref = weakref.ref(connection)
                            del connection
                            gc.collect()
                            self.assertIsNone(ref())
                        else:
                            connection.close()
                            connection.close()
                        self.assertEqual(native.lib.PASQAL_QDMI_device_job_free.call_count, 1)
                    self.assertEqual(native.lib.PASQAL_QDMI_device_session_free.call_count, 1)


@unittest.skipUnless(importlib.util.find_spec("mqt"), "MQT Core is not installed")
class DriverTests(unittest.TestCase):
    def test_mqt_discovery_and_program_list(self):
        """Open the catalog's mock device with the MQT Core driver and execute a list."""
        from mqt.core import qdmi
        driver = qdmi.builtin_driver
        driver.add_manifest(LIBRARY.parent / "pasqal-qdmi-device.qdmi.json")
        self.assertIn("pasqal.mock", driver.registered_device_ids())
        device = driver.open_device("pasqal.mock")
        job = device.submit_job([ONE, TWO], qdmi.ProgramFormat.CUSTOM1, num_shots=7)
        self.assertEqual(job.num_programs, 2)
        self.assertTrue(job.wait(1))
        self.assertEqual(job.get_program(0), ONE)
        self.assertEqual(job.get_program(1), TWO)
        self.assertEqual(job.get_counts(0), {"0": 7})
        self.assertEqual(job.get_counts(1), {"00": 7})


if __name__ == "__main__":
    unittest.main()
