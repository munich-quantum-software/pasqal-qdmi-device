# Pasqal QDMI Device

Submit native Pulser programs to Pasqal through the Quantum Device Management Interface (QDMI). This device is maintained by Munich Quantum Software Company (MQSC). Pasqal is a full-stack neutral-atom quantum computing company.

Pulser builds the pulse sequence. MQT Core discovers the installed device and manages sessions, jobs and results. The C++ provider authenticates with Pasqal and transports the sequence. There is no gate-to-pulse compiler in this package.

Version 1.0 supports built, measured Pulser programs on EMU_FREE. Backend metadata currently reports a maximum of 12 atoms. Hardware execution has not been validated.

```{toctree}
:maxdepth: 2

installation
authentication
usage
capabilities
troubleshooting
maintenance
development
```
