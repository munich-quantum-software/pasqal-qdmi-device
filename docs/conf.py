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

"""Sphinx documentation with offline and credential-enabled MyST notebooks."""

import os
import tomllib
from pathlib import Path

project = "Pasqal QDMI Device"
author = "Lukas Burgholzer and Aleksander Wennersteen"
copyright = "2026 Munich Quantum Software Company GmbH and Pasqal SAS"
release = tomllib.loads((Path(__file__).parents[1] / "pyproject.toml").read_text())["project"]["version"]
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
myst_enable_extensions = ["colon_fence"]

myst_heading_anchors = 3

nb_execution_mode = "cache"
nb_execution_raise_on_error = True
nb_execution_timeout = 360
nb_execution_excludepatterns = (
    []
    if all(os.environ.get(name) for name in ("PASQAL_EMAIL", "PASQAL_PASSWORD", "PASQAL_PROJECT_ID"))
    else ["cloud.md"]
)
