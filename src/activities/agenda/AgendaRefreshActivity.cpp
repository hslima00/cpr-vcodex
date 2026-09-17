#include "AgendaRefreshActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "services/agenda/AgendaService.h"

namespace {
void wifiOff() {
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
}
}  // namespace

void AgendaRefreshActivity::onEnter() {
  Activity::onEnter();
  wifiConnectedOnEnter = WiFi.status() == WL_CONNECTED;
  connectedInActivity = false;

  if (SETTINGS.agendaServerUrl[0] == '\0') {
    state = NOT_CONFIGURED;
    return;
  }

  if (wifiConnectedOnEnter) {
    beginRefresh();
    return;
  }

  LOG_INF("AGENDA", "Manual refresh requested without WiFi, launching WiFi selection");
  WiFi.mode(WIFI_STA);
  openWifiSelection();
}

void AgendaRefreshActivity::onExit() {
  Activity::onExit();

  // Only tear WiFi down if this activity brought it up; a connection that was
  // already live on entry belongs to whoever opened it.
  if (!wifiConnectedOnEnter && connectedInActivity) {
    wifiOff();
  }
}

void AgendaRefreshActivity::openWifiSelection() {
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void AgendaRefreshActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected || WiFi.status() != WL_CONNECTED) {
    LOG_INF("AGENDA", "WiFi selection cancelled or not connected before manual agenda refresh");
    state = NO_WIFI;
    requestUpdate();
    return;
  }

  if (!wifiConnectedOnEnter) {
    connectedInActivity = true;
  }
  beginRefresh();
}

void AgendaRefreshActivity::beginRefresh() {
  state = REFRESHING;
  requestUpdate();
}

void AgendaRefreshActivity::runRefresh() {
  const auto result = AgendaService::refresh();
  switch (result) {
    case AgendaService::RefreshResult::Ok:
      state = SUCCESS;
      break;
    case AgendaService::RefreshResult::NotConfigured:
      state = NOT_CONFIGURED;
      break;
    case AgendaService::RefreshResult::WifiNotConnected:
      state = NO_WIFI;
      break;
    case AgendaService::RefreshResult::HeapTooLow:
    case AgendaService::RefreshResult::DownloadFailed:
      state = FAILED;
      break;
  }
  requestUpdate();
}

void AgendaRefreshActivity::loop() {
  if (state == REFRESHING) {
    // First-tick: render the "Refreshing..." screen, then perform the (blocking) download.
    // requestUpdateAndWait below forces the render before we block on the network.
    requestUpdateAndWait();
    runRefresh();
    return;
  }

  int x = 0;
  int y = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void AgendaRefreshActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_REFRESH_AGENDA));

  const int midY = pageHeight / 2;

  switch (state) {
    case REFRESHING:
      renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_AGENDA_REFRESHING));
      break;
    case SUCCESS:
      renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_AGENDA_REFRESH_OK), true, EpdFontFamily::BOLD);
      break;
    case NOT_CONFIGURED:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_AGENDA_REFRESH_FAILED), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_AGENDA_NOT_CONFIGURED));
      break;
    case NO_WIFI:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_CLOCK_SYNC_NO_WIFI), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_CLOCK_SYNC_NO_WIFI_HINT));
      break;
    case FAILED:
      renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_AGENDA_REFRESH_FAILED), true, EpdFontFamily::BOLD);
      break;
  }

  if (state != REFRESHING) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OK_BUTTON), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
