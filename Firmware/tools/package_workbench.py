#!/usr/bin/env python3
"""Build a Windows onedir bundle. Qt DLLs remain replaceable; outputs stay ignored."""
import argparse
from importlib import metadata
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bridge", type=Path, required=True, help="Release native C fixture, preferably /MT")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Windows executable packaging must run on Windows")
    tools = Path(__file__).resolve().parent
    bridge = args.bridge.resolve()
    if not bridge.is_file():
        parser.error("build the native C fixture first")
    out = args.out.resolve()
    if out.exists():
        parser.error("use a new output directory; packaging never deletes existing evidence")
    out.mkdir(parents=True)
    subprocess.run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--onedir", "--windowed",
        "--name", "WTK-Calibration", "--distpath", str(out / "dist"), "--workpath", str(out / "work"),
        "--specpath", str(out), "--paths", str(tools), "--add-binary", str(bridge) + ";simulation",
        "--exclude-module", "PySide6.QtQml", "--exclude-module", "PySide6.QtQuick",
        "--exclude-module", "PySide6.QtNetwork", "--exclude-module", "PySide6.QtOpenGL",
        str(tools / "pc_osl_workbench.py")], check=True)
    bundle = out / "dist/WTK-Calibration"
    notices = bundle / "third-party-notices"
    notices.mkdir()
    for name in ("PySide6-Essentials", "shiboken6", "pyserial", "PyInstaller"):
        distribution = metadata.distribution(name)
        target = notices / name
        target.mkdir()
        (target / "METADATA.txt").write_text(str(distribution.metadata), encoding="utf-8")
        for entry in distribution.files or []:
            if "license" in str(entry).lower() or "copying" in str(entry).lower():
                source = Path(distribution.locate_file(entry))
                if source.is_file():
                    shutil.copy2(source, target / source.name)
    for filename in ("WORKBENCH_LICENSES.md", "PC_OSL_WORKBENCH.md", "PC_SCOPE_WAVEFORM.md"):
        shutil.copy2(tools / filename, bundle / filename)
    # Keep application/backend source available for examination and modified-library use.
    sources = bundle / "application-source"
    sources.mkdir()
    for source in tools.glob("*.py"):
        shutil.copy2(source, sources / source.name)
    for source in tools.glob("requirements-workbench*.txt"):
        shutil.copy2(source, sources / source.name)
    print(bundle / "WTK-Calibration.exe")


if __name__ == "__main__":
    main()
