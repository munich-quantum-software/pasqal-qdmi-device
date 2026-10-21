# Troubleshooting

**The device ID is missing.** Install the wheel into the same environment as MQT
Core and use Core >=4.1.1. Check `pasqal.qdmi.PASQAL_QDMI_CATALOG_PATH` and the
distribution entry point `mqt.core.qdmi.manifests`. A checkout alone is not an
installed package. Discovery does not require credentials.

**The native library cannot load.** Check the wheel's platform and architecture,
and ensure repaired TLS dependencies are present. Source installs must retain
the library and adjacent relative manifest. Run the relocated consumer check to
separate packaging from authentication.

**Authentication fails or expires.** Recheck your account credentials, auth file
and project membership. Use the documented authentication URL and create a fresh
session after replacing credentials. Never include passwords, bearer tokens,
authentication bodies or complete credential files in an issue.

**The project identifier is rejected.** Pasqal submission requires a project
UUID. Resolve an account's short project ID through its authenticated project
listing; do not guess or substitute another project.

**A wait times out.** A wait timeout does not cancel the cloud job. Preserve its
batch ID, inspect its status and retrieve it later. All live validation should
have a finite local deadline.

**Submission failed without a batch ID.** A dropped response can hide an
accepted batch. Do not retry automatically; reconcile the cloud project's jobs
first. The provider intentionally prevents repeated submission of the same
attempted QDMI job.

**Counts look reversed.** QDMI/Core uses the rightmost character for register
atom zero. The official SDK uses Pulser register order from the left. Reverse
each key once for comparison; do not sort atom labels or discard leading zeros.

**A property returns NOTSUPPORTED.** Unsupported properties are deliberate when
no trustworthy provider mapping exists. Examples include fixed calibrated sites,
gate operations, unknown queue position, chronological shot records and
statevectors. See [capabilities](capabilities.md).

For a report, include the package/Core/Pulser versions, platform, sanitized
error category, batch ID if known, and a minimal nonsecret sequence. Describe
whether the issue occurs with a local HTTP test or the free emulator. Keep
credential material and private endpoint responses out of reports.
