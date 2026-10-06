# Copper

Copper is an electromagnetic simulation engine built around a finite-difference time-domain (FDTD) solver. It accepts geometry expressed as a CSXCAD `ContinuousStructure`, builds its own Yee grid and material coefficients, and runs the time-stepping loop on either Metal or a CPU reference backend.

The project is primarily used by [KiEMS](https://github.com/beelsebob/KiEMS), but it can also be built and exercised independently as an Xcode framework project.

## What it provides

- Metal-backed and CPU FDTD execution with equivalent simulation behavior.
- Yee-grid, material-coefficient, excitation, CPML, probe, and lumped-RLC handling.
- Voltage and current probe samples returned in memory, with an openEMS-compatible text representation available through `CopperProbeResult::data()`.
- Optional field-frame capture, either retained in memory or streamed to an HDF5 file.
- A streaming HDF5 field-frame format with Blosc2 compression, low-resolution previews, and on-demand full-resolution refinement.
- `CopperUtils`, including polygon geometry helpers and a small C++ logging utility.

## Requirements

Copper is currently built through Xcode. Install Xcode and the Xcode command-line tools, then install Homebrew.

The supported Homebrew dependencies are checked by the setup script:

- `geos` 3.15
- `nlohmann-json` 3.x
- `hdf5` 2.2
- `c-blosc2` 3.x
- `cgal` 6.2
- `boost` 1.90
- `gmp` 6.3
- `mpfr` 4.2
- `vtk` 9.x

Copper also uses the `CSXCAD`, `fparser`, and `tinyxml` Git submodules.

## Set up

From a standalone Copper checkout:

```sh
git submodule update --init --recursive
Scripts/check_dependencies.py
```

The dependency check verifies the Homebrew formulas, writes the local dependency stamp required by the Xcode build, and configures the Homebrew prefix and installed VTK version. To accept its installation prompts automatically, use:

```sh
Scripts/check_dependencies.py --yes
```

When Copper is checked out as a KiEMS submodule, run KiEMS’s top-level dependency check instead; it invokes Copper’s check as needed.

## Build and test

Open `Copper.xcodeproj` in Xcode, select the **Copper** scheme to build the framework or **CopperTests** to run the test suite.

From the command line:

```sh
xcodebuild \
  -project Copper.xcodeproj \
  -scheme CopperTests \
  test
```

The tests cover the numerical engine, GPU/CPU behavior, CPML, probes, lumped RLC elements, grid extraction, coefficient generation, and field-frame HDF5 round trips.

## Using the solver

The integration entry points live in `Copper/CopperFDTDRunner.h`:

- `runFDTDPortOnGPU(...)` runs one excited port through the Metal backend.
- `runFDTDPortOnCPU(...)` runs the same operation through the CPU backend.
- `cpmlAlphaMaxForFrequency(...)` derives the CPML alpha parameter for a simulation’s lowest frequency.

Both run functions take a `ContinuousStructure` and a `CopperFDTDPortConfig`. The config supplies the Gaussian excitation parameters, maximum timestep count, boundary types, and—when applicable—the board cutout used to limit the active domain. A run returns `CopperFDTDRunResult`, containing completion state, errors or cancellation state, probe samples, and field-capture metadata.

The runner header intentionally stays separate from the framework umbrella header. Include `CopperFDTDRunner.h` directly from code that already has the installed CSXCAD/openEMS headers available.

Use `CopperFDTDProgressCallback` for setup and timestep progress, and provide an `isCancelled` callback when the host needs prompt cancellation.

## Field-frame files

Pass a `FieldFrameSeriesRequest` to either runner to stream captured fields to disk rather than retain all frames in memory. `FieldFrameSeriesWriter` and `FieldFrameSeriesReader` provide direct writer/reader access for hosts that manage capture themselves.

The file format is versioned and documented in [docs/field_frame_series_format.md](docs/field_frame_series_format.md). It stores compressed preview frames for ordinary playback plus independently addressable full-resolution residual tiles for progressive refinement.

## Layout

| Path | Purpose |
| --- | --- |
| `Copper/` | FDTD runner, Metal shaders, field-frame reader/writer, and internal solver implementation. |
| `CopperUtils/` | Shared logging, string, geometry, and polygon utilities. |
| `CopperTests/` | XCTest coverage for the engine and storage format. |
| `docs/` | File-format and implementation notes. |
| `Scripts/check_dependencies.py` | Verifies and configures the supported local build dependencies. |
| `submodules/` | CSXCAD, fparser, and tinyxml dependencies. |

## Further reading

- [Field frame-series format](docs/field_frame_series_format.md)
- [Field storage experiments](docs/copper_field_storage_experiments.md)
