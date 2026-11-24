"""Package a validated Windows build and its corresponding source (Python 3.10+)."""
import argparse
import hashlib
import io
import re
import subprocess
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
JUCE_REVISION = "d60b7a78030661574a615b586d9325ba7a7fa7dd"


def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args])


def add_repository(destination, repository, prefix):
    with zipfile.ZipFile(io.BytesIO(git(repository, "archive", "--format=zip", "HEAD"))) as source:
        for item in source.infolist():
            if not item.is_dir():
                destination.writestr(prefix + item.filename, source.read(item))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build-native")
    parser.add_argument("--output", type=Path, default=ROOT / "build-release")
    args = parser.parse_args()
    if git(ROOT, "status", "--porcelain").strip():
        raise SystemExit("Commit project changes before packaging a release.")
    cache = (args.build / "CMakeCache.txt").read_text(encoding="utf-8")

    def setting(name):
        match = re.search(rf"^{name}:[^=]+=(.*)$", cache, re.MULTILINE)
        if not match:
            raise SystemExit(f"Missing build setting: {name}")
        return match.group(1).strip()

    version = re.search(r"project\(noclip\.DSP VERSION ([\d.]+)",
                        (ROOT / "CMakeLists.txt").read_text()).group(1)
    revision = git(ROOT, "rev-parse", "HEAD").decode().strip()
    juce = Path(setting("NOCLIP_JUCE_PATH"))
    if git(juce, "rev-parse", "HEAD").decode().strip() != JUCE_REVISION:
        raise SystemExit("JUCE checkout does not match the pinned revision.")
    if git(juce, "status", "--porcelain").strip():
        raise SystemExit("JUCE checkout has local changes.")
    font_setting = setting("NOCLIP_FONT_FILE")
    font = Path(font_setting) if font_setting else None
    bundle = args.build / "noclip.DSP_artefacts/Release/VST3/noclip.DSP.vst3"
    binary = bundle / "Contents/x86_64-win/noclip.DSP.vst3"
    if not binary.is_file():
        raise SystemExit("Build the Release x64 VST3 target first.")
    manifest = (bundle / "Contents/Resources/moduleinfo.json").read_text()
    if f'"Version": "{version}"' not in manifest:
        raise SystemExit("Built plugin version does not match the project.")

    args.output.mkdir(parents=True, exist_ok=True)
    stem = f"noclip.DSP-{version}"
    plugin_zip = args.output / f"{stem}-Windows-x64-VST3.zip"
    source_zip = args.output / f"{stem}-Source.zip"
    release_url = f"https://github.com/eeshan-bhatia/NOCLIPDSP/releases/tag/v{version}"
    install = f"""noclip.DSP {version} - Windows x64 VST3

You do not need the repository, source archive, Visual Studio or JUCE to use it.
Requires Windows 10/11 x64 and a 64-bit VST3 host. This is a development release.

INSTALL / UPDATE
1. Close your DAW.
2. Extract this ZIP. Copy the ENTIRE noclip.DSP.vst3 folder into:
   C:\\Program Files\\Common Files\\VST3\\
   Windows may ask for administrator permission to copy there.
3. Replace the previous noclip.DSP.vst3 bundle when updating. Avoid duplicate copies.
4. Reopen your DAW and rescan plugins. Load noclip.DSP as a mixer effect.

Do not copy only the file inside Contents/x86_64-win, and do not rename the bundle.
FL Studio: open Manage plugins, scan for plugins, then load the effect in a mixer slot.
To uninstall, close the DAW and remove the installed noclip.DSP.vst3 folder.

If scanning reports a missing MSVCP140/VCRUNTIME140 DLL, install or update the
Microsoft Visual C++ v14 x64 Redistributable, then restart the DAW:
https://aka.ms/vc14/vc_redist.x64.exe
Runtime details: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist

Source revision: {revision}
Corresponding source, JUCE and build inputs: {source_zip.name} on {release_url}
The source download is optional for plugin users. License: GNU AGPL v3; see LICENSE.
"""
    notices = """noclip.DSP: GNU Affero General Public License version 3 (LICENSE).
JUCE 9.0.5: distributed under AGPL v3. See JUCE-LICENSE.md and JUCE.spdx.json
for JUCE and bundled third-party copyright/license notices.
"""
    if font:
        notices += ("Counter-Strike Regular by SoJa: supplied by the project owner, who confirmed\n"
                    "redistribution permission for this release. It is a separate third-party asset.\n")
    with zipfile.ZipFile(plugin_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(bundle.rglob("*")):
            if path.is_file():
                archive.write(path, "noclip.DSP.vst3/" + path.relative_to(bundle).as_posix())
        archive.writestr("INSTALL.txt", install)
        archive.write(ROOT / "LICENSE", "LICENSE")
        archive.writestr("THIRD-PARTY-NOTICES.txt", notices)
        archive.write(juce / "LICENSE.md", "JUCE-LICENSE.md")
        archive.write(juce / "JUCE.spdx.json", "JUCE.spdx.json")

    prefix = stem + "-Source/"
    font_option = ' "-DNOCLIP_FONT_FILE=release-inputs/cs_regular.ttf"' if font else ""
    source_info = f"""Corresponding source for noclip.DSP {version}
Project commit: {revision}
JUCE commit: {JUCE_REVISION}

This archive includes the project, pinned JUCE source and release build inputs.
From this directory, using Visual Studio 2026 C++ desktop tools, Windows SDK and CMake:
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DNOCLIP_JUCE_PATH=third_party/JUCE{font_option}
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure

The release was built with MSVC 19.52.37029 and Windows SDK 10.0.28000.0.
See README.md, LICENSE, and RELEASE-THIRD-PARTY-NOTICES.txt.
"""
    with zipfile.ZipFile(source_zip, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        add_repository(archive, ROOT, prefix)
        add_repository(archive, juce, prefix + "third_party/JUCE/")
        if font:
            archive.write(font, prefix + "release-inputs/cs_regular.ttf")
        archive.writestr(prefix + "BUILD-RELEASE.txt", source_info)
        archive.writestr(prefix + "RELEASE-THIRD-PARTY-NOTICES.txt", notices)
        archive.write(juce / "LICENSE.md", prefix + "JUCE-LICENSE.md")
        archive.write(juce / "JUCE.spdx.json", prefix + "JUCE.spdx.json")

    checksums = []
    for path in (plugin_zip, source_zip):
        with zipfile.ZipFile(path) as archive:
            if archive.testzip() is not None:
                raise SystemExit(f"Archive integrity failure: {path}")
        checksums.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n")
        print(f"{path.name}: {path.stat().st_size / 1024 / 1024:.2f} MiB")
    (args.output / "SHA256SUMS.txt").write_text("".join(checksums), encoding="utf-8")


if __name__ == "__main__":
    main()
