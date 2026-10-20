# Capabilities and limits

The native interface targets released QDMI 1.4.0. Python integration targets released MQT Core 4.1.1; no unreleased shared dependency is required for the supported path. The scope is native Pulser transport to EMU_FREE, with no claim of gate-based frontend compilation or hardware validation.

| QDMI capability | Behavior |
| --- | --- |
| Discovery | Installed manifest IDs `pasqal.default` and `pasqal.emu-free` select EMU_FREE. Discovery itself needs no authentication. |
| Authentication | Standard username/password/auth URL/auth file fields; in-memory access-token lifecycle. |
| Metadata | Selected backend capacity and status come from `/devices`; raw Pulser constraints are a separate custom property. Current EMU_FREE metadata reports 12 atoms and unbilled emulator execution. |
| Sites and operations | Empty standard lists. A program defines its own register; a Pulser channel is a pulse control resource, not an advertised gate. No fabricated calibrated traps or square zones. |
| Program format | `CUSTOM1`: built Pulser abstract JSON, including register insertion order and measurement basis. OpenQASM, QIR, unbound variables and other formats are unsupported. |
| Batching | Ordered programs with common shot count, one closed native Pasqal batch; indexed status/results and fresh-session retrieval. |
| Results | Validated histogram keys and integer counts; atom zero appears at the right in Core/QDMI. Counter totals must match runs. Raw chronological shots, statevectors and probability arrays are unsupported. |
| Job retrieval | Native batch ID; legacy individual cloud job IDs may be retrieved as a single program. |
| Cancellation | Request followed by one refresh: confirmed canceled returns success; pending confirmation returns FATAL (Core raises; poll status); already completed returns INVALIDARGUMENT and retains results. |
| Queue | Validated backend queue length sums waiting jobs. Queue position returns `QDMI_ERROR_NOTSUPPORTED`: the published batch-position field does not establish QDMI's required number-of-jobs-ahead meaning. |

## Custom field contract

| Field | Type and meaning | Lifetime/units |
| --- | --- | --- |
| Session `CUSTOM1` / `BACKEND` | NUL-terminated UTF-8 backend identifier; installed IDs select `EMU_FREE` | Copied during configuration, fixed once initialized; unitless |
| Session `CUSTOM3` / `PROJECTID` | NUL-terminated UTF-8 Pasqal project UUID | Copied during configuration, fixed once initialized; unitless |
| Device `CUSTOM1` / `PULSERSPECS` | NUL-terminated UTF-8 JSON array with full Pulser device specifications | Immutable session snapshot; retain the source schema's units (e.g. duration ns, positions µm, angular frequency rad/µs), without converting them into unrelated standard properties |
| Device `CUSTOM2` / backend metadata | NUL-terminated UTF-8 JSON selected `/devices` entry: capacity, status, emulator/billing flags and other provider metadata | Immutable session snapshot; counts are dimensionless; no invented calibration lifetime |
| Program format `CUSTOM1` / `PULSER` | UTF-8 JSON from a built `Sequence.to_abstract_repr()` | Copied into job; units and measurement basis follow Pulser serialization |

The generated public headers plus `pasqal-qdmi-device/constants.h` define symbolic aliases. Standard buffer sizing applies: first query required bytes, then provide at least that much storage; string byte counts include the terminating NUL. Result buffers are caller-owned. Keep a C session alive until its jobs are freed; Core owns these relationships through its bindings.

## Submission and recovery

The cloud API does not document an idempotency key for batch creation. The provider never automatically repeats a POST after a lost response. If the response is lost before a batch ID is known, work may have been accepted remotely; do not blindly resubmit. Inspect the project's cloud jobs or contact the provider to reconcile the ambiguity.

Once a batch ID is known, later metadata failures do not require resubmission. Retain that ID and retry safe reads or retrieve from a fresh session. A repeated submit on the same attempted QDMI job is rejected. Native batching avoids the former sequence of independent POSTs and its partial-submission ambiguity.

Core 4.1.1 raises on failed submission and does not return the failed job object. General recovery of provider-specific submission diagnostics needs shared Core functionality; this package does not introduce a second Python binding to bypass that behavior.

Cancellation cannot undo already completed work. Queue data, backend capacity and availability can change; runtime metadata takes precedence over historical documentation. EMU_FREE success establishes emulator compatibility only.
