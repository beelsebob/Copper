#!/usr/bin/env python3
"""Checks that the third-party libraries Copper builds against are installed through Homebrew, at
the versions the Xcode project expects, and that its git submodules are checked out. Offers to
install anything missing, after asking. Missing formulas are errors; version mismatches are warnings.

Usage: Scripts/check_dependencies.py [--yes]
  --yes   answer "yes" to every prompt (for unattended setup)

On success writes the untracked Config/DependenciesChecked.generated.h; until it exists every
Xcode compile stops with an #error from Config/DependencyCheck.h. The script never runs from Xcode.
When Copper is a submodule, the parent repository's check_dependencies.py runs this one for it.
See dependency_tool.py for how repositories cooperate.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dependency_tool import Context, Formula, Repository, main  # noqa: E402

# A prefix of "9.6" accepts 9.6, 9.6.2, 9.6.2_1, ... but not 9.7. Where the project hard-codes a
# version (vtk's -9.6 library suffixes, hdf5 2.x's API) a mismatch will break the build; elsewhere
# it is the version the project was last verified with.
HOMEBREW = [
    Formula("geos",          "3.15", "CopperUtils (polygon geometry)"),
    Formula("nlohmann-json", "3",    "Copper"),
    Formula("hdf5",          "2.2",  "Copper, CSXCAD"),
    Formula("c-blosc2",      "3",    "Copper (field-frame compression)"),
    Formula("cgal",          "6.2",  "CSXCAD"),
    Formula("boost",         "1.90", "CSXCAD"),
    Formula("gmp",           "6.3",  "CSXCAD"),
    Formula("mpfr",          "4.2",  "CSXCAD"),
    Formula("vtk",           "9",    "CSXCAD (VTK_VERSION build setting follows the installed version)"),
]

SUBMODULES = ["submodules/CSXCAD", "submodules/fparser", "submodules/tinyxml"]


def configure(ctx: Context) -> None:
    # Config/BuildPaths.xcconfig holds machine-specific settings; make its untracked override match
    # this machine.
    ctx.set_build_setting("Config/BuildPaths.xcconfig", "HOMEBREW_PREFIX", str(ctx.brew_prefix))
    # Homebrew's VTK names its header directory and libraries after its major.minor version.
    vtk = ctx.linked_version("vtk")
    if vtk:
        ctx.set_build_setting("Config/BuildPaths.xcconfig", "VTK_VERSION", ".".join(vtk.split(".")[:2]))


if __name__ == "__main__":
    sys.exit(main(__file__, Repository(
        name="Copper",
        homebrew=HOMEBREW,
        submodules=SUBMODULES,
        stamp="Config/DependenciesChecked.generated.h",
        configure=configure,
    )))
