#pragma once

#include <Arduino.h>
#include <MeshCore.h>

#if defined(OFFBAND_OBSERVER) || defined(OFFBAND_WEB_PORTAL)

#if defined(OFFBAND_WEB_PORTAL)
#include <helpers/wifi/MinimalWebPortal.h>
#endif
#if defined(OFFBAND_OBSERVER)
  #include <helpers/wifi_observer/WifiObserver.h>
  #include <helpers/wifi_observer/ObserverPipeline.h>
#endif

class MyMesh;

namespace offband {

class OffbandExtension {
private:
  mesh::MainBoard* board_ = nullptr;
  MyMesh* mesh_ = nullptr;
  bool wifi_connected_ = false;
  bool ntp_started_ = false;
  bool ntp_synced_ = false;

public:
  void begin(mesh::MainBoard* board, MyMesh* mesh);
  void onWifiConnected(IPAddress ip);
  void onWifiDisconnected();
  void loop();
  bool handleCommand(const char* command, char* reply, size_t max_len);

  bool isNtpSynced() const { return ntp_synced_; }
  bool isWifiConnected() const { return wifi_connected_; }

  static OffbandExtension& instance();
};

} // namespace offband

#endif
