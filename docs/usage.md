---
file_format: mystnb
kernelspec:
  name: python3
language_info:
  name: python
---

# Native Pulser usage

Install the `core` and `pulser` extras and
[configure your credentials](authentication.md). This page runs on Pasqal's free
EMU_FREE service during the documentation build. It submits two small batches.
Other emulators can consume credits.

## Connect to EMU_FREE

MQT Core discovers the installed provider. The native library reads
`PASQAL_EMAIL`, `PASQAL_PASSWORD` and `PASQAL_PROJECT_ID`, then manages its own
access tokens.

```{code-cell} ipython3
import json

from mqt.core.qdmi import CustomProperty, Job, ProgramFormat, builtin_driver

device = builtin_driver.open_device("pasqal.emu-free")
metadata = json.loads(device.query_custom_property(CustomProperty.CUSTOM2, str))
assert metadata["device_type"] == "EMU_FREE"
assert metadata["is_emulator"] is True and metadata["bill_usage"] is False
print(f"EMU_FREE: {device.qubits_num()} atoms")
```

## Build the programs

Pulser defines the register, pulses and measurement. Its `MockDevice` supplies
ideal emulator constraints; execution takes place on Pasqal Cloud. These two
sequences excite the left and right atoms, respectively:

```{code-cell} ipython3
import math

from pulser import Pulse, Register, Sequence
from pulser.devices import MockDevice

register = Register({"left": (-10, 0), "right": (10, 0)})
programs = []
for target in register.qubit_ids:
    sequence = Sequence(register, MockDevice)
    sequence.declare_channel("local", "rydberg_local", initial_target=target)
    sequence.add(Pulse.ConstantPulse(1000, math.pi, 0, 0), "local")
    sequence.measure("ground-rydberg")
    programs.append(sequence.to_abstract_repr())
print(sequence)
```

`Sequence.to_abstract_repr()` produces the native JSON transport format. Resolve
register mappings and symbolic variables before submission. Pulse construction
and validation belong to Pulser; this provider does not compile gate circuits
into pulses.

## Submit and wait

Pass one JSON string or an ordered list of programs with a common shot count.
The provider sends one closed batch; the program index identifies each result.

```{code-cell} ipython3
job = device.submit_job(programs, ProgramFormat.CUSTOM1, num_shots=100)
print("Batch ID:", job.id, flush=True)
```

Keep the job ID before waiting. A timeout does not cancel the job: retrieve that
ID later instead of submitting the programs again. Never automatically retry a
submission whose response was lost.

```{code-cell} ipython3
assert job.wait(timeout=300), f"Retrieve pending job {job.id} later"
assert job.check() == Job.Status.DONE
assert job.num_programs == 2
counts = [job.get_counts(i) for i in range(job.num_programs)]
for actual, expected in zip(counts, ("01", "10"), strict=True):
    assert sum(actual.values()) == 100 and actual.get(expected, 0) >= 98
print(counts)
```

Different shot counts per program and appending to an existing batch are not
exposed. See [submission and recovery](capabilities.md#submission-and-recovery)
for cancellation and failure behavior.

## Retrieve from a fresh session

```{code-cell} ipython3
fresh_device = builtin_driver.open_device("pasqal.emu-free")
retrieved = fresh_device.retrieve_job_by_id(job.id)
assert [retrieved.get_counts(i) for i in range(2)] == counts
assert [json.loads(retrieved.get_program(i)) for i in range(2)] == [json.loads(p) for p in programs]
```

## Interpret atom ordering

Pulser's atom order is register insertion order, independent of label sorting,
coordinates or trap number. Pasqal SDK counter strings follow this order from
left to right. Core puts atom index zero at the right, so the provider reverses
each SDK counter key once.

For register labels `("z", "a", "m")`, an SDK key `"100"` (first atom excited)
becomes Core/QDMI key `"001"`. Leading zeros remain significant. Convert back
for Pulser/SDK comparison with
`{key[::-1]: value for key, value in counts[0].items()}`. Histogram keys and
counts stay associated; the sum equals the program's run count. Chronological
shot ordering cannot be reconstructed from a counter.

## Run through a Pulser backend

`pasqal.qdmi.pulser.QDMIConnection` is a Pulser remote connection over this
device, for use with `QPUBackend`. Emulator-specific backend options are
unsupported. It returns Pulser devices built from the cloud's public
specifications. Build against them so Pulser validates the real constraints.
FRESNEL_CAN1 requires a pre-calibrated layout; its 60-trap layout accepts small
registers. This adiabatic sweep on three atoms 5 µm apart prepares the
antiferromagnetic state `101`:

```{code-cell} ipython3
from pulser import QPUBackend
from pulser.waveforms import InterpolatedWaveform, RampWaveform

from pasqal.qdmi.pulser import QDMIConnection

connection = QDMIConnection("pasqal.emu-free")
fresnel = connection.fetch_available_devices()["FRESNEL_CAN1"]
layout = next(l for l in fresnel.pre_calibrated_layouts if l.number_of_traps == fresnel.min_layout_traps)
traps = [layout.get_traps_from_coordinates((x, 0.0))[0] for x in (-5.0, 0.0, 5.0)]
register = layout.define_register(*traps, qubit_ids=["q0", "q1", "q2"])

omega, delta, duration = 2 * math.pi, 4 * math.pi, 4000
sequence = Sequence(register, fresnel)
sequence.declare_channel("ising", "rydberg_global")
amplitude = InterpolatedWaveform(duration, [1e-9, omega, omega, 1e-9])
sequence.add(Pulse(amplitude, RampWaveform(duration, -delta, delta), 0), "ising")
sequence.measure("ground-rydberg")

results = QPUBackend(sequence, connection).run([{"runs": 100}], wait=True)
counts = results.results[0].final_bitstrings
assert counts.get("101", 0) >= 90
print(results.batch_id, dict(counts))
```

Each Pulser job is one program of a single closed batch, with the job ID
`<batch ID>#<index>`. All jobs share one run count; open batches and appending
to a batch are not supported. Counts follow Pulser's register order, so they
need no reversal. `RemoteResults(batch_id, QDMIConnection(...))` retrieves a
batch from a new session.
