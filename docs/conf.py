# Copyright (c) 2026 Munich Quantum Software Company GmbH
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Offline documentation; no package import, authentication or live execution."""

import tomllib
from pathlib import Path

project = "Pasqal QDMI Device"
author = "Munich Quantum Software Company GmbH"
copyright = "2026 Munich Quantum Software Company GmbH"
release = tomllib.loads((Path(__file__).parents[1] / "pyproject.toml").read_text())["project"][
    "version"
]
extensions = ["myst_parser", "sphinx_copybutton"]
source_suffix = {".md": "markdown"}
exclude_patterns = ["_build", "assessment.md", "evidence/**"]
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
