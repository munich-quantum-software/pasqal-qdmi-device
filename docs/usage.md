---
file_format: mystnb
kernelspec:
  name: python3
language_info:
  name: python
---

# Native Pulser usage

Install the `mqt-core` and `pulser` extras and
[configure your credentials](authentication.md). This example submits one batch
of two small programs to Pasqal's free EMU_FREE service. Other emulators can
consume credits.

## Connect to EMU_FREE

`QDMIConnection` connects Pulser's `QPUBackend` to the installed provider
through MQT Core. Its `device` exposes Core's native QDMI interface. The native
library reads `PASQAL_EMAIL`, `PASQAL_PASSWORD` and `PASQAL_PROJECT_ID` and
manages access tokens.

```{code-cell} ipython3
import json

from mqt.core.qdmi import CustomProperty, Job, ProgramFormat
from pasqal.qdmi.pulser import QDMIConnection

connection = QDMIConnection("pasqal.emu-free")
device = connection.device
metadata = json.loads(device.query_custom_property(CustomProperty.CUSTOM2, str))
assert metadata["device_type"] == "EMU_FREE"
assert metadata["is_emulator"] is True and metadata["bill_usage"] is False
print(f"EMU_FREE: {device.qubits_num()} atoms")
```

The selected catalogue ID determines where programs run. `QPUBackend` validates
hardware constraints even when the connection targets EMU_FREE.
Emulator-specific backend options are unsupported.

## Build the programs

Pulser defines the register, pulses and measurement. Use the public device
specifications exposed by the connection for `QPUBackend`. This example uses
FRESNEL_CAN1's calibrated layout with traps at `(-5, 0)`, `(0, 0)` and `(5, 0)`
µm. Two sweep durations prepare the antiferromagnetic state `101`:

```{code-cell} ipython3
import math

from pulser import Pulse, QPUBackend, Sequence
from pulser.waveforms import InterpolatedWaveform, RampWaveform

fresnel = connection.fetch_available_devices()["FRESNEL_CAN1"]
layout = next(l for l in fresnel.pre_calibrated_layouts if l.number_of_traps == fresnel.min_layout_traps)
traps = [layout.get_traps_from_coordinates((x, 0.0))[0] for x in (-5.0, 0.0, 5.0)]
register = layout.define_register(*traps, qubit_ids=["z", "a", "m"])
sequence = Sequence(register, fresnel)
sequence.declare_channel("ising", "rydberg_global")
duration = sequence.declare_variable("duration", dtype=int)
omega, delta = 2 * math.pi, 4 * math.pi
amplitude = InterpolatedWaveform(duration, [1e-9, omega, omega, 1e-9])
sequence.add(Pulse(amplitude, RampWaveform(duration, -delta, delta), 0), "ising")
sequence.measure("ground-rydberg")
job_params = [{"runs": 100, "variables": {"duration": value}} for value in (4000, 6000)]
```

The connection builds one program per parameter set, including register mappings
when present. Without variables or a mappable register, each parameter set
repeats the same program. Pulse construction and validation belong to Pulser;
the native provider accepts built abstract JSON and does not compile gate
circuits.

## Submit and wait

Choose Pulser's backend or Core's native interface for submission. Both send one
closed batch with a common shot count. The executable example uses Pulser:

```{code-cell} ipython3
results = QPUBackend(sequence, connection).run(job_params)
print("Batch ID:", results.batch_id, flush=True)
```

The equivalent Core submission accepts serialized programs in input order:

```python
programs = [sequence.build(**params["variables"]).to_abstract_repr() for params in job_params]
job = device.submit_job(programs, ProgramFormat.CUSTOM1, num_shots=100)
```

Keep the batch ID before waiting. A timeout does not cancel the job: retrieve
that ID later instead of submitting again. Core callers can wait on the returned
`job` directly. For the Pulser path, retrieve Core's job handle to bound the
wait:

```{code-cell} ipython3
job = device.retrieve_job_by_id(results.batch_id)
assert job.wait(timeout=300), f"Retrieve pending job {results.batch_id} later"
assert job.check() == Job.Status.DONE
counts = [result.final_bitstrings for result in results.results]
for counter in counts:
    assert sum(counter.values()) == 100 and counter.get("101", 0) >= 90
print(counts)
```

Pulser job IDs are `<batch ID>#<program index>`. Open batches, appending
programs and differing shot counts within a batch are unsupported. See
[submission and recovery](capabilities.md#submission-and-recovery) for failed
submissions and cancellation.

## Retrieve from a fresh session

```{code-cell} ipython3
from pulser.backend.remote import RemoteResults

fresh = RemoteResults(results.batch_id, QDMIConnection("pasqal.emu-free"))
assert [result.final_bitstrings for result in fresh.results] == counts
assert [result.atom_order for result in fresh.results] == [("z", "a", "m")] * 2
```

Core callers use `device.retrieve_job_by_id(batch_id)` and access programs and
results by their input index. Pulser callers can select individual results with
`RemoteResults(batch_id, connection, job_ids=[...])`.

## Interpret atom ordering

Pulser's atom order is register insertion order, independent of label sorting,
coordinates or trap number. Pulser and Pasqal SDK counter strings follow this
order from left to right. Core/QDMI puts atom index zero at the right.

For labels `("z", "a", "m")`, Pulser key `"100"` becomes Core/QDMI key `"001"`.
The connection performs the conversion when returning Pulser results. Native
Core counts can be compared explicitly:

```{code-cell} ipython3
assert [{bits[::-1]: count for bits, count in job.get_counts(i).items()} for i in range(2)] == counts
```

Leading zeros remain significant, and each counter's total equals its run count.
Chronological shot ordering cannot be reconstructed from a counter.
