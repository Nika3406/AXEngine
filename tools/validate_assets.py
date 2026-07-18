#!/usr/bin/env python3
"""Validate file references inside AXEngine's authored project data."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

TEXT_SUFFIXES = {
    ".axscene", ".axproject", ".axattack", ".axcombatset", ".axreaction",
    ".axcombo", ".axvfx", ".axsound", ".axenemy", ".axui", ".axinput",
    ".json", ".axmat", ".axrender", ".axcomponents", ".axtimeline",
}
REFERENCE_SUFFIXES = {
    ".glb", ".gltf", ".png", ".jpg", ".jpeg", ".bmp", ".tga",
    ".wav", ".ogg", ".mp3", ".vert", ".frag", ".axscene", ".axattack",
    ".axcombatset", ".axreaction", ".axcombo", ".axvfx", ".axsound",
    ".axenemy", ".axui", ".axinput", ".axmat", ".json", ".axtimeline",
}
STRING_PATTERN = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')


def authored_files(project_root: Path):
    candidates = [project_root / "project.axproject"]
    candidates.extend((project_root / "assets").rglob("*"))
    for path in candidates:
        if path.is_file() and (
            path.name == "project.axproject" or
            path.suffix.lower() in TEXT_SUFFIXES or
            any(path.name.endswith(suffix) for suffix in TEXT_SUFFIXES)
        ):
            yield path


def normalize_reference(raw: str) -> str | None:
    value = raw.replace("\\/", "/").replace("\\\\", "\\").strip()
    if value.startswith("scripts/assets/"):
        value = "assets/" + value[len("scripts/assets/"):]
    if not value.startswith("assets/"):
        return None
    if Path(value).suffix.lower() not in REFERENCE_SUFFIXES:
        return None
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("project_root", nargs="?", default=".")
    args = parser.parse_args()

    root = Path(args.project_root).resolve()
    missing: dict[str, list[str]] = {}
    legacy: dict[str, list[str]] = {}
    checked = 0

    for source in authored_files(root):
        try:
            text = source.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        rel_source = source.relative_to(root).as_posix()
        for match in STRING_PATTERN.finditer(text):
            raw = match.group(1)
            if raw.startswith("scripts/assets/"):
                legacy.setdefault(raw, []).append(rel_source)
            ref = normalize_reference(raw)
            if ref is None:
                continue
            checked += 1
            if not (root / ref).is_file():
                missing.setdefault(ref, []).append(rel_source)

    if legacy:
        print("Legacy scripts/assets references:")
        for ref, sources in sorted(legacy.items()):
            print(f"  {ref} <- {', '.join(sorted(set(sources)))}")

    if missing:
        print("Missing asset references:")
        for ref, sources in sorted(missing.items()):
            print(f"  {ref} <- {', '.join(sorted(set(sources)))}")
        print(f"\nValidation failed: {len(missing)} missing unique reference(s), {checked} checked.")
        return 1

    print(f"Asset validation passed: {checked} reference(s) checked.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
