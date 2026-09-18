#include "AgendaService.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"

namespace {
constexpr const char* TMP_PATH = "/.crosspoint/agenda.bmp.tmp";
constexpr uint32_t MDNS_QUERY_TIMEOUT_MS = 3000;

std::string buildUrl() {
  std::string url = SETTINGS.agendaServerUrl;
  if (SETTINGS.agendaServerToken[0] != '\0') {
    url += (url.find('?') == std::string::npos) ? '?' : '&';
    url += "token=";
    url += SETTINGS.agendaServerToken;
  }
  return url;
}

// AGENDA-PATCH: HttpDownloader's underlying HTTP client resolves plain DNS
// only, so a server advertised as e.g. "agenda.local" (common on a phone
// hotspot with no fixed IP — see xteink-agenda-server's mDNS support) needs
// resolving here first. Returns the URL unchanged if its host isn't
// "*.local"; returns an empty string if it is and resolution fails.
std::string resolveMdnsHost(const std::string& url) {
  const size_t schemeEnd = url.find("://");
  if (schemeEnd == std::string::npos) return url;
  const size_t hostStart = schemeEnd + 3;
  const size_t hostEnd = url.find_first_of(":/", hostStart);
  const size_t hostLen = (hostEnd == std::string::npos) ? std::string::npos : hostEnd - hostStart;
  const std::string host = url.substr(hostStart, hostLen);

  constexpr char kLocalSuffix[] = ".local";
  constexpr size_t kLocalSuffixLen = sizeof(kLocalSuffix) - 1;
  if (host.size() <= kLocalSuffixLen || host.compare(host.size() - kLocalSuffixLen, kLocalSuffixLen, kLocalSuffix) != 0) {
    return url;
  }
  const std::string shortName = host.substr(0, host.size() - kLocalSuffixLen);

  MDNS.end();
  if (!MDNS.begin("agenda-client")) {
    LOG_ERR("AGENDA", "mDNS init failed, cannot resolve %s", host.c_str());
    return "";
  }
  const IPAddress resolved = MDNS.queryHost(shortName.c_str(), MDNS_QUERY_TIMEOUT_MS);
  MDNS.end();

  if (resolved == IPAddress(0, 0, 0, 0)) {
    LOG_ERR("AGENDA", "mDNS could not resolve %s", host.c_str());
    return "";
  }

  LOG_DBG("AGENDA", "Resolved %s -> %s", host.c_str(), resolved.toString().c_str());
  std::string resolvedUrl = url.substr(0, hostStart) + resolved.toString().c_str();
  if (hostEnd != std::string::npos) {
    resolvedUrl += url.substr(hostEnd);
  }
  return resolvedUrl;
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

  const std::string url = resolveMdnsHost(buildUrl());
  if (url.empty()) {
    return RefreshResult::DownloadFailed;
  }

  Storage.ensureDirectoryExists("/.crosspoint");
  Storage.remove(TMP_PATH);

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
