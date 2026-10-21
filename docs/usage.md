---
file_format: mystnb
kernelspec:
  name: python3
language_info:
  name: python
---

# Native Pulser usage

Install the `core` and `pulser` extras to run this page's examples. These cells
run during the documentation build without credentials or a cloud connection.
For execution on Pasqal Cloud, continue with [the EMU_FREE example](cloud.md).

## Build a sequence

Pulser defines the register and pulses. This two-atom sequence applies a global
pulse and measures in the Rydberg basis:

```{code-cell} ipython3
import math

from pulser import Pulse, Register, Sequence
from pulser.devices import MockDevice

register = Register({"left": (0, 0), "right": (10, 0)})
sequence = Sequence(register, MockDevice)
sequence.declare_channel("global", "rydberg_global")
sequence.add(Pulse.ConstantPulse(1000, math.pi, 0, 0), "global")
sequence.measure("ground-rydberg")
program = sequence.to_abstract_repr()
assert list(Sequence.from_abstract_repr(program).register.qubit_ids) == ["left", "right"]
print(sequence)
```

`Sequence.to_abstract_repr()` produces the native JSON transport format. Resolve
register mappings and symbolic variables before submission. Pulse construction
and validation belong to Pulser; this provider does not compile gate circuits
into pulses.

## Exercise the QDMI interface offline

MQT Core discovers the installed provider from its device catalogue. The `MOCK`
backend exercises the transport interface and returns all-zero counts; it is a
deterministic fixture, not a pulse simulator.

```{code-cell} ipython3
from mqt.core.qdmi import Job, ProgramFormat, builtin_driver

assert "pasqal.emu-free" in builtin_driver.registered_device_ids()
device = builtin_driver.open_device("pasqal.default", custom1="MOCK")
job = device.submit_job(program, ProgramFormat.CUSTOM1, num_shots=20)
assert job.wait(timeout=5)
assert job.check() == Job.Status.DONE
counts = job.get_counts()
assert counts == {"00": 20}
print(counts)
```

## Multiple programs

Pass an ordered list of built JSON sequences with one common shot count. The
program index identifies the matching result:

```{code-cell} ipython3
job = device.submit_job([program, program], ProgramFormat.CUSTOM1, num_shots=20)
assert job.wait(timeout=5)
assert job.num_programs == 2
print([job.get_counts(program_index=i) for i in range(job.num_programs)])
```

On Pasqal Cloud, the provider sends one closed batch containing the ordered
programs. Different shot counts per program and appending to an existing batch
are not exposed. See
[submission and recovery](capabilities.md#submission-and-recovery) for
retrieval, cancellation and failure behavior.

## Interpret atom ordering

Pulser's atom order is register insertion order, independent of label sorting,
coordinates or trap number. Pasqal SDK counter strings follow this order from
left to right. Core puts atom index zero at the right, so the provider reverses
each SDK counter key once.

For register labels `("z", "a", "m")`, an SDK key `"100"` (first atom excited)
becomes Core/QDMI key `"001"`. Leading zeros remain significant. Convert back
for Pulser/SDK comparison with
`{key[::-1]: value for key, value in counts.items()}`. Histogram keys and counts
stay associated; the sum equals the program's run count. Chronological shot
ordering cannot be reconstructed from a counter.
