# Agenda patch — contact points with upstream files

This file tracks every place the "Agenda" sleep-screen feature touches a file
that also changes upstream (`franssjz/cpr-vcodex`) or in the fork's own
`upstream` branch. Every touch point below is also marked in the source with
a `// AGENDA-PATCH` comment (or `# AGENDA-PATCH` isn't used in YAML, since
the i18n loader rejects comment lines — see the i18n section below).

New files this feature owns outright (not listed as contact points, since a
rebase can never conflict on a file upstream doesn't have):

- `src/services/agenda/AgendaService.h` / `.cpp`
- `src/activities/agenda/AgendaRefreshActivity.h` / `.cpp`

## Contact points in shared files

### `src/CrossPointSettings.h`
- `SLEEP_SCREEN_MODE::AGENDA = 13` — appended after the existing upstream
  additions (`QUICK_RESUME = 11`, `TRANSPARENT_CUSTOM = 12`), right before
  `SLEEP_SCREEN_MODE_COUNT`. **If upstream adds its own value 13, this
  collides** — see "Enum collision" below.
- `agendaServerUrl[128]`, `agendaServerToken[64]`, `agendaUpdateOnSleep` —
  appended right before the "Upstream field-name aliases" block.
- `agendaRefreshShortcut`, `agendaRefreshShortcutOrder`,
  `agendaRefreshShortcutVisible` — appended after `opdsBrowserShortcut*`.

### `src/JsonSettingsIO.cpp`
- Load block: `agendaServerUrl`, `agendaServerToken` (obfuscated, same
  pattern as `opdsPassword`), `agendaUpdateOnSleep`, and the three
  `agendaRefreshShortcut*` fields (load + save, in the shortcut
  location/order/visible blocks).
- Save block: mirrors the load block.

### `src/activities/boot_sleep/SleepActivity.h` / `.cpp`
- New method `renderAgendaSleepScreen()`, declared next to the other
  `render*SleepScreen()` methods.
- One new `case` in the `switch (SETTINGS.sleepScreen)` in `onEnter()`.
- The function body only reads `/.crosspoint/agenda.bmp` from the SD card
  (via the existing `Bitmap`/`SleepScreenCache`/`renderBitmapSleepScreen`
  helpers) and falls back to `renderDefaultSleepScreen()`. No network code.

### `src/activities/settings/SettingsActivity.cpp`
- One line in `buildSleepScreenValues()`:
  `values[CrossPointSettings::AGENDA] = StrId::STR_AGENDA;`

### `src/network/CrossPointWebServer.cpp`
- `WebDynamicSetting` enum: `AgendaServerUrl`, `AgendaServerToken` appended.
- `WEB_SETTINGS[]`: two `WEB_DYNAMIC_STRING` rows (URL, token — both under
  `STR_CAT_DISPLAY`), one `WEB_TOGGLE` row (`agendaUpdateOnSleep`), one
  `WEB_ENUM` row (`agendaRefreshShortcut`, under `STR_SHORTCUTS_SECTION`).
- `handleGetSettings()` / `handlePostSettings()`: one `case` each for
  `AgendaServerUrl` / `AgendaServerToken` (token is write-only, same
  convention as `KoPassword` — never round-trips to the browser).

### `src/util/ShortcutRegistry.h`
- `ShortcutId::AgendaRefresh` appended to the enum.
- `getShortcutDefinitions()` array size bumped `17 → 18`; one new
  `ShortcutDefinition` entry appended. **If upstream adds its own 18th
  shortcut, the array size bump conflicts but merges trivially** — this is
  a much softer collision than the enum one above, since it's a compile
  error either way, not a silent misbehavior.

### `src/activities/apps/AppsActivity.cpp`
- One `#include "activities/agenda/AgendaRefreshActivity.h"`.
- One `case ShortcutId::AgendaRefresh:` in the exhaustive switch in
  `openApp()` (the switch has no `default:`, so forgetting this is a
  compile error, not a silent bug).

### `src/activities/apps/SyncDayActivity.cpp`
- One `#include "services/agenda/AgendaService.h"`.
- One `AgendaService::refresh()` call at the end of `syncTime()`, only on
  success — silent, fire-and-forget, same spirit as the reading-stats
  auto-backup call right above it.

### `src/main.cpp`
- One `#include "services/agenda/AgendaService.h"`.
- One gated `AgendaService::refresh()` call in `enterDeepSleep()`, before
  `activityManager.goToSleep()`. Gated on `agendaUpdateOnSleep` (off by
  default) AND `sleepScreen == AGENDA`. **Does not bring Wi-Fi up itself** —
  only refreshes if Wi-Fi already happens to be connected. See the comment
  in `main.cpp` for why (this code runs seconds before deep sleep; a
  hung/slow Wi-Fi connect here is a worse outcome than a stale image).

### `lib/I18n/translations/english.yaml`
- All new keys are appended at the very end of the file, after
  `STR_NO_BIN_FILES`. Comment lines (`#`) are **not** supported by the
  custom YAML parser in `scripts/gen_i18n.py` (it aborts on any line that
  isn't a blank line or `KEY: "value"`), so this block has no header —
  everything after `STR_NO_BIN_FILES` is Agenda's.
- Other languages auto-fill these keys from English (with a build-time
  warning) until someone translates them; no other translation file needs
  editing.

## Enum collision risk (SLEEP_SCREEN_MODE)

`AGENDA = 13` is only safe as long as upstream (or a future fork rebase)
doesn't also claim index 13. Before merging `agenda` onto a new upstream
tag, check `SLEEP_SCREEN_MODE_COUNT` and every explicit `= N` in the enum in
`src/CrossPointSettings.h` — if upstream added anything between 11 and 13,
bump `AGENDA`'s value and grep the codebase for the old numeric value in any
saved-settings-migration code before shipping. The GitHub Actions workflow
(`.github/workflows/agenda-sync.yml`) automates this check and refuses to
publish a release if it detects a collision — see that workflow for the
exact logic.

## OTA / auto-update

`src/network/OtaUpdater.cpp` points its three update-check URLs at
`franssjz/cpr-vcodex`'s GitHub Pages manifest, raw-Actions release JSON, and
GitHub Releases API. **Decision: redirect these three constants to this
fork's releases** (`hslima00/cpr-vcodex`) rather than disabling OTA,
so the device keeps receiving automatic updates — just from the fork
instead of upstream. This is a fourth contact point in a shared file but
isn't part of the Agenda feature logic itself; see the OTA section of the
GitHub Actions workflow for how the release is published there.

## Rebase checklist

1. `git fetch upstream && git log upstream/master -1` — note the new tag.
2. `git checkout upstream && git reset --hard upstream/master && git push`.
3. `git checkout agenda && git rebase upstream`.
4. Resolve conflicts using this file as the map — every conflict should be
   inside one of the sections above. If a conflict shows up anywhere else,
   the patch surface has grown beyond what's documented here; update this
   file in the same commit that resolves it.
5. Re-check the "Enum collision risk" section by hand even if the rebase
   applied cleanly (a clean merge of the enum block doesn't prove upstream
   didn't independently pick 13 for something else in a non-conflicting
   spot).
6. Build `default` and `gh_release`, run `scripts/pre_release_check.py`.
