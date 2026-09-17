#!/usr/bin/env python3
"""Writes docs/firmware/manifest.json for the agenda fork's own OTA checks.

The stock firmware checks franssjz/cpr-vcodex's GitHub Pages manifest, then
falls back to raw.githubusercontent.com. OtaUpdater.cpp on this fork points
both of those at hslima00/cpr-vcodex instead, so this script produces the
manifest that fallback URL needs to actually resolve to something — a much
smaller job than upstream's sync_autoflash_firmware.py, since this fork only
ever ships the one C3 X4/X3 binary (no X4 Pro asset, no flash.html to keep
in sync).

Usage:
    python scripts/agenda_ota_manifest.py --repo hslima00/cpr-vcodex --tag <tag>
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path

MANIFEST_PATH = Path(__file__).resolve().parent.parent / "docs" / "firmware" / "manifest.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, help="owner/repo of the fork, e.g. hslima00/cpr-vcodex")
    parser.add_argument("--tag", required=True, help="release tag to point the manifest at")
    args = parser.parse_args()

    asset_name = f"{args.tag}.bin"
    download_url = f"https://github.com/{args.repo}/releases/download/{args.tag}/{asset_name}"

    print(f"Downloading {download_url} to compute size/sha256...")
    with urllib.request.urlopen(download_url) as response:
        data = response.read()

    sha256 = hashlib.sha256(data).hexdigest()
    size = len(data)

    manifest = {
        "name": "CPR-vCodex (agenda fork)",
        "version": args.tag,
        "downloadUrl": download_url,
        "size": size,
        "sha256": sha256,
        "source": {
            "type": "github-release",
            "repo": args.repo,
            "tag": args.tag,
            "asset": asset_name,
        },
    }

    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {MANIFEST_PATH} (version={args.tag}, size={size}, sha256={sha256})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
