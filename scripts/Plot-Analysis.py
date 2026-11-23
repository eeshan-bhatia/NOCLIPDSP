"""Reproduce figures from native CTest measurement CSVs; no audio downloads.
Run: py -3.12 scripts/Plot-Analysis.py --build build-native
Dependencies: numpy, matplotlib (project-local .tools/analysis is supported).
"""
import argparse
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / ".tools" / "analysis"))
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument("--build", default="build-native")
args = parser.parse_args()
source = root / args.build
output = root / "docs" / "measurements"
output.mkdir(parents=True, exist_ok=True)
data = np.genfromtxt(source / "oversampling-spectrum.csv", delimiter=",", names=True)
plt.style.use("dark_background")
fig, ax = plt.subplots(figsize=(11, 5), layout="constrained")
for column, label, color in zip(data.dtype.names[1:], ["1x", "2x", "4x", "8x"], ["#de9b35", "#ccba7c", "#5d79ae", "#a2ff57"]):
    ax.plot(data["frequency"] / 1000, data[column], label=label, color=color, linewidth=0.9)
ax.set(xlim=(0, 24), ylim=(-140, 0), xlabel="Frequency (kHz)", ylabel="Amplitude (dBFS)",
       title="noclip.DSP: 9 kHz sine through hard clipping, 48 kHz host rate")
ax.axvline(21, color="white", linestyle=":", alpha=.4)
ax.text(20.8, -5, "Third-harmonic alias", rotation=90, ha="right", va="top", fontsize=9)
ax.grid(alpha=.15); ax.legend(loc="lower left")
fig.savefig(output / "oversampling-spectrum.png", dpi=160)
plt.close(fig)

cross = np.genfromtxt(source / "crossover-response.csv", delimiter=",", names=True)
fig, axes = plt.subplots(2, 1, figsize=(10, 6), layout="constrained", sharex=True)
for rate in np.unique(cross["sample_rate"]):
    rows = cross[cross["sample_rate"] == rate]
    axes[0].semilogx(rows["frequency"], rows["magnitude_db"], "o-", label=f"{rate/1000:g} kHz")
    axes[1].semilogx(rows["frequency"], np.unwrap(rows["phase_radians"]), "o-")
axes[0].set(title="Three-band Linkwitz–Riley reconstruction (120 Hz / 2.5 kHz)", ylabel="Magnitude (dB)", ylim=(-.01, .01))
axes[0].legend()
axes[1].set(ylabel="Unwrapped phase (radians)", xlabel="Frequency (Hz)")
for ax in axes: ax.grid(alpha=.2)
fig.savefig(output / "crossover-response.png", dpi=160)
plt.close(fig)
harmonics = np.genfromtxt(source / "808-harmonics.csv", delimiter=",", names=True)
fig, ax = plt.subplots(figsize=(10, 5), layout="constrained")
for field, label in (("clean", "Clean 808"), ("preserved", "Drive + sub preserve"), ("dirty", "Drive, no sub preserve")):
    ax.plot(harmonics["frequency"], 20*np.log10(np.maximum(1e-9,harmonics[field])), "o-", label=label)
ax.set(xlabel="Frequency (Hz)", ylabel="Harmonic amplitude (dBFS)", ylim=(-100, 5),
       title="50 Hz sine through 808 engine: +24 dB drive, 120 Hz split")
ax.grid(alpha=.2); ax.legend()
fig.savefig(output / "808-harmonics.png", dpi=160)
plt.close(fig)
for name in ("oversampling-spectrum.csv", "crossover-response.csv", "oversampling-cpu.csv", "808-harmonics.csv"):
    (output / name).write_bytes((source / name).read_bytes())
print(f"Saved figures and source measurements to {output}")
