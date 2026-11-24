"""Package the validated universal macOS VST3 and corresponding source."""
import hashlib
import importlib.util
import shutil
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("packaging", ROOT / "scripts/Package-Release.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


def main():
    if packaging.git(ROOT, "status", "--porcelain", "--untracked-files=no").strip():
        raise SystemExit("Commit tracked changes before packaging.")
    revision = packaging.git(ROOT, "rev-parse", "HEAD").decode().strip()
    juce = ROOT / ".tools/JUCE"
    if packaging.git(juce, "rev-parse", "HEAD").decode().strip() != packaging.JUCE_REVISION:
        raise SystemExit("Unexpected JUCE revision")
    bundle = ROOT / "build-macos/noclip.DSP_artefacts/Release/VST3/noclip.DSP.vst3"
    subprocess.run(["lipo", "-verify_arch", "arm64", "x86_64", str(bundle / "Contents/MacOS/noclip.DSP")], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(bundle)], check=True)
    output = ROOT / "build-macos-release"
    output.mkdir(exist_ok=True)
    stage = ROOT / "build-macos/package"
    stage.mkdir(exist_ok=True)
    shutil.copytree(bundle, stage / bundle.name, dirs_exist_ok=True, symlinks=True)
    for source, name in [(ROOT / "LICENSE", "LICENSE"), (juce / "LICENSE.md", "JUCE-LICENSE.md"),
                         (juce / "JUCE.spdx.json", "JUCE.spdx.json")]:
        shutil.copy2(source, stage / name)
    notices = """noclip.DSP: GNU AGPL version 3; see LICENSE.
JUCE 9.0.5: distributed under AGPL version 3; see JUCE-LICENSE.md and JUCE.spdx.json.
Counter-Strike Regular by SoJa: separate third-party asset supplied by the project
owner, who confirmed redistribution permission for the release.
"""
    (stage / "THIRD-PARTY-NOTICES.txt").write_text(notices)
    (stage / "INSTALL.txt").write_text(f"""noclip.DSP 0.6.0 - macOS universal VST3 prerelease
Apple Silicon and Intel. Deployment target: macOS 11 or later.

1. Close your DAW and extract the ZIP.
2. In Finder, choose Go > Go to Folder and enter:
   ~/Library/Audio/Plug-Ins/VST3/
   Create this folder if it does not exist.
3. Copy the whole noclip.DSP.vst3 bundle there, replacing any older copy.
4. Reopen your VST3-compatible DAW and rescan plugins.

You do not need the repository or source archive. This is VST3, not Audio Unit;
Logic Pro does not load VST3 plugins. No standalone application is in this ZIP.
To uninstall, close your DAW and remove the installed bundle.

This development build is ad-hoc signed, NOT Developer ID signed or notarized.
macOS may block it after download. Follow macOS Privacy & Security approval
prompts if available. DAW scanning behavior varies; a smooth Gatekeeper install
is not guaranteed. Do not disable system-wide security protections.

Tests run on Apple Silicon and Intel via Rosetta; real Mac DAW listening and
project recall, and execution on macOS 11, are not yet verified.
Source revision: {revision}
Corresponding source: noclip.DSP-0.6.0-macOS-Source.zip on
https://github.com/eeshan-bhatia/NOCLIPDSP/releases/tag/v0.6.0-macos
License and third-party notices are included.
""")
    plugin_zip = output / "noclip.DSP-0.6.0-macOS-Universal-VST3.zip"
    subprocess.run(["ditto", "-c", "-k", "--sequesterRsrc", str(stage), str(plugin_zip)], check=True)
    source_zip = output / "noclip.DSP-0.6.0-macOS-Source.zip"
    prefix = "noclip.DSP-0.6.0-macOS-Source/"
    with zipfile.ZipFile(source_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        packaging.add_repository(archive, ROOT, prefix)
        packaging.add_repository(archive, juce, prefix + "third_party/JUCE/")
        archive.write(ROOT / ".tools/cs_regular.ttf", prefix + "release-inputs/cs_regular.ttf")
        archive.writestr(prefix + "RELEASE-THIRD-PARTY-NOTICES.txt", notices.replace("JUCE-LICENSE.md and JUCE.spdx.json", "third_party/JUCE/LICENSE.md and third_party/JUCE/JUCE.spdx.json"))
        archive.writestr(prefix + "BUILD-RELEASE.txt", f"""Source commit: {revision}
JUCE commit: {packaging.JUCE_REVISION}
Install Xcode and CMake, then run from this directory:
cmake -S . -B build -G Xcode '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DNOCLIP_JUCE_PATH=third_party/JUCE -DNOCLIP_FONT_FILE=release-inputs/cs_regular.ttf
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
See .github/workflows/macos-release.yml for validation and packaging steps.
""")
    sums = []
    for path in (plugin_zip, source_zip):
        with zipfile.ZipFile(path) as archive:
            if archive.testzip():
                raise SystemExit("Archive integrity failure")
        sums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n")
        print(f"{path.name}: {path.stat().st_size / 1024 / 1024:.2f} MiB")
    (output / "SHA256SUMS-macOS.txt").write_text("".join(sums))
    # Validate the actual delivered bundle after extracting it again.
    extracted = ROOT / "build-macos/package-check"
    subprocess.run(["ditto", "-x", "-k", str(plugin_zip), str(extracted)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(extracted / bundle.name)], check=True)


if __name__ == "__main__":
    main()
