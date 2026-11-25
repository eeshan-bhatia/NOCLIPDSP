# Building

## Windows prerequisites

Production targets require Visual Studio 2022+ C++ desktop tools, an actual
Windows 10/11 SDK, CMake >=3.22 (new enough for your VS version), Git and C++20.

## Verified environment (2026-11-23)

The SDK issue was resolved after the user's Visual Studio update.
CMake detected Windows SDK 10.0.28000.0 and MSVC 19.52.37029.0. Native Release
configuration, VST3, standalone and the three tests available at that time passed.
The earlier missing 10.0.22621.0 SDK is no longer an active blocker.

## Native plugin build

```powershell
& .tools/python/cmake/data/bin/cmake.exe -S . -B build-native -G "Visual Studio 18 2026" -A x64 "-DNOCLIP_JUCE_PATH=D:/Coding/NOCLIPDSP/.tools/JUCE"
& .tools/python/cmake/data/bin/cmake.exe --build build-native --config Release --parallel 2
& .tools/python/cmake/data/bin/ctest.exe --test-dir build-native -C Release --output-on-failure
```

Use VS 17 2022 for that installed toolchain, with its C++ components and SDK.
Do not reuse a build directory for a different generator/compiler. CMake can
fetch pinned JUCE if NOCLIP_JUCE_PATH is omitted. Build outputs and dependencies
are ignored. Installation paths are documented in the root README.

## SDK-independent DSP-only build

Portable tools are local, not global. They are optional validation dependencies,
not dependencies of the distributed plugin:
- CMake 4.4.5 under .tools/python/cmake.
- Zig 0.17.1 under .tools/python/ziglang.
- Ninja 1.13.2 under .tools/ninja-python/bin.
- Official JUCE checkout under .tools/JUCE.

To recreate tools using an existing Python installation:
```powershell
python -m pip install --target .tools/python cmake==4.4.5 ziglang==0.17.1
python -m pip install --target .tools/ninja-python ninja==1.13.2
```

Then:
```powershell
./scripts/Test-DSP.ps1
./build-dsp/DSPBenchmarks.exe
```

The script configures the real project with NOCLIP_BUILD_PLUGIN=OFF and the
cmake/zig-tests.cmake toolchain, builds it and runs CTest. It fails on any failed
command. Zig cache is kept under .tools. Run from the repository root; a custom
build folder can be supplied with -BuildDirectory.

JUCE explicitly rejects MinGW; this optional toolchain must not be used to
claim a validated Windows plugin. Portable Linux object compilation of the
JUCE-facing sources is a separate source/API check only.

## VST3 validation

Official pluginval 1.0.4 Windows release:
https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip

Extract into .tools/pluginval and run ./scripts/Validate-Plugin.ps1. The wrapper
waits for the validator process, checks the exit code, and writes its log
under build-native. It validates the bundle by path without a system installation.

## Product filenames
The CMake project and plugin target are named noclip.DSP. Native Windows outputs:
- build-native/noclip.DSP.slnx
- build-native/noclip.DSP_artefacts/Release/Standalone/noclip.DSP.exe
- build-native/noclip.DSP_artefacts/Release/VST3/noclip.DSP.vst3

The workspace directory can retain its existing name. The internal NOCLIPDSP_STATE
identifier remains unchanged for compatibility with saved host projects.

## Release packages

After building and validating Release, commit the source and run:

```powershell
python scripts/Package-Release.py --build build-native --output build-release
```

The script requires clean project and pinned JUCE checkouts. It produces a
Windows x64 VST3 ZIP, a separate corresponding-source ZIP including the pinned
JUCE checkout and release inputs, and SHA-256 checksums. Confirm redistribution
permission for all external inputs before publishing. Upload all three assets
to the release matching the packaged version and source commit.

Only the VST3 ZIP is needed to install the plugin. Keep the source archive
available alongside it. Build outputs and release archives remain ignored.

## macOS universal build

Install Xcode and CMake. To build for Apple Silicon and Intel:

```sh
cmake -S . -B build-macos -G Xcode '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-macos --config Release --parallel 3
ctest --test-dir build-macos -C Release --output-on-failure
```

The `macOS prerelease build` GitHub Actions workflow fetches the pinned build
inputs, builds both architectures, runs the four suites natively and under
Rosetta, and validates VST3 loading with pluginval level 5. It produces plugin
and corresponding-source ZIPs plus checksums as a workflow artifact. Publication
is a separate step after reviewing the successful run.

`scripts/Package-MacOS.py` packages the workflow's `build-macos` output and verifies
the extracted bundle's ad-hoc signature. Developer ID signing and notarization
require Apple developer credentials and are not performed by this workflow.
