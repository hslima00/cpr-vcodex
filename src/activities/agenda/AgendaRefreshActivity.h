#pragma once

#include "activities/Activity.h"

// AGENDA-PATCH: manual "Refresh Agenda" shortcut action. Connects to WiFi if
// needed (reusing the normal WiFi selection flow, same as ClockSyncActivity),
// downloads a fresh agenda image, reports success/failure, then waits for
// Back / OK / a screen tap. Modeled directly on ClockSyncActivity.
class AgendaRefreshActivity final : public Activity {
 public:
  explicit AgendaRefreshActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AgendaRefresh", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return state == REFRESHING; }
  void render(RenderLock&&) override;

 private:
  enum State { REFRESHING, SUCCESS, NOT_CONFIGURED, NO_WIFI, FAILED };
  State state = REFRESHING;
  bool wifiConnectedOnEnter = false;
  bool connectedInActivity = false;

  void beginRefresh();
  void openWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void runRefresh();
};
