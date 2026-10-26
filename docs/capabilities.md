# Capabilities and limits

The native interface targets QDMI 1.4.0. Python integration uses MQT Core 4.1.1
or later, directly or through Pulser's `QPUBackend` with `QDMIConnection`.
Programs use native Pulser transport; gate-based frontend compilation is
unsupported. EMU_FREE has been validated end to end; hardware execution has not.

## Device catalogue

The installed catalogue follows the
[official cloud offering](https://docs.pasqal.com/qpu-emulators/) and the public
device API. It includes systems independently of the current account's execution
permissions.

| Installed ID                        | Cloud backend                 | Region                     |
| ----------------------------------- | ----------------------------- | -------------------------- |
| `pasqal.default`, `pasqal.emu-free` | EMU_FREE                      | Global                     |
| `pasqal.emu-fresnel`                | EMU_FRESNEL                   | Global                     |
| `pasqal.emu-mps`                    | EMU_MPS                       | Global                     |
| `pasqal.emu-sv`                     | EMU_SV                        | Global                     |
| `pasqal.emu-tn`                     | EMU_TN (deprecated by Pasqal) | Global                     |
| `pasqal.fresnel`                    | FRESNEL                       | Global                     |
| `pasqal.fresnel-can1`               | FRESNEL_CAN1                  | Global; hardware in Canada |
| `pasqal.fresnel-sa1`                | FRESNEL_SA1                   | Saudi Arabia               |

Listing these IDs is offline. Opening a device reads current cloud metadata;
public visibility does not grant permission to execute. Capacity, status,
billing and account access come from the service. FRESNEL may be inactive even
while its metadata remains available to authenticated users. Retired EMU_C is
not included because it does not support the Pulser counter-result contract.

FRESNEL_SA1 selects the Saudi API endpoint. Execution there requires a bearer
token issued for that region and an authorized regional project; global
username/password authentication does not apply. No hardware execution has been
validated.

## QDMI capabilities

| QDMI capability      | Behavior                                                                                                                                                                                                |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Discovery            | Installed catalogue IDs cover Pasqal Cloud systems; `pasqal.default` selects EMU_FREE unless configured otherwise. Discovery itself needs no authentication.                                            |
| Authentication       | Standard username/password/auth URL/auth file fields; in-memory access-token lifecycle.                                                                                                                 |
| Metadata             | Selected backend capacity and status come from `/devices`; raw Pulser constraints are a separate custom property. Current EMU_FREE metadata reports 12 atoms and unbilled emulator execution.           |
| Sites and operations | Empty standard lists. A program defines its own register; a Pulser channel is a pulse control resource, not an advertised gate. No fabricated calibrated traps or square zones.                         |
| Program format       | `CUSTOM1`: built Pulser abstract JSON, including register insertion order and measurement basis. OpenQASM, QIR, unbound variables and other formats are unsupported.                                    |
| Batching             | Ordered programs with common shot count, one closed native Pasqal batch; indexed status/results and fresh-session retrieval.                                                                            |
| Results              | Validated histogram keys and integer counts; atom zero appears at the right in Core/QDMI. Counter totals must match runs. Raw chronological shots, statevectors and probability arrays are unsupported. |
| Job retrieval        | Native batch ID; individual cloud job IDs may be retrieved as a single program.                                                                                                                         |
| Cancellation         | Request followed by one refresh: confirmed canceled returns success; pending confirmation returns FATAL (Core raises; poll status); already completed returns INVALIDARGUMENT and retains results.      |
| Queue                | Validated backend queue length sums waiting jobs. Queue position returns `QDMI_ERROR_NOTSUPPORTED`: the published batch-position field does not establish QDMI's required number-of-jobs-ahead meaning. |

## Custom field contract

| Field                               | Type and meaning                                                                                                          | Lifetime/units                                                                                                                                                                      |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Session `CUSTOM1` / `BACKEND`       | NUL-terminated UTF-8 backend identifier; installed IDs select the corresponding cloud backend                             | Copied during configuration, fixed once initialized; unitless                                                                                                                       |
| Session `CUSTOM3` / `PROJECTID`     | NUL-terminated UTF-8 Pasqal project UUID                                                                                  | Copied during configuration, fixed once initialized; unitless                                                                                                                       |
| Device `CUSTOM1` / `PULSERSPECS`    | NUL-terminated UTF-8 JSON array with full Pulser device specifications                                                    | Immutable session snapshot; retain the source schema's units (e.g. duration ns, positions µm, angular frequency rad/µs), without converting them into unrelated standard properties |
| Device `CUSTOM2` / backend metadata | NUL-terminated UTF-8 JSON selected `/devices` entry: capacity, status, emulator/billing flags and other provider metadata | Immutable session snapshot; counts are dimensionless; no invented calibration lifetime                                                                                              |
| Program format `CUSTOM1` / `PULSER` | UTF-8 JSON from a built `Sequence.to_abstract_repr()`                                                                     | Copied into job; units and measurement basis follow Pulser serialization                                                                                                            |

The generated public headers plus `pasqal-qdmi-device/constants.h` define
symbolic aliases. Standard buffer sizing applies: first query required bytes,
then provide at least that much storage; string byte counts include the
terminating NUL. Result buffers are caller-owned. Keep a C session alive until
its jobs are freed; Core owns these relationships through its bindings. Calls on
one session and all of its jobs must be serialized. Separate sessions may run in
parallel.

## Submission and recovery

The cloud API does not document an idempotency key for batch creation. The
provider never automatically repeats a POST after a lost response. If the
response is lost before a batch ID is known, work may have been accepted
remotely; do not blindly resubmit. Inspect the project's cloud jobs or contact
the provider to reconcile the ambiguity.

Once a batch ID is known, later metadata failures do not require resubmission.
Retain that ID and retry safe reads or retrieve from a fresh session. A repeated
submit on the same attempted QDMI job is rejected. The Pulser connection
includes an accepted batch ID in the exception note if waiting fails.

Core raises on failed submission and does not return the failed job object. If
no batch ID is available, reconcile the submission through Pasqal Cloud before
creating another batch.

Cancellation cannot undo already completed work. Queue data, backend capacity
and availability can change; runtime metadata takes precedence over historical
documentation. EMU_FREE success establishes emulator compatibility only.
