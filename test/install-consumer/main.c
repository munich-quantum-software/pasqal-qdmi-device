// Copyright (c) 2026 Munich Quantum Software Company GmbH
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <pasqal_qdmi/device.h>

int main(void) {
  PASQAL_QDMI_Device_Session session = NULL;
  if (PASQAL_QDMI_device_initialize() != QDMI_SUCCESS ||
      PASQAL_QDMI_device_session_alloc(NULL) != QDMI_ERROR_INVALIDARGUMENT ||
      PASQAL_QDMI_device_session_alloc(&session) != QDMI_SUCCESS ||
      session == NULL) {
    return 1;
  }
  PASQAL_QDMI_device_session_free(session);
  return PASQAL_QDMI_device_finalize() == QDMI_SUCCESS ? 0 : 1;
}
