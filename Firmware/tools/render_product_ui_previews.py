#!/usr/bin/env python3
"""Render the real host PRODUCT UI for both languages and check result refresh."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess

from resource_pack_format import build_pack


SCENARIOS = ("ready", "menu", "result", "details", "wizard", "upload", "fault")
SECONDARY_SCENARIOS = ("display", "sound", "language", "diagnostics",
                       "maintenance", "calibration", "wizard-intro", "wizard-capture")
LANGUAGES = ("en", "pt-BR")


def render(executable: Path, pack: Path | str, language: str, scenario: str, output: Path) -> None:
    subprocess.run([str(executable), str(pack), language, scenario, str(output)], check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preview-exe", type=Path, required=True)
    parser.add_argument("--manifest", type=Path,
                        default=Path(__file__).resolve().parents[1] / "assets/resource_manifest.json")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--contact-sheet", type=Path, required=True)
    args = parser.parse_args()

    from PIL import Image, ImageDraw

    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.contact_sheet.parent.mkdir(parents=True, exist_ok=True)
    pack = args.output_dir / "product.wrp2"
    pack.write_bytes(build_pack(args.manifest))

    sheet = Image.new("RGB", (260 * len(SCENARIOS), 350 * len(LANGUAGES)), "#171a1b")
    draw = ImageDraw.Draw(sheet)
    recovery = Image.new("RGB", (520, 700), "#171a1b")
    recovery_draw = ImageDraw.Draw(recovery)
    for row, language in enumerate(LANGUAGES):
        for column, scenario in enumerate(SCENARIOS):
            output = args.output_dir / f"{scenario}-{language}.ppm"
            render(args.preview_exe, pack, language, scenario, output)
            with Image.open(output) as frame:
                if frame.size != (240, 320):
                    raise ValueError(f"unexpected frame dimensions: {output}: {frame.size}")
                sheet.paste(frame.convert("RGB"), (260 * column, 350 * row + 25))
            draw.text((260 * column + 4, 350 * row + 4),
                      f"{scenario} {language}", fill="white")

        for column, scenario in enumerate(("upload", "resource-error")):
            output = args.output_dir / f"recovery-{scenario}-{language}.ppm"
            render(args.preview_exe, "-", language, scenario, output)
            with Image.open(output) as frame:
                recovery.paste(frame.convert("RGB"), (260 * column, 350 * row + 25))
            recovery_draw.text((260 * column + 4, 350 * row + 4),
                               f"{scenario} {language}", fill="white")

    secondary = Image.new("RGB", (260 * len(SECONDARY_SCENARIOS),
                                  350 * len(LANGUAGES)), "#171a1b")
    secondary_draw = ImageDraw.Draw(secondary)
    for row, language in enumerate(LANGUAGES):
        for column, scenario in enumerate(SECONDARY_SCENARIOS):
            output = args.output_dir / f"{scenario}-{language}.ppm"
            render(args.preview_exe, pack, language, scenario, output)
            with Image.open(output) as frame:
                if frame.size != (240, 320):
                    raise ValueError(f"unexpected frame dimensions: {output}: {frame.size}")
                secondary.paste(frame.convert("RGB"), (260 * column, 350 * row + 25))
            secondary_draw.text((260 * column + 4, 350 * row + 4),
                                f"{scenario} {language}", fill="white")

    updated = args.output_dir / "result-updated.ppm"
    refreshed = args.output_dir / "result-refresh.ppm"
    render(args.preview_exe, pack, "en", "result-updated", updated)
    render(args.preview_exe, pack, "en", "result-refresh", refreshed)
    if updated.read_bytes() != refreshed.read_bytes():
        raise ValueError("result refresh leaves stale pixels")

    sheet.save(args.contact_sheet)
    secondary_path = args.contact_sheet.with_name(
        args.contact_sheet.stem + "-secondary" + args.contact_sheet.suffix
    )
    secondary.save(secondary_path)
    recovery_path = args.contact_sheet.with_name(
        args.contact_sheet.stem + "-recovery" + args.contact_sheet.suffix
    )
    recovery.save(recovery_path)
    print(f"wrote {args.contact_sheet}, {secondary_path}, and {recovery_path}; "
          "result refresh matches fresh draw")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
