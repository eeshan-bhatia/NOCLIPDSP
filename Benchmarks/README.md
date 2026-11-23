# DSP benchmarks

Measured 2026-11-23 with Zig 0.17.1 / Clang 22.1.8, CMake Release,
x86_64-windows-gnu. 20,000 blocks per profile, 256 stereo frames, 48 kHz,
12 dB drive, unit threshold/ceiling, fully wet. Timings include refilling
input arrays each block so nonlinear output is not repeatedly reprocessed.

| Profile | Microseconds/block |
| --- | ---: |
| Clip disabled | 3.2071 |
| Hard clip | 8.40011 |
| Tanh | 19.7754 |
| Arctangent | 20.9247 |
| Cubic | 10.7826 |
| Adjustable knee | 10.711 |
| Asymmetric | 10.9038 |

Single wall-clock observations, not real-time deadline guarantees or DAW CPU
readings. Excludes host/GUI overhead and future oversampling/multiband/limiting.
Hardware identity was not collected, so cross-machine comparisons are unjustified.
Printed RMS checksums keep outputs observable.

Reproduce after the DSP-only build:
```powershell
./scripts/Test-DSP.ps1
./build-dsp/DSPBenchmarks.exe
```

After a native MSVC multi-configuration build:
```powershell
./build-native/Release/DSPBenchmarks.exe
```

The previous 2026-11-22 gain-only result was 1.06197 us/block under a different
benchmark workload and engine. It is not a valid before/after speed comparison.
No speculative SIMD optimization has been introduced.
