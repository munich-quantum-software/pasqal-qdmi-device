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

"""Sphinx documentation with executable Pasqal Cloud examples."""

from __future__ import annotations

import os
from importlib import metadata

project = "Pasqal QDMI Device"
author = "Lukas Burgholzer and Aleksander Wennersteen"
copyright = "2026 Munich Quantum Software Company GmbH and Pasqal SAS"

try:
    version = metadata.version("pasqal-qdmi")
except ModuleNotFoundError:
    msg = "pasqal-qdmi must be installed to build the documentation"
    raise ModuleNotFoundError(msg) from None

release = version.split("+")[0]
extensions = ["myst_nb", "sphinx_copybutton"]
source_suffix = [".md"]
exclude_patterns = ["_build", "**.ipynb_checkpoints", "**.jupyter_cache", "**.jupyter_execute"]
html_theme = "furo"
html_title = project
html_static_path = ["_static"]
html_favicon = "_static/logo-mqsc-light.svg"
html_theme_options = {
    "light_logo": "logo-mqsc-light.svg",
    "dark_logo": "logo-mqsc-dark.svg",
    "sidebar_hide_name": False,
}
html_show_sourcelink = False
html_extra_path = ["_build/doxygen"]
nitpicky = True
myst_enable_extensions = ["colon_fence"]

myst_heading_anchors = 3

nb_execution_mode = "cache"
nb_execution_excludepatterns = [] if os.getenv("PASQAL_QDMI_LIVE_DOCS") == "1" else ["usage.md"]
nb_execution_raise_on_error = True
nb_execution_show_tb = True
nb_execution_timeout = 360
