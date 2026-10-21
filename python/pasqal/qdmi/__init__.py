# Copyright (c) 2026 Munich Quantum Software Company GmbH
# Copyright (c) 2025 - 2026 Pasqal SAS
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

"""Installed artifacts for the Pasqal QDMI device.

MQT Core discovers the catalogue without importing this module or loading the
native device. Use ``mqt.core.qdmi`` for sessions and jobs.
"""

import sys
from importlib.metadata import distribution
from pathlib import Path

__version__ = distribution("pasqal-qdmi").version
_DATA = Path(str(distribution("pasqal-qdmi").locate_file("pasqal/qdmi/data"))).resolve()
_LIBRARY_DIR = _DATA / ("bin" if sys.platform == "win32" else "lib")
PASQAL_QDMI_DEVICE_ID = "pasqal.default"
PASQAL_QDMI_PREFIX = "PASQAL"
PASQAL_QDMI_CATALOG_PATH = _LIBRARY_DIR / "pasqal-qdmi-device.qdmi.json"
PASQAL_QDMI_INCLUDE_DIR = _DATA / "include"
PASQAL_QDMI_CMAKE_DIR = _DATA / "share" / "cmake"
if sys.platform == "win32":
    _NAMES = ("pasqal-qdmi-device.dll", "libpasqal-qdmi-device.dll")
else:
    _NAMES = ("libpasqal-qdmi-device." + ("dylib" if sys.platform == "darwin" else "so"),)
PASQAL_QDMI_LIBRARY_PATH = next(
    (_LIBRARY_DIR / name for name in _NAMES if (_LIBRARY_DIR / name).is_file()),
    _LIBRARY_DIR / _NAMES[0],
)

__all__ = [
    "PASQAL_QDMI_CATALOG_PATH",
    "PASQAL_QDMI_CMAKE_DIR",
    "PASQAL_QDMI_DEVICE_ID",
    "PASQAL_QDMI_INCLUDE_DIR",
    "PASQAL_QDMI_LIBRARY_PATH",
    "PASQAL_QDMI_PREFIX",
    "__version__",
]
