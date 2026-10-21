// Copyright (c) 2026 Munich Quantum Software Company GmbH
// Copyright (c) 2026 Pasqal SAS
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
