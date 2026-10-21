---
file_format: mystnb
kernelspec:
  name: python3
language_info:
  name: python
---

# Run on EMU_FREE

This example submits one two-atom, 20-shot job to Pasqal's free emulator.
[Install the `core` and `pulser` extras](installation.md) and provide
`PASQAL_EMAIL`, `PASQAL_PASSWORD` and `PASQAL_PROJECT_ID` through your
environment. The project ID must be its full UUID. The documentation build
executes this page only when all three variables are present; ordinary builds
remain offline.

## Connect and inspect the backend

```{code-cell} ipython3
import json
import os

from mqt.core.qdmi import CustomProperty, Job, ProgramFormat, builtin_driver

session = {
    "username": os.environ["PASQAL_EMAIL"],
    "password": os.environ["PASQAL_PASSWORD"],
    "custom3": os.environ["PASQAL_PROJECT_ID"],
}
device = builtin_driver.open_device("pasqal.emu-free", **session)
metadata = json.loads(device.query_custom_property(CustomProperty.CUSTOM2, str))
assert metadata["device_type"] == "EMU_FREE"
assert metadata["is_emulator"] is True and metadata["bill_usage"] is False
print(f"EMU_FREE: {device.qubits_num()} atoms")
```

## Submit once or retrieve an existing job

A register with a one-microsecond delay stays in its initial ground state.
Setting `PASQAL_JOB_ID` retrieves a previous job instead of creating another
one.

```{code-cell} ipython3
from pulser import Register, Sequence
from pulser.devices import MockDevice

sequence = Sequence(Register({"left": (0, 0), "right": (10, 0)}), MockDevice)
sequence.declare_channel("global", "rydberg_global")
sequence.delay(1000, "global")
sequence.measure("ground-rydberg")
if job_id := os.environ.get("PASQAL_JOB_ID"):
    job = device.retrieve_job_by_id(job_id)
else:
    job = device.submit_job(sequence.to_abstract_repr(), ProgramFormat.CUSTOM1, num_shots=20)
print("Retain this job ID:", job.id, flush=True)
```

The job ID is available before waiting. A timeout does not cancel the job. Reuse
that ID after an interrupted run; do not resubmit if the submission response was
lost.

```{code-cell} ipython3
if not job.wait(timeout=300):
    raise TimeoutError(f"Job {job.id} is pending; retrieve it on the next run")
if job.check() != Job.Status.DONE:
    raise RuntimeError(f"Job {job.id} ended with status {job.check()}")
counts = job.get_counts()
if not job_id:
    assert counts == {"00": 20}
print(counts)
```

Retrieve the result through a fresh session to demonstrate recovery without a
second submission:

```{code-cell} ipython3
fresh_device = builtin_driver.open_device("pasqal.emu-free", **session)
retrieved = fresh_device.retrieve_job_by_id(job.id)
assert retrieved.get_counts() == counts
```

MyST-NB caches successful notebook executions by code, not environment
variables. To refresh a completed example, clear the notebook cache and supply
`PASQAL_JOB_ID` to reuse its job. A clean cache or a code change can execute
this page again when credentials are available. Remove the credential variables
to build only the offline examples. See
[documentation maintenance](maintenance.md#documentation) for Read the Docs
setup.
