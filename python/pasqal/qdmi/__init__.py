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

"""Python wrapper for exposing the Pasqal QDMI device library."""

# ruff: file-ignore[non-empty-init-module]

import sys
from importlib.metadata import distribution
from pathlib import Path

from ._version import version as __version__

__all__ = [
    "PASQAL_QDMI_CATALOG_PATH",
    "PASQAL_QDMI_CMAKE_DIR",
    "PASQAL_QDMI_DEVICE_ID",
    "PASQAL_QDMI_INCLUDE_DIR",
    "PASQAL_QDMI_LIBRARY_PATH",
    "PASQAL_QDMI_PREFIX",
    "__version__",
]

PASQAL_QDMI_DEVICE_ID = "pasqal.default"
PASQAL_QDMI_PREFIX = "PASQAL"


def __dir__() -> list[str]:
    return __all__


dist = distribution("pasqal-qdmi")
located_include_dir = dist.locate_file("pasqal/qdmi/data/include/pasqal_qdmi")
resolved_include_dir = Path(str(located_include_dir)).resolve(strict=True)

_PASQAL_QDMI_DATA = resolved_include_dir.parents[1]


def _resolve_library_dir() -> Path:
    """Return the directory containing the packaged Pasqal QDMI shared library.

    Raises:
        FileNotFoundError: If the expected library directory does not exist.
    """
    if sys.platform == "win32":
        library_dir = _PASQAL_QDMI_DATA / "bin"
        if library_dir.exists():
            return library_dir
        msg = f"Expected 'bin' directory for Pasqal QDMI library on Windows, but it does not exist: {library_dir}"
        raise FileNotFoundError(msg)

    library_dir = _PASQAL_QDMI_DATA / "lib"
    if library_dir.exists():
        return library_dir

    library_dir = _PASQAL_QDMI_DATA / "lib64"
    if library_dir.exists():
        return library_dir

    msg = (
        "Expected 'lib' or 'lib64' directory for Pasqal QDMI library on Unix-like systems, "
        f"but neither exists: {_PASQAL_QDMI_DATA}"
    )
    raise FileNotFoundError(msg)


_PASQAL_QDMI_LIBRARY_DIR = _resolve_library_dir()
_PASQAL_QDMI_CATALOG_NAME = "pasqal-qdmi-device.qdmi.json"

# Ignore the adjacent QDMI catalogue when locating the native library.
library_files = [
    path for path in _PASQAL_QDMI_LIBRARY_DIR.glob("*pasqal-qdmi-device*") if path.name != _PASQAL_QDMI_CATALOG_NAME
]
if not library_files:
    msg = f"No Pasqal QDMI library found in: {_PASQAL_QDMI_LIBRARY_DIR}"
    raise FileNotFoundError(msg)
PASQAL_QDMI_LIBRARY_PATH = min(library_files, key=lambda p: len(p.name))

PASQAL_QDMI_CATALOG_PATH = _PASQAL_QDMI_LIBRARY_DIR / _PASQAL_QDMI_CATALOG_NAME
if not PASQAL_QDMI_CATALOG_PATH.exists():
    msg = f"PASQAL_QDMI_CATALOG_PATH does not exist: {PASQAL_QDMI_CATALOG_PATH}"
    raise FileNotFoundError(msg)

PASQAL_QDMI_INCLUDE_DIR = _PASQAL_QDMI_DATA / "include"
if not PASQAL_QDMI_INCLUDE_DIR.exists():
    msg = f"PASQAL_QDMI_INCLUDE_DIR does not exist: {PASQAL_QDMI_INCLUDE_DIR}"
    raise FileNotFoundError(msg)

PASQAL_QDMI_CMAKE_DIR = _PASQAL_QDMI_DATA / "share" / "cmake"
if not PASQAL_QDMI_CMAKE_DIR.exists():
    msg = f"PASQAL_QDMI_CMAKE_DIR does not exist: {PASQAL_QDMI_CMAKE_DIR}"
    raise FileNotFoundError(msg)

del dist, located_include_dir, resolved_include_dir
