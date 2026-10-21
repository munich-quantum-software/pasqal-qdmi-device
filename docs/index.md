# Pasqal QDMI Device

Submit native Pulser programs to Pasqal through the Quantum Device Management
Interface (QDMI). This device is developed by Munich Quantum Software Company
(MQSC) and Pasqal.

Pulser builds the pulse sequence. MQT Core discovers the installed device and
manages sessions, jobs and results. The C++ provider authenticates with Pasqal
and transports the sequence. There is no gate-to-pulse compiler in this package.

Version 1.0 accepts built, measured Pulser programs. The catalogue includes
Pasqal Cloud processors and emulators; account permissions and regional
availability govern access. EMU_FREE is validated end to end. Hardware execution
has not been validated.

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
