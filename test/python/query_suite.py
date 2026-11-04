# Copyright (c) 2026 Munich Quantum Software Company GmbH
# Copyright (c) 2026 Pasqal SAS
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Run the shared native query suite against the local Pasqal fixture."""

import contextlib
import os
import subprocess
import sys
import tempfile

from cloud import Cloud

with contextlib.ExitStack() as cleanup, tempfile.TemporaryDirectory() as home:
    cloud = Cloud(cleanup)
    env = os.environ.copy()
    for name in ("PASQAL_EMAIL", "PASQAL_PASSWORD", "PASQAL_PROJECT_ID", "PASQAL_AUTHFILE"):
        env.pop(name, None)
    for name in tuple(env):
        if name.startswith("GTEST_"):
            del env[name]
    env.update(HOME=home, PASQAL_QDMI_QUERY_URL=cloud.base, NO_PROXY="127.0.0.1", no_proxy="127.0.0.1")
    result = subprocess.run([sys.argv[1]], env=env, check=False).returncode  # ruff: ignore[subprocess-without-shell-equals-true] - executable comes from CMake
    if cloud.submissions or cloud.auth_calls:
        msg = "Query tests submitted a job or requested authentication"
        raise RuntimeError(msg)
    sys.exit(result)
