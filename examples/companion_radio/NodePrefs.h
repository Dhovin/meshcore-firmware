#pragma once
#include <cstdint> // For uint8_t, uint32_t
#include <helpers/ConfigSerializer.h>
#include <helpers/CommonRadioPrefs.h>
#include <helpers/DynamicConfigSerializer.h>
#include <helpers/GpsPowerMode.h>  // #1555: GpsMode + kGpsModeUnset
#include <helpers/PathHashDefault.h>   // #1753: the shared default path-hash mode
#ifdef OFFBAND_GUI
  #include <helpers/gui/GuiPrefs.h>
#endif

#define TELEM_MODE_DENY            0
#define TELEM_MODE_ALLOW_FLAGS     1     // use contact.flags
#define TELEM_MODE_ALLOW_ALL       2

#define ADVERT_LOC_NONE       0
#define ADVERT_LOC_SHARE      1

// #1244: the text size a badge boots into. It is badgeui::kTextLarge, and the badge's
// own BadgeScreens.cpp static_asserts these two against each other -- this header is
// shared by every board, so it cannot include the badge's layout to say so directly.
// Boards without the badge UI carry the value and never read it.
#ifndef DEFAULT_UI_TEXT_SIZE
  #define DEFAULT_UI_TEXT_SIZE  0
#endif

// #1362: the text size MESSAGE screens boot into, split from the navigation one above.
// It defaults to whatever navigation uses, so the sixty-one variants that compile these
// screens keep exactly the behavior they have today; the badge's env sets medium,
// because the badge is the board the split was asked for.
#ifndef DEFAULT_UI_MSG_TEXT_SIZE
  #define DEFAULT_UI_MSG_TEXT_SIZE  DEFAULT_UI_TEXT_SIZE
#endif

// #1245: seconds before the display blanks. 0 means "whatever this board was compiled
// with" -- AUTO_OFF_MILLIS -- so a board with no way to set it behaves exactly as it did.
// The badge's env sets its own, because the badge is the one with the setting.
#ifndef DEFAULT_UI_SCREEN_SECS
  #define DEFAULT_UI_SCREEN_SECS  0
#endif

class NodePrefs : public ConfigSerializer {  // persisted to file
public:
  float airtime_factor = 0;
  char node_name[32];
  double node_lat = 0, node_lon = 0;
  float freq = 0;
  uint8_t sf = 0;
  uint8_t cr = 0;
  uint8_t multi_acks = 0;
  uint8_t manual_add_contacts = 0;
  float bw = 0;
  int8_t tx_power_dbm = 0;
  uint8_t telemetry_mode_base = 0;
  uint8_t telemetry_mode_loc = 0;
  uint8_t telemetry_mode_env = 0;
  float rx_delay_base = 0;
  float tx_delay_factor = 0;
  float direct_tx_delay_factor = 0;
  uint32_t ble_pin = 0;
  uint8_t  advert_loc_policy = 0;
  uint8_t  buzzer_quiet = 0;
  uint8_t  vibe_quiet = 0;

  // #510: notify_scope. 0 = ALL (default, legacy behaviour: all channels + DMs),
  // 1 = SELF (my channel mentions + DMs), 2 = NONE (silent / no buzzer or vibration).
  uint8_t  notify_scope = 0;

  // #368: syslog target persistence. Kept in the companion's NodePrefs alongside
  // repeater/observer CommonCLI so syslog.host / syslog.port / syslog.level /
  // syslog.mask / syslog.transport can be configured uniformly across roles.
  uint8_t  syslog_level = 0;
  uint8_t  syslog_mask = 0;
  uint8_t  syslog_transport = 0;
  char     syslog_host[64];
  uint16_t syslog_port = 514;

  // #428: caplog (MeshLog capture) persistence.
  uint8_t  caplog_enabled = 0;
  uint8_t  caplog_level = 0;

  // #542 B2: status/traffic LED mode (0 = normal, 1 = suppressed/dark).
  uint8_t  ui_led_mode = 0;

  // #542 B1: OLED mode (0 = auto / on with timeout, 1 = always on, 2 = always off / dark).
  uint8_t  display_mode = 0;

  // #1244: font scale for navigation screens (0 = large default, 1 = medium, 2 = small).
  uint8_t  ui_nav_text_size = DEFAULT_UI_TEXT_SIZE;

  // #1362: font scale for message preview and thread view.
  uint8_t  ui_msg_text_size = DEFAULT_UI_MSG_TEXT_SIZE;

  // #1245: auto-blank delay in seconds. 0 = compiled-in AUTO_OFF_MILLIS.
  uint16_t ui_screen_secs = DEFAULT_UI_SCREEN_SECS;

  // #1248: index into TimeZones.h table; 0 = unset / UTC.
  uint8_t  ui_tz_idx = 0;

  // #1250: learned or pinned full battery voltage in millivolts; 0 = unlearned.
  uint16_t batt_full_mv = 0;
  uint8_t  batt_full_pinned = 0;

  // #509: assignable button actions for single, double, triple, and quadruple press.
  uint8_t  button_actions[4] = { 0, 0, 0, 0 };

  uint8_t  gps_enabled = 0;      // GPS enabled flag (0=disabled, 1=enabled)
  uint32_t gps_interval = 0;     // GPS read interval in seconds
  // #1555: HOW GPS runs while it is enabled -- On or Power save. **Not whether it is on;
  // `gps_enabled` above still decides that, with its meaning unchanged, so every existing
  // reader and writer of it stays correct with no translation.**
  //
  // Resolve the two together with offband::gpsModeFromPrefs(gps_mode, gps_enabled); never
  // read this byte on its own. `kGpsModeUnset` (0xFF) is what a prefs file written before
  // this existed reads as, and it means On -- what the firmware did before Power save.
  //
  // Keeping the bool in charge of on/off is deliberate: a writer that only knows the bool
  // (the app's toggle, the CLI, ui-tiny) leaves this byte alone, so toggling GPS off and
  // back on returns the badge to Power save instead of silently costing it the saving.
  uint8_t  gps_mode = offband::kGpsModeUnset;
  // The duty cycle, kept separate from gps_interval ON PURPOSE. gps_interval is a
  // reporting cadence (gps_update_interval_sec in the sensor manager); this is how long
  // Power save sleeps between acquisition attempts. Conflating them would make a
  // reporting-rate change silently alter power behaviour. 0 means "use the compiled
  // default", so an older file lands on it rather than on a zero-length sleep.
  uint32_t gps_ps_interval_s = 0;
  uint8_t autoadd_config = 0;    // bitmask for auto-add contacts config
  uint8_t rx_boosted_gain = 0; // SX126x RX boosted gain mode (0=power saving, 1=boosted)
  uint8_t radio_fem_rxgain = 0; // external LoRa FEM RX gain (LNA)
  uint8_t radio_fem_txgain = 0; // external LoRa FEM TX gain (low by default)
  uint8_t _client_repeat = 0;  // DEPRECATED -> use repeat.disable_fwd
  // #1753: 2-byte path hashes by default, from the same shared constant the other
  // roles use, so a companion and a repeater cannot end up originating at different
  // sizes. Reasoning and accepted costs are in helpers/PathHashDefault.h.
  uint8_t path_hash_mode = offband::DEFAULT_PATH_HASH_MODE;    // which path mode to use when sending
  uint8_t autoadd_max_hops = 0;  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
  uint8_t cad_enabled = 0;
  uint8_t interference_threshold = 0;
  uint8_t agc_reset_interval = 0;  // secs / 4
  char default_scope_name[31];
  uint8_t default_scope_key[16];
  int8_t tz_offset = 0;
#ifdef ENABLE_WIFI_INTERFACE
  #ifndef WIFI_SSID
    #define WIFI_SSID ""
  #endif
  char wifi_ssid[33] = {0};   // if empty, the compile-time WIFI_SSID is used
  char wifi_pwd[64] = {0};
  uint8_t wifi_enabled = 1; // enabled by default to allow wifi only builds to work. wifi won't be started if ssid is empty
  // use ssid from prefs, or fallback to ssid from build flags
  const char* getWifiSSID() const { return wifi_ssid[0] ? wifi_ssid : WIFI_SSID; }
#endif

private:
  class RadioPrefs : public CommonRadioPrefs {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("freq", _parent->freq);
      def("bw", _parent->bw);
      def("sf", _parent->sf);
      def("cr", _parent->cr);
      def("cad", _parent->cad_enabled);
      def("int_thr", _parent->interference_threshold);
      def("rxgain", _parent->rx_boosted_gain);
      def("fem_rxgain", _parent->radio_fem_rxgain);   // fem_rxgain WAS mapped to wrong JSON property previously
      def("fem_txgain", _parent->radio_fem_txgain);
      def("tx", _parent->tx_power_dbm);
      def("af", _parent->airtime_factor);
      def("rxdelay", _parent->rx_delay_base);
      def("f_txdelay", _parent->tx_delay_factor);
      def("d_txdelay", _parent->direct_tx_delay_factor);
      def("agc_int", _parent->agc_reset_interval);
      def("hash_mode", _parent->path_hash_mode);
      def("multi_ack", _parent->multi_acks);
    }
  public:
    RadioPrefs(NodePrefs* parent) : _parent(parent) { }

    // CommonRadioPrefs interface
    float getFreq() const override { return _parent->freq; }
    void setFreq(float f) override { _parent->freq = f; markDirty(); }
    float getBandwidth() const override { return _parent->bw; }
    void setBandwidth(float bw) override { _parent->bw = bw; markDirty(); }
    uint8_t getSpreadFactor() const override { return _parent->sf; }
    void setSpreadFactor(uint8_t sf) override { _parent->sf = sf; markDirty(); }
    uint8_t getCodingRate() const override { return _parent->cr; }
    void setCodingRate(uint8_t cr) override { _parent->cr = cr; markDirty(); }
    float getAirtimeFactor() const override { return _parent->airtime_factor; }
    void setAirtimeFactor(float af) override { _parent->airtime_factor = af; markDirty(); }
    bool isCadEnabled() const override { return _parent->cad_enabled; }
    void setCadEnabled(bool en) override { _parent->cad_enabled = en; markDirty(); }
    uint8_t getIntThresh() const override { return _parent->interference_threshold; }
    void setIntThresh(uint8_t t) override { _parent->interference_threshold = t; markDirty(); }
    uint8_t getRxGain() const override { return _parent->rx_boosted_gain; }
    void setRxGain(uint8_t g) override { _parent->rx_boosted_gain = g; markDirty(); }
    uint8_t getTxPower() const override { return _parent->tx_power_dbm; }
    void setTxPower(uint8_t dbm) override { _parent->tx_power_dbm = dbm; markDirty(); }
    float getRxDelay() const override { return _parent->rx_delay_base; }
    void setRxDelay(float d) override { _parent->rx_delay_base = d; markDirty(); }
    uint16_t getAgcResetInt() const override { return _parent->agc_reset_interval * 4; }
    void setAgcResetInt(uint16_t secs) override { _parent->agc_reset_interval = secs / 4; markDirty(); }
    uint8_t getHashMode() const override { return _parent->path_hash_mode; }
    void setHashMode(uint8_t m) override { _parent->path_hash_mode = m; markDirty(); }
    uint8_t getMultiAcks() const override { return _parent->multi_acks; }
    void setMultiAcks(uint8_t m) override { _parent->multi_acks = m; markDirty(); }
    float getFloodTxDelay() const override { return _parent->tx_delay_factor; }
    void setFloodTxDelay(float d) override { _parent->tx_delay_factor = d; markDirty(); }
    float getDirectTxDelay() const override { return _parent->direct_tx_delay_factor; }
    void setDirectTxDelay(float d) override { _parent->direct_tx_delay_factor = d; markDirty(); }
    uint8_t getFEMRxGain() const override { return _parent->radio_fem_rxgain; }
    void setFEMRxGain(uint8_t g) override { _parent->radio_fem_rxgain = g; markDirty(); }
    uint8_t getFEMTxGain() const override { return _parent->radio_fem_txgain; }
    void setFEMTxGain(uint8_t g) override { _parent->radio_fem_txgain = g; markDirty(); }
  };
  RadioPrefs radio;

  class GPSPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("en", _parent->gps_enabled); // boolean
      def("int", _parent->gps_interval);   // interval in seconds
      def("adv_loc", _parent->advert_loc_policy);
      def("mode", _parent->gps_mode);            // #1555: GpsMode
      def("ps_int", _parent->gps_ps_interval_s); // #1555: Power save sleep interval (s)
    }
  public:
    GPSPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  GPSPrefs gps;

  class RepeatPrefs : public ConfigSerializer {  // COPIED from CommonCLI (for now)
  public:
    uint8_t disable_fwd = 1;
  protected:
    void structure() override {
      def("disable", disable_fwd);
    }
  };
  RepeatPrefs repeat;

  class CompanionPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("auto_max", _parent->autoadd_max_hops);  // 0 = no limit, 1 = direct (0 hops), N = up to N-1 hops (max 64)
      def("defs_nm", _parent->default_scope_name, sizeof(_parent->default_scope_name));
      def("defs_key", (void *) _parent->default_scope_key, sizeof(_parent->default_scope_key));
      def("pin", _parent->ble_pin);
      def("buzz_q", _parent->buzzer_quiet);
      def("vibe_q", _parent->vibe_quiet);
      def("auto_add", _parent->autoadd_config);    // bitmask for auto-add contacts config
      def("man_add", _parent->manual_add_contacts);
      def("tel_base", _parent->telemetry_mode_base);
      def("tel_loc", _parent->telemetry_mode_loc);
      def("tel_env", _parent->telemetry_mode_env);
      def("tz_offset", _parent->tz_offset);
    }
  public:
    CompanionPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  CompanionPrefs companion;

  class UIPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("scope", _parent->notify_scope);      // #510: notify_scope (0 ALL, 1 SELF, 2 NONE)
      def("led", _parent->ui_led_mode);         // #542 B2: status/traffic LED mode
      def("disp", _parent->display_mode);       // #542 B1: OLED mode
      def("nav_sz", _parent->ui_nav_text_size); // #1244: font scale for menus/nav
      def("msg_sz", _parent->ui_msg_text_size); // #1362: font scale for messages
      def("off_s", _parent->ui_screen_secs);    // #1245: auto-blank timeout (secs)
      def("tz", _parent->ui_tz_idx);            // #1248: TimeZones.h index
      def("b_full", _parent->batt_full_mv);     // #1250: learned/pinned 100% point (mV)
      def("b_pin", _parent->batt_full_pinned);  // #1250: 1 = pinned by user, 0 = learned
      def("btn", _parent->button_actions, sizeof(_parent->button_actions));
    }
  public:
    UIPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  UIPrefs ui;

  class LogPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("caplog_en", _parent->caplog_enabled);  // #428
      def("caplog_lvl", _parent->caplog_level);   // #428
    }
  public:
    LogPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  LogPrefs log;

  DynamicConfigSerializer custom;

#ifdef ENABLE_WIFI_INTERFACE
  class WiFiPrefs : public ConfigSerializer {
    NodePrefs* _parent;
  protected:
    void structure() override {
      def("ssid", _parent->wifi_ssid, sizeof(_parent->wifi_ssid));
      def("pwd", _parent->wifi_pwd, sizeof(_parent->wifi_pwd));
      def("enabled", _parent->wifi_enabled);
    }
  public:
    WiFiPrefs(NodePrefs* parent) : _parent(parent) { }
  };
  WiFiPrefs wifi;
#endif

protected:
  void structure() override {
    def("name", node_name, sizeof(node_name));
    def("lat", node_lat);
    def("lon", node_lon);
    def("radio", radio);
    def("gps", gps);
    def("repeat", repeat);
    def("comp", companion);
    def("ui", ui);      // Offband #509/#510/#542
    def("log", log);    // Offband #428
#ifdef OFFBAND_GUI
    def("gui", gui);    // Offband #1457: the on-device GUI's settings
#endif
    def("custom", custom);
#ifdef ENABLE_WIFI_INTERFACE
    def("wifi", wifi);
#endif
  }
public:
  NodePrefs() : radio(this), gps(this), companion(this), ui(this), log(this), custom(&radio)
#ifdef ENABLE_WIFI_INTERFACE
    , wifi(this)
#endif
  {
    node_name[0] = 0;
    default_scope_name[0] = 0;
    memset(default_scope_key, 0, sizeof(default_scope_key));
    syslog_host[0] = 0;
  }
  // new accessor methods
  bool isRepeatEn() const { return repeat.disable_fwd == 0; }
  void setRepeatEn(bool en) { repeat.disable_fwd = en ? 0 : 1; }

  CommonRadioPrefs* getRadioPrefs() { return &radio; }
  KeyValueStore* getCustom() { return &custom; }

  bool isDirty() const override {
    return ConfigSerializer::isDirty() || radio.isDirty() || custom.isDirty()
#ifdef ENABLE_WIFI_INTERFACE
      || wifi.isDirty()
#endif
    ;
  }
  void clearDirty() override {
    ConfigSerializer::clearDirty();
    radio.clearDirty();
    custom.clearDirty();
#ifdef ENABLE_WIFI_INTERFACE
    wifi.clearDirty();
#endif
  }

#ifdef OFFBAND_GUI
  // #1457: the on-device GUI's settings. Unlike the groups above it holds its
  // own fields, so it is public.
  offband::gui::GuiPrefs gui;
#endif
};

// #509: button-sequence indices. Order is the press count, so the array index IS the
// sequence id on the wire.
#define OFFBAND_UI_SEQ_SINGLE  0
#define OFFBAND_UI_SEQ_DOUBLE  1
#define OFFBAND_UI_SEQ_TRIPLE  2
#define OFFBAND_UI_SEQ_QUAD    3
#define OFFBAND_UI_SEQ_COUNT   4

// #509: assignable actions. Values are wire values -- see the 0xC5 contract in
// OffbandConfigProtocol.h. Append only; never renumber.
#define OFFBAND_UI_ACTION_NONE          0
#define OFFBAND_UI_ACTION_ADVERT        1
#define OFFBAND_UI_ACTION_GPS_TOGGLE    2
#define OFFBAND_UI_ACTION_CYCLE_SCOPE   3
#define OFFBAND_UI_ACTION_BATTERY_BEEP  4
#define OFFBAND_UI_ACTION_COUNT         5

// #510: values for NodePrefs::notify_scope. Kept as an enum-like constant set rather
// than a bare magic number so the CLI, the button handler and the config command all
// agree. Order is the triple-press cycle order: ALL -> SELF -> NONE -> ALL.
#define NOTIFY_SCOPE_ALL    0
#define NOTIFY_SCOPE_SELF   1
#define NOTIFY_SCOPE_NONE   2
#define NOTIFY_SCOPE_COUNT  3

// #1468: the three decisions a scope control has to make, kept out of the screen so
// they are host-tested and so the badge and the 0xC5 handler cannot drift apart.
//
// Context for why this exists at all: notify_scope is the gate UPSTREAM of every tune
// choice -- channelMsgShouldNotify() and the DM path consult it before notify() is ever
// called -- and until now the only writer was the client. A badge left on SELF or NONE
// had no on-device route back, because the one gesture it has toggles buzzer_quiet,
// which cannot undo a scope.
namespace offband {

// The cycle the comment above has always documented: ALL -> SELF -> NONE -> ALL. A
// function rather than `(scope + 1) % COUNT` at each call site, because the order is a
// contract shared with the client's control and with #510's triple-press on ui-orig; an
// out-of-range stored value lands on ALL rather than wrapping somewhere unpredictable.
inline uint8_t nextNotifyScope(uint8_t scope) {
  if (scope >= NOTIFY_SCOPE_NONE) return NOTIFY_SCOPE_ALL;
  return (uint8_t)(scope + 1);
}

// buzzer_quiet, as the 0xC5 SET handler already sets it: silent only at NONE.
//
// This is the line that fixes the strand. The handler applied this coupling on the way
// IN and nothing applied it on the way OUT, so a client that set NONE also muted the
// buzzer, and nothing a user could reach on the badge un-muted it. Any scope control
// must apply the same rule, or it changes the gate and leaves the driver behind.
inline uint8_t quietForScope(uint8_t scope) {
  return scope == NOTIFY_SCOPE_NONE ? 1 : 0;
}

// What the badge shows. Deliberately NOT the client's per-channel words (#1469): the
// client filters one channel, this is the whole device, and the middle value is not even
// a clean middle -- at SELF a DM still sounds, only channel traffic is filtered. "DMs +
// @" is terse but it is the only short phrase that is actually true; calling it
// "Mentions" would tell a user DMs are filtered too, which they are not.
inline const char* notifyScopeLabel(uint8_t scope) {
  switch (scope) {
    case NOTIFY_SCOPE_ALL:  return "All";
    case NOTIFY_SCOPE_SELF: return "DMs + @";
    case NOTIFY_SCOPE_NONE: return "Off";
    default:                return "All";   // an unknown stored value reads as its fallback
  }
}

}  // namespace offband
