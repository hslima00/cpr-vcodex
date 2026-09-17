#!/usr/bin/env python3
"""Guards against SLEEP_SCREEN_MODE::AGENDA colliding with an upstream addition.

Run after rebasing the `agenda` branch onto a new upstream tag, before
building or publishing anything. Exits non-zero with a clear message if
AGENDA is missing (dropped during conflict resolution) or its numeric value
now collides with another entry (upstream claimed the same index).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

SETTINGS_FILE = Path(__file__).resolve().parent.parent / "src" / "CrossPointSettings.h"
REQUIRED_NAME = "AGENDA"


def main() -> int:
    text = SETTINGS_FILE.read_text(encoding="utf-8")
    match = re.search(r"enum SLEEP_SCREEN_MODE\s*\{(.*?)\};", text, re.DOTALL)
    if not match:
        print("::error::Could not find 'enum SLEEP_SCREEN_MODE { ... };' in CrossPointSettings.h")
        return 1

    entries: dict[str, int] = {}
    next_value = 0
    for raw_line in match.group(1).splitlines():
        line = raw_line.split("//")[0].strip().rstrip(",")
        if not line or line == "SLEEP_SCREEN_MODE_COUNT":
            continue
        name_match = re.match(r"^(\w+)(?:\s*=\s*(-?\d+))?$", line)
        if not name_match:
            print(f"::error::Unparseable line in SLEEP_SCREEN_MODE: {raw_line!r}")
            return 1
        name = name_match.group(1)
        if name_match.group(2) is not None:
            next_value = int(name_match.group(2))
        entries[name] = next_value
        next_value += 1

    if REQUIRED_NAME not in entries:
        print(
            f"::error::SLEEP_SCREEN_MODE::{REQUIRED_NAME} is missing from CrossPointSettings.h "
            "(likely dropped while resolving a rebase conflict)."
        )
        return 1

    agenda_value = entries[REQUIRED_NAME]
    collisions = [name for name, value in entries.items() if value == agenda_value and name != REQUIRED_NAME]
    if collisions:
        print(
            f"::error::SLEEP_SCREEN_MODE::{REQUIRED_NAME} = {agenda_value} collides with "
            f"{', '.join(collisions)}. Upstream claimed this index — bump AGENDA's value in "
            "src/CrossPointSettings.h (and update the matching entries in SettingsActivity.cpp, "
            "CrossPointWebServer.cpp and SleepActivity.cpp; see AGENDA_PATCH.md)."
        )
        return 1

    print(f"OK: SLEEP_SCREEN_MODE::{REQUIRED_NAME} = {agenda_value}, no collisions among {len(entries)} entries.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
