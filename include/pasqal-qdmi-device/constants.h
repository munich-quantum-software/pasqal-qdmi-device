/*
 * Pasqal-specific names for the custom QDMI values used by the Pasqal device.
 */

#pragma once

#include "pasqal_qdmi/constants.h" // IWYU pragma: export

/// Session parameter selecting the backend: "EMU_FREE", "EMU_MPS", "EMU_FRESNEL",
/// "FRESNEL", "FRESNEL_CAN1" or "MOCK". Defaults to $PASQAL_QDMI_BACKEND, then "EMU_FREE".
#define PASQAL_QDMI_DEVICE_SESSION_PARAMETER_BACKEND                           \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1

/// Session parameter setting the Pasqal Cloud project ID.
#define PASQAL_QDMI_DEVICE_SESSION_PARAMETER_PROJECTID                         \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3

/// Device property holding the Pulser devices the session's backend runs, as a JSON
/// array of {"device_type", "specs"} with the specs in Pulser's abstract
/// representation: the one device for a QPU, the real devices' public specs for an
/// emulator. Not supported for the mock device.
#define PASQAL_QDMI_DEVICE_PROPERTY_PULSERSPECS QDMI_DEVICE_PROPERTY_CUSTOM1

/// Program format for a Pulser sequence in abstract representation (JSON).
#define PASQAL_QDMI_PROGRAM_FORMAT_PULSER QDMI_PROGRAM_FORMAT_CUSTOM1
