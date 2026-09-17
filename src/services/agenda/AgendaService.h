#pragma once

// AGENDA-PATCH: downloads the calendar/TODO image a self-hosted server
// renders, and caches it on the SD card for SleepActivity to display. All
// code for this feature lives under src/services/agenda/ and
// src/activities/boot_sleep (SleepActivity's AGENDA case) to keep the
// upstream diff small; see AGENDA_PATCH.md for the full list of contact
// points in shared files.

class AgendaService {
 public:
  enum class RefreshResult {
    Ok,
    NotConfigured,   // agendaServerUrl is empty
    WifiNotConnected,
    HeapTooLow,      // below HttpDownloader::MIN_TLS_FREE_HEAP/MIN_TLS_MAX_ALLOC
    DownloadFailed,
  };

  // Downloads SETTINGS.agendaServerUrl (+ ?token=SETTINGS.agendaServerToken)
  // to a .tmp file on the SD card and renames it into place only once the
  // download succeeds, so a failed refresh never corrupts the last-good
  // image SleepActivity reads. Safe to call with Wi-Fi already connected;
  // does not manage the Wi-Fi connection itself.
  static RefreshResult refresh();

  // Path SleepActivity::renderAgendaSleepScreen() reads.
  static constexpr const char* CACHED_IMAGE_PATH = "/.crosspoint/agenda.bmp";
};
