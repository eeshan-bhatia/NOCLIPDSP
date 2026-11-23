param([string]$BuildDirectory = "build-dsp")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$cmake = Join-Path $root ".tools/python/cmake/data/bin/cmake.exe"
$ctest = Join-Path $root ".tools/python/cmake/data/bin/ctest.exe"
$ninja = Join-Path $root ".tools/ninja-python/bin/ninja.exe"
$zig = Join-Path $root ".tools/python/ziglang/zig.exe"
foreach ($tool in @($cmake, $ctest, $ninja, $zig)) {
    if (!(Test-Path -LiteralPath $tool)) { throw "Missing local tool: $tool. See docs/BUILDING.md." }
}
$previousCache = $env:ZIG_GLOBAL_CACHE_DIR
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root ".tools/zig-cache"
    & $cmake -S $root -B $BuildDirectory -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$root/cmake/zig-tests.cmake" "-DNOCLIP_BUILD_PLUGIN=OFF" "-DCMAKE_BUILD_TYPE=Release"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    & $cmake --build $BuildDirectory --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
    & $ctest --test-dir $BuildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "CTest failed." }
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $previousCache
}
