#include "AgendaService.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"

namespace {
constexpr const char* TMP_PATH = "/.crosspoint/agenda.bmp.tmp";

std::string buildUrl() {
  std::string url = SETTINGS.agendaServerUrl;
  if (SETTINGS.agendaServerToken[0] != '\0') {
    url += (url.find('?') == std::string::npos) ? '?' : '&';
    url += "token=";
    url += SETTINGS.agendaServerToken;
  }
  return url;
}
}  // namespace

AgendaService::RefreshResult AgendaService::refresh() {
  if (SETTINGS.agendaServerUrl[0] == '\0') {
    return RefreshResult::NotConfigured;
  }
  if (WiFi.status() != WL_CONNECTED) {
    return RefreshResult::WifiNotConnected;
  }
  // Same pre-flight floor every other TLS transfer in the firmware checks
  // before starting: below this a session can MEMORY_E mid-stream.
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
      ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    LOG_ERR("AGENDA", "Skipping refresh: heap too low (free=%u maxAlloc=%u)", ESP.getFreeHeap(),
            ESP.getMaxAllocHeap());
    return RefreshResult::HeapTooLow;
  }

  Storage.ensureDirectoryExists("/.crosspoint");
  Storage.remove(TMP_PATH);

  const std::string url = buildUrl();
  const auto result = HttpDownloader::downloadToFile(url, TMP_PATH);
  if (result != HttpDownloader::OK) {
    LOG_ERR("AGENDA", "Download failed: %d", result);
    Storage.remove(TMP_PATH);
    return RefreshResult::DownloadFailed;
  }

  // Only replace the last-good image once the new one is fully on disk, so a
  // refresh that fails partway never leaves SleepActivity with a corrupt file.
  Storage.remove(CACHED_IMAGE_PATH);
  if (!Storage.rename(TMP_PATH, CACHED_IMAGE_PATH)) {
    LOG_ERR("AGENDA", "Failed to rename %s to %s", TMP_PATH, CACHED_IMAGE_PATH);
    Storage.remove(TMP_PATH);
    return RefreshResult::DownloadFailed;
  }

  LOG_DBG("AGENDA", "Refreshed agenda image");
  return RefreshResult::Ok;
}
