#!/usr/bin/env python3
# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Explicit, bounded EMU_FREE validation through installed MQT Core; never a QPU."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import stat
import subprocess
import sys
import tempfile
from datetime import UTC, datetime
from importlib.metadata import version
from pathlib import Path


def credential(name: str, env: str) -> str:
    if os.environ.get(env):
        return os.environ[env]
    directory = Path.home() / ".config/codex/pasqal-qdmi-device"
    path = directory / name
    for entry in (directory, path):
        mode = entry.lstat()
        if stat.S_ISLNK(mode.st_mode) or mode.st_uid != os.getuid() or mode.st_mode & 0o077:
            raise PermissionError("credential path ownership or permissions are unsafe")
    value = path.read_text().removesuffix("\n")
    if not value or "\n" in value or "\r" in value:
        raise ValueError("credential file must contain one nonempty line")
    return value


def programs() -> list[str]:
    from pulser import Pulse, Register, Sequence
    from pulser.devices import MockDevice

    result = []
    for target in ("z", "m"):
        sequence = Sequence(Register({"z": (10, 0), "a": (-10, 0), "m": (0, 10)}), MockDevice)
        sequence.declare_channel("local", "rydberg_local", initial_target=target)
        sequence.add(Pulse.ConstantPulse(1000, math.pi, 0, 0), "local")
        sequence.measure("ground-rydberg")
        result.append(sequence.to_abstract_repr())
    return result


def validate(args, evidence: dict) -> None:
    from mqt.core.qdmi import CustomProperty, Job, ProgramFormat, builtin_driver
    from pasqal_cloud.pasqal_cloud_client import PasqalCloudClient

    email = credential("email", "PASQAL_EMAIL")
    password = credential("password", "PASQAL_PASSWORD")
    project_hint = credential("project-id", "PASQAL_PROJECT_ID")
    evidence["phase"] = "authenticated project resolution"
    sdk = PasqalCloudClient(username=email, password=password)
    matches = [
        p.id
        for p in sdk.get_all_projects()
        if p.id == project_hint or p.id.startswith(project_hint)
    ]
    if len(matches) != 1:
        raise ValueError("project identifier must match exactly one accessible active project")
    project = matches[0]
    sdk.project_id = project

    def open_device():
        return builtin_driver.open_device(
            "pasqal.emu-free", username=email, password=password, custom3=project
        )

    evidence["phase"] = "free backend metadata verification"
    device = open_device()
    metadata = json.loads(device.query_custom_property(CustomProperty.CUSTOM2, str))
    if not (
        metadata["device_type"] == "EMU_FREE"
        and metadata["is_emulator"] is True
        and metadata["bill_usage"] is False
        and metadata["max_atom_num"] == 12
        and metadata["availability"] == "ACTIVE"
        and metadata["status"] == "UP"
    ):
        raise RuntimeError(
            "current backend identity, limits or billing differs from approved EMU_FREE contract"
        )
    evidence["backend"] = {
        k: metadata[k]
        for k in (
            "device_type",
            "max_atom_num",
            "is_emulator",
            "bill_usage",
            "status",
            "availability",
        )
    }
    source = programs()
    evidence["program_sha256"] = [hashlib.sha256(p.encode()).hexdigest() for p in source]
    evidence["atom_order"] = ["z", "a", "m"]
    evidence["shots_per_program"] = 100
    evidence["acceptance"] = {
        "core_expected": ["001", "100"],
        "minimum_expected_fraction": 0.98,
        "rationale": "isolated resonant pi pulse; independent local occupations within 1e-5",
    }
    evidence["phase"] = "submission" if not args.retrieve else "retrieval"
    write(args.output, evidence)
    if args.retrieve:
        job = device.retrieve_job_by_id(args.retrieve)
    else:
        # A failed/ambiguous submission is never retried by this script.
        job = device.submit_job(source, ProgramFormat.CUSTOM1, num_shots=100)
    evidence["job_id"] = job.id
    evidence["phase"] = "waiting"
    write(args.output, evidence)  # Retain the receipt even if waiting is interrupted.
    if not job.wait(timeout=300):
        raise TimeoutError(
            "bounded cloud wait expired; retrieve the recorded ID without resubmitting"
        )
    if job.check() != Job.Status.DONE or job.num_programs != 2:
        raise RuntimeError("cloud job did not complete both programs successfully")
    counts = [job.get_counts(i) for i in range(2)]
    for actual, expected in zip(counts, ("001", "100"), strict=True):
        if sum(actual.values()) != 100 or actual.get(expected, 0) < 98:
            raise RuntimeError("counts failed the documented analytic acceptance threshold")
    restored = open_device().retrieve_job_by_id(job.id)
    if [restored.get_counts(i) for i in range(2)] != counts:
        raise RuntimeError("fresh-session results differ")
    if [json.loads(restored.get_program(i)) for i in range(2)] != [json.loads(p) for p in source]:
        raise RuntimeError("fresh-session program association differs")
    batch_id = job.id.removeprefix("pasqal-batch:")
    official = sdk.get_batch(batch_id)
    official_counts = [j.result for j in official.ordered_jobs]
    if [
        {bits[::-1]: count for bits, count in counter.items()} for counter in official_counts
    ] != counts:
        raise RuntimeError("official SDK interpretation differs from QDMI")
    evidence.update(
        phase="complete",
        counts=counts,
        sdk_counts=official_counts,
        cloud_job_ids=[j.id for j in official.ordered_jobs],
        fresh_session=True,
        sdk_parity=True,
        hardware_validated=False,
    )


def write(path: Path, evidence: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(json.dumps(evidence, indent=2) + "\n")
    temporary.replace(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--run-live", action="store_true", help="authorize this bounded free-emulator check"
    )
    parser.add_argument("--retrieve", help="retrieve an existing validation ID; never submit again")
    parser.add_argument("--output", type=Path, default=Path("build/emu-free-evidence.json"))
    parser.add_argument("--_worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.run_live:
        parser.error("explicit --run-live is required")
    if not args._worker:
        if args.output.exists() and not args.retrieve:
            parser.error(
                "evidence already exists; use --retrieve or a new output path for a new job"
            )
        # Bound SDK authentication and reads as well as the native job wait.
        try:
            return subprocess.run(
                [sys.executable, __file__, *sys.argv[1:], "--_worker"],
                timeout=600,
                check=False,
            ).returncode
        except subprocess.TimeoutExpired:
            receipt = json.loads(args.output.read_text()) if args.output.exists() else {}
            receipt.update(outcome="failed", error_type="OverallTimeout")
            write(args.output, receipt)
            print(
                "Validation exceeded 600 seconds. Retain any recorded ID; do not resubmit.",
                file=sys.stderr,
            )
            return 1
    evidence = {
        "started_utc": datetime.now(UTC).isoformat(),
        "versions": {
            p: version(p) for p in ("pasqal-qdmi", "mqt-core", "pulser-core", "pasqal-cloud")
        },
    }
    if args.retrieve:
        evidence["job_id"] = args.retrieve
    try:
        validate(args, evidence)
    except Exception as error:
        # SDK exceptions may contain HTTP response bodies; never print them.
        evidence.update(outcome="failed", error_type=type(error).__name__)
        write(args.output, evidence)
        print(
            f"Validation failed during {evidence.get('phase', 'setup')} ({type(error).__name__}).",
            file=sys.stderr,
        )
        print(
            "If a job ID was recorded, use --retrieve with that ID; do not repeat submission.",
            file=sys.stderr,
        )
        return 1
    evidence["outcome"] = "passed"
    write(args.output, evidence)
    print(f"EMU_FREE validation passed; sanitized evidence: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
