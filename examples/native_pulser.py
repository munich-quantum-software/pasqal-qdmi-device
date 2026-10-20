# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Native Pulser usage. Authentication is supplied by a private QDMI auth file."""

from __future__ import annotations

import argparse

from mqt.core.qdmi import ProgramFormat, builtin_driver
from pulser import Register, Sequence
from pulser.devices import MockDevice


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--auth-file", required=True)
    args = parser.parse_args()
    sequence = Sequence(Register({"left": (0, 0), "right": (10, 0)}), MockDevice)
    sequence.measure("ground-rydberg")
    device = builtin_driver.open_device("pasqal.emu-free", auth_file=args.auth_file)
    job = device.submit_job(sequence.to_abstract_repr(), ProgramFormat.CUSTOM1, num_shots=100)
    print("Keep this ID for retrieval:", job.id)
    if job.wait(timeout=300):
        print(job.check(), job.get_counts())


if __name__ == "__main__":
    main()
