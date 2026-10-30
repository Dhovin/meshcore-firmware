#pragma once

#if defined(ESP32)

#include <Arduino.h>
#include <MeshCore.h>
#include <WiFi.h>

namespace offband {

class MinimalWebPortal {
public:
  /**
   * Start the minimal web portal on port 80.
   * Safe to call multiple times (idempotent if already running).
   *
   * @param board Optional pointer to MainBoard for battery, manufacturer, and rebooting.
   * @param node_name Optional node name to display.
   * @param role Optional role name (e.g. "Companion", "Observer", "Repeater").
   * @param admin_password Optional HTTP auth password for /update OTA (null or empty = no auth).
   */
  static void begin(mesh::MainBoard* board = nullptr,
                    const char* node_name = nullptr,
                    const char* role = nullptr,
                    const char* admin_password = nullptr);

  /**
   * Stop the web portal and release server resources.
   */
  static void stop();

  /**
   * Check if web portal is currently running.
   */
  static bool isRunning();

  /**
   * Get the current portal URL (e.g. "http://192.168.1.100").
   */
  static const char* getUrl();

  /**
   * Update active node metadata displayed on the dashboard.
   */
  static void setNodeInfo(mesh::MainBoard* board, const char* node_name, const char* role);
};

} // namespace offband

#endif // ESP32
