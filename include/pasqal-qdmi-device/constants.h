// Copyright (c) 2026 Munich Quantum Software Company GmbH
// All rights reserved.
//
// Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://llvm.org/LICENSE.txt
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Pasqal-specific names for the custom QDMI values used by the Pasqal device.

#pragma once

#include "pasqal_qdmi/constants.h" // IWYU pragma: export

/// NUL-terminated UTF-8 session backend, copied and fixed at init: "EMU_FREE",
/// "EMU_MPS", "EMU_FRESNEL", "FRESNEL", "FRESNEL_CAN1" or "MOCK". Defaults to
/// $PASQAL_QDMI_BACKEND, then "EMU_FREE".
#define PASQAL_QDMI_DEVICE_SESSION_PARAMETER_BACKEND                           \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1

/// NUL-terminated UTF-8 Pasqal project ID, copied and fixed at session init.
#define PASQAL_QDMI_DEVICE_SESSION_PARAMETER_PROJECTID                         \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3

/// NUL-terminated UTF-8 JSON array of public Pulser constraint metadata entries
/// {"device_type", "specs"}, copied from Pasqal as a session snapshot. These
/// are candidate device models, not an exhaustive list of emulator-compatible
/// sequences: emulators can also accept Pulser VirtualDevice models.
/// Not supported for the offline mock fixture.
#define PASQAL_QDMI_DEVICE_PROPERTY_PULSERSPECS QDMI_DEVICE_PROPERTY_CUSTOM1

/// NUL-terminated UTF-8 JSON for a built, measured Pulser abstract sequence.
/// The device copies the payload. Histogram bit i addresses serialized
/// register[i], with register[0] rightmost. Pulse construction and validation
/// remain in Pulser.
#define PASQAL_QDMI_PROGRAM_FORMAT_PULSER QDMI_PROGRAM_FORMAT_CUSTOM1

/// Session snapshot of the selected /devices backend entry, as NUL-terminated
/// UTF-8 JSON. Includes capacity, status, emulator and billing flags; no
/// credentials.
#define PASQAL_QDMI_DEVICE_PROPERTY_BACKENDMETADATA QDMI_DEVICE_PROPERTY_CUSTOM2
