# Native Pulser usage

First [install the package](installation.md) and
[configure credentials](authentication.md). Construct and validate a built
sequence with released Pulser. `Sequence.to_abstract_repr()` is the native JSON
transport format. Resolve register mappings and symbolic variables before
submission; pulse compilation belongs to Pulser and its supported device
tooling.

## Submit and retrieve

The following assumes `sequence` is a measured, built Pulser `Sequence`, and
`email`, `password` and `project_uuid` came from secure local input:

```python
from mqt.core import qdmi
from mqt.core.qdmi import builtin_driver

device = builtin_driver.open_device("pasqal.emu-free", username=email, password=password, custom3=project_uuid)
job = device.submit_job(sequence.to_abstract_repr(), qdmi.ProgramFormat.CUSTOM1, num_shots=100)
batch_id = job.id
if not job.wait(timeout=300):  # seconds; a bounded local wait
    raise TimeoutError("Job is still pending; retain its batch ID")
counts = job.get_counts()

fresh_device = builtin_driver.open_device("pasqal.emu-free", username=email, password=password, custom3=project_uuid)
retrieved = fresh_device.retrieve_job_by_id(batch_id)
assert retrieved.get_counts() == counts
```

Check the returned job status before assuming success: completion can also mean
a failed or canceled job. Results are available only for completed programs.
Retain the sanitized cloud batch ID so a process restart does not require
resubmission.

## Multiple programs

Pass an ordered list of built JSON sequences to `submit_job`, with one common
shot count:

```python
job = device.submit_job(
    [first.to_abstract_repr(), second.to_abstract_repr()],
    qdmi.ProgramFormat.CUSTOM1,
    num_shots=100,
)
# After completion:
first_counts = job.get_counts(program_index=0)
second_counts = job.get_counts(program_index=1)
```

The provider sends one closed Pasqal batch containing ordered per-job sequences.
The QDMI program index identifies the matching cloud job. Different shot counts
per program and appending to an existing batch are not exposed. `job.cancel()`
requests cloud cancellation and checks status once. QDMI reports success only
when cancellation is confirmed. If the cloud still reports cancellation in
progress, Core raises because the device returns `QDMI_ERROR_FATAL`; retain the
job and poll its status. If completion wins the race, the device returns
`QDMI_ERROR_INVALIDARGUMENT` and completed results remain available. Do not
resubmit or infer that a pending cancellation failed.

## Interpret atom ordering

Pulser's atom order is register insertion order, independent of label sorting,
coordinates or trap number. Pasqal SDK counter strings follow this order from
left to right. Core's bit convention puts atom index zero at the right, so this
provider reverses each SDK counter key exactly once.

For register labels `("z", "a", "m")`, an SDK key `"100"` (first atom excited)
becomes Core/QDMI key `"001"`. Leading zeros remain significant. Convert back
for Pulser/SDK comparison with
`{key[::-1]: value for key, value in counts.items()}`. Histogram keys and counts
stay associated; the sum must equal the program's run count. Chronological shot
ordering cannot be reconstructed from a counter and is not fabricated.

## Runnable example

With the `core` and `pulser` extras installed, the example submits a measured
two-atom register to EMU_FREE and prints the batch ID before waiting:

```console
python examples/native_pulser.py --auth-file ~/.config/pasqal-qdmi.auth
```

This explicitly submits a cloud job. Preserve the printed ID if the local wait
times out, and retrieve that job instead of resubmitting it.
