#include "OffbandExtension.h"

#if defined(OFFBAND_OBSERVER) || defined(OFFBAND_WEB_PORTAL)

#include <WiFi.h>
#include "../../examples/companion_radio/MyMesh.h"
#if defined(OFFBAND_WEB_PORTAL)
  #include <helpers/wifi/MinimalWebPortal.h>
#endif
#if defined(OFFBAND_OBSERVER)
  #include <helpers/wifi_observer/WifiObserver.h>
  #include <helpers/wifi_observer/ObserverPipeline.h>
  #include <helpers/wifi_observer/ConfigSchema.h>
  #include <helpers/wifi_observer/ObserverCli.h>
#endif

namespace offband {

static char s_observer_device_id[65] = {0};

static void bytesToHex(const uint8_t* src, size_t src_len, char* dest, size_t dest_size) {
  static const char kHexDigits[] = "0123456789ABCDEF";
  if (dest_size < src_len * 2 + 1) return;
  for (size_t i = 0; i < src_len; ++i) {
    dest[i * 2]     = kHexDigits[(src[i] >> 4) & 0x0F];
    dest[i * 2 + 1] = kHexDigits[src[i] & 0x0F];
  }
  dest[src_len * 2] = '\0';
}

OffbandExtension& OffbandExtension::instance() {
  static OffbandExtension s_instance;
  return s_instance;
}

void OffbandExtension::begin(mesh::MainBoard* board, MyMesh* mesh) {
  board_ = board;
  mesh_ = mesh;

#if defined(OFFBAND_OBSERVER)
  wifiObserverBegin();

  if (mesh_) {
    bytesToHex(mesh_->self_id.pub_key, PUB_KEY_SIZE,
               s_observer_device_id, sizeof(s_observer_device_id));

    wifiObserverSetMeshContext(
        mesh_->self_id,
        s_observer_device_id,
        mesh_->getNodeName(),
        FIRMWARE_VERSION,
        FIRMWARE_VERSION,
        board_ ? board_->getManufacturerName() : "MeshCore");
  }
#endif
}

void OffbandExtension::onWifiConnected(IPAddress ip) {
  wifi_connected_ = true;

#if defined(ESP32)
  if (!ntp_started_) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    ntp_started_ = true;
  }
#endif

#if defined(ESP32) && defined(OFFBAND_WEB_PORTAL)
  const char* node_name = mesh_ ? mesh_->getNodeName() : "MeshCore";
  MinimalWebPortal::begin(board_, node_name, "Observer", nullptr);
#endif
}

void OffbandExtension::onWifiDisconnected() {
  wifi_connected_ = false;
}

void OffbandExtension::loop() {
#if defined(ESP32)
  if (ntp_started_) {
    time_t now = time(nullptr);
    if (now > 1735689600) { // Sane wall clock (> Jan 1, 2025)
      static uint32_t s_last_rtc_sync = 0;
      if (!ntp_synced_ || (millis() - s_last_rtc_sync > 3600000)) { // Sync immediately then every hour
        ntp_synced_ = true;
        s_last_rtc_sync = millis();
        if (mesh_ && mesh_->getRTCClock()) {
          mesh_->getRTCClock()->setCurrentTime((uint32_t)now);
        }
      }
    }
  }
#endif

#if defined(OFFBAND_OBSERVER)
  wifiObserverLoop();
#endif
}

bool OffbandExtension::handleCommand(const char* command, char* reply, size_t max_len) {
  if (command == nullptr || reply == nullptr || max_len == 0) return false;

  // 1. Help topics
  if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0) {
    snprintf(reply, max_len,
      "Help topics:\n"
      "  help mqtt   - MQTT & Observer\n"
      "  help wifi   - Wi-Fi config\n"
      "  help radio  - Radio params\n"
      "  ver, board, clock, ntp, reboot, status");
    return true;
  }
  if (strcmp(command, "help mqtt") == 0) {
    snprintf(reply, max_len,
      "MQTT commands:\n"
      "  mqtt status|view <N>|enable <N>|disable <N>|clear <N>\n"
      "  get/set mqtt.broker.<N>.<key>\n"
      "  help mqtt keys");
    return true;
  }
  if (strcmp(command, "help mqtt keys") == 0) {
    snprintf(reply, max_len,
      "MQTT broker keys:\n"
      "  url, port, transport (tcp|tls|wss),\n"
      "  auth_type (none|basic|jwt),\n"
      "  username, password, ca_cert (letsencrypt|isrg-x2|gts-r4|none)");
    return true;
  }
  if (strcmp(command, "help wifi") == 0) {
    snprintf(reply, max_len,
      "WiFi commands:\n"
      "  wifi status | enable | disable | clear\n"
      "  get/set wifi.ssid <ssid>\n"
      "  get/set wifi.pwd <pwd>\n"
      "  get/set wifi.enabled <0|1>\n"
      "  get wifi.ip");
    return true;
  }
  if (strcmp(command, "help radio") == 0) {
    snprintf(reply, max_len,
      "Radio commands:\n"
      "  get radio\n"
      "  set radio <freq> <bw> <sf> <cr>\n"
      "  get/set af <0-9>\n"
      "  get/set dutycycle <1-100>");
    return true;
  }

  // 2. Status & NTP commands
  if (strcmp(command, "status") == 0) {
    uint32_t uptime = millis() / 1000;
    snprintf(reply, max_len, "Node: %s | Up: %luh%02lum | Heap: %u | WiFi: %s | NTP: %s",
             mesh_ ? mesh_->getNodeName() : "MeshCore",
             (unsigned long)(uptime / 3600), (unsigned long)((uptime % 3600) / 60),
             (unsigned)ESP.getFreeHeap(),
             WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "Down",
             ntp_synced_ ? "Synced" : (wifi_connected_ ? "Syncing" : "Down"));
    return true;
  }

  if (strcmp(command, "ntp") == 0 || strcmp(command, "ntp status") == 0) {
    time_t now = time(nullptr);
    if (now > 1735689600) {
      struct tm* tm = gmtime(&now);
      snprintf(reply, max_len, "NTP: Synced (%04d-%02d-%02d %02d:%02d:%02d UTC)",
               tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
               tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else if (wifi_connected_) {
      snprintf(reply, max_len, "NTP: Syncing with pool.ntp.org...");
    } else {
      snprintf(reply, max_len, "NTP: Wi-Fi disconnected (cannot sync)");
    }
    return true;
  }

  if (strcmp(command, "ntp sync") == 0) {
#if defined(ESP32)
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    ntp_started_ = true;
    snprintf(reply, max_len, "NTP: Sync requested from pool.ntp.org");
#else
    snprintf(reply, max_len, "NTP: Unsupported on this platform");
#endif
    return true;
  }

  // 3. Clock commands
  if (strcmp(command, "clock") == 0) {
    if (mesh_ && mesh_->getRTCClock()) {
      time_t now = (time_t)mesh_->getRTCClock()->getCurrentTime();
      struct tm* tm = gmtime(&now);
      if (tm) {
        snprintf(reply, max_len, "%02d:%02d - %02d/%02d/%04d UTC",
                 tm->tm_hour, tm->tm_min, tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
      } else {
        snprintf(reply, max_len, "UTC time: %lu", (unsigned long)now);
      }
    } else {
      snprintf(reply, max_len, "ERR: clock unavailable");
    }
    return true;
  }
  if (memcmp(command, "time ", 5) == 0) {
    uint32_t secs = strtoul(&command[5], nullptr, 10);
    if (mesh_ && mesh_->getRTCClock()) {
      uint32_t curr = mesh_->getRTCClock()->getCurrentTime();
      if (secs > curr) {
        mesh_->getRTCClock()->setCurrentTime(secs);
        time_t now = (time_t)mesh_->getRTCClock()->getCurrentTime();
        struct tm* tm = gmtime(&now);
        if (tm) {
          snprintf(reply, max_len, "OK - clock set: %02d:%02d - %02d/%02d/%04d UTC",
                   tm->tm_hour, tm->tm_min, tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
        } else {
          snprintf(reply, max_len, "OK - clock set: %lu", (unsigned long)now);
        }
      } else {
        snprintf(reply, max_len, "ERR: clock cannot go backwards");
      }
    } else {
      snprintf(reply, max_len, "ERR: clock unavailable");
    }
    return true;
  }

  // 4. Observer CLI commands (mqtt status, mqtt view, set mqtt.broker, display always on, wifi status, etc.)
#if defined(OFFBAND_OBSERVER)
  if (dispatchObserverCli(command, reply, max_len, wifiObserverPool())) {
    return true;
  }
#endif

  return false;
}

} // namespace offband

#endif
