#include "MinimalWebPortal.h"

#if defined(ESP32)

#include <SPIFFS.h>
#include <Preferences.h>
#include <esp_system.h>

#if __has_include(<ESPAsyncWebServer.h>) && __has_include(<AsyncElegantOTA.h>)
  #include <ESPAsyncWebServer.h>
  #include <AsyncElegantOTA.h>
  #define HAS_ASYNC_WEB_PORTAL 1
#else
  #define HAS_ASYNC_WEB_PORTAL 0
#endif

#if defined(OFFBAND_OBSERVER) || defined(WITH_MQTT_UPLINK)
  #include "helpers/wifi_observer/ConfigSchema.h"
  #include "helpers/wifi_observer/ObserverCli.h"
  #define HAS_OBSERVER_CONFIG 1
#else
  #define HAS_OBSERVER_CONFIG 0
#endif

#if defined(OFFBAND_MQTT_POOL)
  #include "helpers/wifi_telemetry/RepeaterMqttPool.h"
#endif

namespace offband {

#if HAS_ASYNC_WEB_PORTAL

static AsyncWebServer* g_portal_server = nullptr;
static mesh::MainBoard* g_board = nullptr;
static char g_node_name[32] = {0};
static char g_role[32] = {0};
static char g_admin_pass[32] = {0};
static char g_portal_url[80] = {0};
static bool g_reboot_pending = false;
static uint32_t g_reboot_at = 0;

static String formatUptime() {
  uint32_t s = millis() / 1000;
  uint32_t d = s / 86400;
  s %= 86400;
  uint32_t h = s / 3600;
  s %= 3600;
  uint32_t m = s / 60;
  s %= 60;
  char buf[48];
  if (d > 0) {
    snprintf(buf, sizeof(buf), "%ud %uh %um %us", (unsigned)d, (unsigned)h, (unsigned)m, (unsigned)s);
  } else if (h > 0) {
    snprintf(buf, sizeof(buf), "%uh %um %us", (unsigned)h, (unsigned)m, (unsigned)s);
  } else {
    snprintf(buf, sizeof(buf), "%um %us", (unsigned)m, (unsigned)s);
  }
  return String(buf);
}

static const char kPageHeader[] PROGMEM = 
  "<!DOCTYPE html><html><head><meta charset='utf-8'>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>MeshCore Portal</title>"
  "<style>"
  "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:#121316;color:#e1e4e8;margin:0;padding:16px;line-height:1.5;}"
  ".wrap{max-width:640px;margin:0 auto;}"
  ".card{background:#1c1e24;border:1px solid #2d3139;border-radius:8px;padding:16px;margin-bottom:16px;box-shadow:0 2px 4px rgba(0,0,0,0.3);}"
  "h1,h2,h3{margin-top:0;color:#58a6ff;}"
  "h1{font-size:22px;}h2{font-size:18px;margin-bottom:12px;}h3{font-size:15px;color:#8b949e;}"
  "table{width:100%;border-collapse:collapse;margin:8px 0;}"
  "td{padding:6px 4px;border-bottom:1px solid #262930;font-size:14px;}"
  "td:first-child{color:#8b949e;width:40%;}"
  ".badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:12px;font-weight:600;background:#1f6feb;color:#fff;}"
  ".btn{display:inline-block;padding:8px 14px;margin:4px 4px 4px 0;background:#238636;color:#fff;text-decoration:none;border-radius:6px;border:none;font-weight:600;font-size:13px;cursor:pointer;}"
  ".btn-sec{background:#30363d;color:#c9d1d9;}"
  ".btn-danger{background:#da3633;}"
  ".btn:hover{filter:brightness(1.15);}"
  "input[type=text],input[type=password],input[type=number],select{width:100%;box-sizing:border-box;padding:8px;background:#0d1117;border:1px solid #30363d;border-radius:6px;color:#c9d1d9;font-size:14px;margin:4px 0 12px;}"
  "label{font-size:13px;font-weight:600;color:#8b949e;display:block;margin-top:8px;}"
  ".help{font-size:12px;color:#6e7681;margin-top:-8px;margin-bottom:10px;}"
  ".nav{margin-bottom:16px;}"
  ".nav a{color:#58a6ff;text-decoration:none;font-size:14px;margin-right:12px;font-weight:600;}"
  ".nav a:hover{text-decoration:underline;}"
  "</style></head><body><div class='wrap'>";

static const char kPageFooter[] PROGMEM = 
  "<div style='text-align:center;font-size:12px;color:#6e7681;margin-top:24px;'>"
  "MeshCore & Offband Firmware &bull; Plain HTTP Port 80"
  "</div></div></body></html>";

static void registerRoutes(AsyncWebServer* server) {
  // GET / -> Live Status Dashboard
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = FPSTR(kPageHeader);
    html += "<div class='nav'>";
    html += "<a href='/'>Dashboard</a>";
    html += "<a href='/mqtt'>MQTT Config</a>";
    html += "<a href='/update'>OTA Update</a>";
    html += "<a href='/log'>Packet Log</a>";
    html += "</div>";

    html += "<div class='card'>";
    html += "<h1>" + (g_node_name[0] ? String(g_node_name) : String("MeshCore Node")) + "</h1>";
    html += "<div><span class='badge'>" + (g_role[0] ? String(g_role) : String("Radio Node")) + "</span> ";
    if (g_board) {
      html += "<span style='font-size:13px;color:#8b949e;'>" + String(g_board->getManufacturerName()) + "</span>";
    }
    html += "</div>";
    html += "</div>";

    // System Status Card
    html += "<div class='card'>";
    html += "<h2>System & Network</h2><table>";
    html += "<tr><td>IP Address</td><td><strong>" + (WiFi.localIP().toString()) + "</strong></td></tr>";
    html += "<tr><td>Wi-Fi MAC</td><td>" + WiFi.macAddress() + "</td></tr>";
    html += "<tr><td>Wi-Fi RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
    html += "<tr><td>Uptime</td><td>" + formatUptime() + "</td></tr>";

    if (g_board) {
      uint16_t mv = g_board->getBattMilliVolts();
      int pct = 0;
      if (mv > 3300) {
        pct = constrain((int)((mv - 3300) * 100 / (4200 - 3300)), 0, 100);
      }
      html += "<tr><td>Battery</td><td>" + String(mv) + " mV (" + String(pct) + "%)</td></tr>";
      if (g_board->hasUsbPowerDetect()) {
        html += "<tr><td>USB Power</td><td>" + String(g_board->isUsbPowered() ? "Connected" : "Disconnected") + "</td></tr>";
      }
    }
    html += "</table></div>";

    // Memory & PSRAM Card
    html += "<div class='card'>";
    html += "<h2>Memory & PSRAM</h2><table>";
    html += "<tr><td>Free Heap</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB (min " + String(ESP.getMinFreeHeap() / 1024) + " KB)</td></tr>";
    size_t psram_size = ESP.getPsramSize();
    if (psram_size > 0) {
      html += "<tr><td>Total PSRAM</td><td>" + String(psram_size / 1024) + " KB</td></tr>";
      html += "<tr><td>Free PSRAM</td><td><strong>" + String(ESP.getFreePsram() / 1024) + " KB</strong></td></tr>";
    } else {
      html += "<tr><td>PSRAM</td><td><span style='color:#e3b341;'>Not detected / disabled</span></td></tr>";
    }
    html += "<tr><td>CPU Frequency</td><td>" + String(ESP.getCpuFreqMHz()) + " MHz</td></tr>";
    html += "</table></div>";

    // Quick Actions Card
    html += "<div class='card'>";
    html += "<h2>Actions</h2>";
    html += "<a href='/mqtt' class='btn'>MQTT Configuration</a>";
    html += "<a href='/update' class='btn btn-sec'>Firmware OTA Update</a>";
    html += "<a href='/log' class='btn btn-sec'>Download Packet Log</a>";
    html += "<button onclick=\"if(confirm('Reboot node now?')){fetch('/api/reboot',{method:'POST'}).then(()=>{alert('Rebooting...');location.reload();});}\" class='btn btn-danger'>Reboot Device</button>";
    html += "</div>";

    html += FPSTR(kPageFooter);
    request->send(200, "text/html", html);
  });

  // GET /mqtt -> MQTT Settings Form
  server->on("/mqtt", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = FPSTR(kPageHeader);
    html += "<div class='nav'>";
    html += "<a href='/'>&larr; Dashboard</a>";
    html += "</div>";

    // Read current settings
    String host = "192.168.1.100";
    uint16_t port = 1883;
    uint8_t transport = 0; // 0 = plain, 1 = tls
    uint8_t auth = 0;      // 0 = none, 1 = basic, 2 = jwt
    String user = "";
    String prefix = "meshcore/v1";
    String iata = "XYZ";
    uint16_t interval = 60;
    bool enabled = true;

#if HAS_OBSERVER_CONFIG
    BrokerConfig cfg;
    if (readBrokerConfig(0, cfg)) {
      if (cfg.url[0]) host = cfg.url;
      if (cfg.port > 0) port = cfg.port;
      transport = (uint8_t)cfg.transport;
      auth = (uint8_t)cfg.auth_type;
      user = cfg.username;
      enabled = cfg.enabled;
      if (cfg.topic_prefix[0]) prefix = cfg.topic_prefix;
      if (cfg.iata_override[0]) iata = cfg.iata_override;
    }
    char giata[16] = {0};
    if (readGlobalIata(giata, sizeof(giata)) && giata[0]) {
      if (iata == "XYZ") iata = giata;
    }
    uint16_t s_int = readStatusIntervalSec();
    if (s_int > 0) interval = s_int;
#else
    Preferences p;
    if (p.begin("mqtt", true)) {
      host = p.getString("url", host);
      port = p.getUShort("port", port);
      transport = p.getUChar("trans", transport);
      auth = p.getUChar("auth", auth);
      user = p.getString("user", user);
      prefix = p.getString("prefix", prefix);
      iata = p.getString("iata", iata);
      interval = p.getUShort("int", interval);
      enabled = p.getBool("en", enabled);
      p.end();
    }
#endif

    html += "<div class='card'>";
    html += "<h2>MQTT Configuration</h2>";
    html += "<p style='font-size:13px;color:#8b949e;'>Configure LoRa packet uplink and node telemetry to your MQTT broker.</p>";

    html += "<form method='POST' action='/mqtt'>";
    
    html += "<label>Broker Status</label>";
    html += "<select name='enabled'>";
    html += "<option value='1'" + String(enabled ? " selected" : "") + ">Enabled</option>";
    html += "<option value='0'" + String(!enabled ? " selected" : "") + ">Disabled</option>";
    html += "</select>";

    html += "<label>Broker Host / IP Address</label>";
    html += "<input type='text' name='host' value='" + host + "' placeholder='e.g. 192.168.1.100 or broker.hivemq.com' required>";

    html += "<label>Port</label>";
    html += "<input type='number' name='port' value='" + String(port) + "' min='1' max='65535' required>";
    html += "<div class='help'>Default: 1883 for Plain TCP, 8883 for TLS</div>";

    html += "<label>Transport (TLS is not needed for local setups)</label>";
    html += "<select name='transport'>";
    html += "<option value='0'" + String(transport == 0 ? " selected" : "") + ">Plain TCP (Recommended for Heltec V4, zero TLS overhead)</option>";
    html += "<option value='1'" + String(transport == 1 ? " selected" : "") + ">TLS Encrypted (Requires CA cert in flash)</option>";
    html += "</select>";

    html += "<label>Authentication</label>";
    html += "<select name='auth'>";
    html += "<option value='0'" + String(auth == 0 ? " selected" : "") + ">None / Anonymous</option>";
    html += "<option value='1'" + String(auth == 1 ? " selected" : "") + ">Username & Password</option>";
    html += "<option value='2'" + String(auth == 2 ? " selected" : "") + ">JWT Key Token</option>";
    html += "</select>";

    html += "<label>Username</label>";
    html += "<input type='text' name='username' value='" + user + "' placeholder='Optional username'>";

    html += "<label>Password</label>";
    html += "<input type='password' name='password' placeholder='Leave blank to keep existing password'>";

    html += "<label>Topic Prefix</label>";
    html += "<input type='text' name='topic_prefix' value='" + prefix + "' placeholder='meshcore/v1'>";
    html += "<div class='help'>Packets will be published under &lt;prefix&gt;/packets/&lt;iata&gt;</div>";

    html += "<label>Region / IATA Code</label>";
    html += "<input type='text' name='iata' value='" + iata + "' maxlength='8' placeholder='e.g. YYZ, JFK, US-WEST'>";

    html += "<label>Telemetry Interval (seconds)</label>";
    html += "<input type='number' name='interval' value='" + String(interval) + "' min='10' max='3600'>";

    html += "<br><button type='submit' class='btn'>Save & Apply MQTT Config</button>";
    html += " <a href='/' class='btn btn-sec'>Cancel</a>";
    html += "</form>";
    html += "</div>";

    html += FPSTR(kPageFooter);
    request->send(200, "text/html", html);
  });

  // POST /mqtt -> Save settings
  server->on("/mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    String host = request->hasParam("host", true) ? request->getParam("host", true)->value() : "";
    int port = request->hasParam("port", true) ? request->getParam("port", true)->value().toInt() : 1883;
    int transport = request->hasParam("transport", true) ? request->getParam("transport", true)->value().toInt() : 0;
    int auth = request->hasParam("auth", true) ? request->getParam("auth", true)->value().toInt() : 0;
    String user = request->hasParam("username", true) ? request->getParam("username", true)->value() : "";
    String pass = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    String prefix = request->hasParam("topic_prefix", true) ? request->getParam("topic_prefix", true)->value() : "meshcore/v1";
    String iata = request->hasParam("iata", true) ? request->getParam("iata", true)->value() : "XYZ";
    int interval = request->hasParam("interval", true) ? request->getParam("interval", true)->value().toInt() : 60;
    bool enabled = request->hasParam("enabled", true) && request->getParam("enabled", true)->value() == "1";

    if (port <= 0 || port > 65535) port = 1883;
    if (interval < 5) interval = 5;

    // Save into NVS
#if HAS_OBSERVER_CONFIG
    BrokerConfig cfg;
    readBrokerConfig(0, cfg);
    if (host.length() > 0) strncpy(cfg.url, host.c_str(), sizeof(cfg.url) - 1);
    cfg.port = (uint16_t)port;
    cfg.transport = (transport == 1) ? BrokerTransport::TLS : BrokerTransport::PLAIN;
    cfg.auth_type = (auth == 1) ? BrokerAuthType::BASIC : (auth == 2 ? BrokerAuthType::JWT : BrokerAuthType::NONE);
    strncpy(cfg.username, user.c_str(), sizeof(cfg.username) - 1);
    if (pass.length() > 0) strncpy(cfg.password, pass.c_str(), sizeof(cfg.password) - 1);
    strncpy(cfg.topic_prefix, prefix.c_str(), sizeof(cfg.topic_prefix) - 1);
    strncpy(cfg.iata_override, iata.c_str(), sizeof(cfg.iata_override) - 1);
    cfg.enabled = enabled;
    writeBrokerConfig(0, cfg);
    writeGlobalIata(iata.c_str());
    writeStatusIntervalSec((uint16_t)interval);

    // Trigger live reload
    wifiObserverPool().reload();
#else
    Preferences p;
    if (p.begin("mqtt", false)) {
      if (host.length() > 0) p.putString("url", host);
      p.putUShort("port", (uint16_t)port);
      p.putUChar("trans", (uint8_t)transport);
      p.putUChar("auth", (uint8_t)auth);
      p.putString("user", user);
      if (pass.length() > 0) p.putString("pass", pass);
      p.putString("prefix", prefix);
      p.putString("iata", iata);
      p.putUShort("int", (uint16_t)interval);
      p.putBool("en", enabled);
      p.end();
    }
#endif

#if defined(OFFBAND_MQTT_POOL)
    repeaterMqttPool().reload();
#endif

    String html = FPSTR(kPageHeader);
    html += "<div class='card' style='text-align:center;'>";
    html += "<h2>Settings Saved!</h2>";
    html += "<p>MQTT broker settings updated successfully.</p>";
    html += "<p style='color:#58a6ff;'>Reloading portal in 2 seconds...</p>";
    html += "<script>setTimeout(function(){ window.location.href='/mqtt'; }, 2000);</script>";
    html += "</div>";
    html += FPSTR(kPageFooter);
    request->send(200, "text/html", html);
  });

  // GET /log -> Packet Log download
  server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/packet_log")) {
      request->send(SPIFFS, "/packet_log", "text/plain");
    } else {
      request->send(200, "text/plain", "No packet log found in filesystem.");
    }
  });

  // POST /api/reboot -> Restart board
  server->on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Rebooting device...");
    g_reboot_pending = true;
    g_reboot_at = millis() + 1000;
  });
}

#endif // HAS_ASYNC_WEB_PORTAL

void MinimalWebPortal::begin(mesh::MainBoard* board,
                             const char* node_name,
                             const char* role,
                             const char* admin_password) {
#if HAS_ASYNC_WEB_PORTAL
  if (board) g_board = board;
  if (node_name && node_name[0]) {
    strncpy(g_node_name, node_name, sizeof(g_node_name) - 1);
  }
  if (role && role[0]) {
    strncpy(g_role, role, sizeof(g_role) - 1);
  }
  if (admin_password && admin_password[0]) {
    strncpy(g_admin_pass, admin_password, sizeof(g_admin_pass) - 1);
  }

  IPAddress ip = (WiFi.getMode() & WIFI_AP) ? WiFi.softAPIP() : WiFi.localIP();
  snprintf(g_portal_url, sizeof(g_portal_url), "http://%s", ip.toString().c_str());

  if (g_portal_server != nullptr) {
    // Already running
    return;
  }

  if (board) {
    board->setInhibitSleep(true); // Don't enter deep sleep while web portal is active
  }

  g_portal_server = new AsyncWebServer(80);
  registerRoutes(g_portal_server);

  // Configure OTA on /update
  static char ota_id[64];
  snprintf(ota_id, sizeof(ota_id), "%s (%s)",
           g_node_name[0] ? g_node_name : "MeshCore",
           g_board ? g_board->getManufacturerName() : "ESP32");
  AsyncElegantOTA.setID(ota_id);

  if (g_admin_pass[0]) {
    AsyncElegantOTA.begin(g_portal_server, "admin", g_admin_pass);
  } else {
    AsyncElegantOTA.begin(g_portal_server);
  }

  g_portal_server->begin();
  Serial.printf("[WebPortal] Plain HTTP Web Portal running at %s\n", g_portal_url);
#else
  Serial.println("[WebPortal] ESPAsyncWebServer not available in this build");
#endif
}

void MinimalWebPortal::stop() {
#if HAS_ASYNC_WEB_PORTAL
  if (g_portal_server) {
    g_portal_server->end();
    delete g_portal_server;
    g_portal_server = nullptr;
  }
  g_portal_url[0] = 0;
  if (g_board) {
    g_board->setInhibitSleep(false);
  }
#endif
}

bool MinimalWebPortal::isRunning() {
#if HAS_ASYNC_WEB_PORTAL
  return g_portal_server != nullptr;
#else
  return false;
#endif
}

const char* MinimalWebPortal::getUrl() {
#if HAS_ASYNC_WEB_PORTAL
  return g_portal_url;
#else
  return "";
#endif
}

void MinimalWebPortal::setNodeInfo(mesh::MainBoard* board, const char* node_name, const char* role) {
#if HAS_ASYNC_WEB_PORTAL
  if (board) g_board = board;
  if (node_name && node_name[0]) {
    strncpy(g_node_name, node_name, sizeof(g_node_name) - 1);
  }
  if (role && role[0]) {
    strncpy(g_role, role, sizeof(g_role) - 1);
  }
#endif
}

} // namespace offband

#endif // ESP32
