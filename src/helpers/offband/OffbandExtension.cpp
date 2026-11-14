#include "OffbandExtension.h"

#if defined(OFFBAND_OBSERVER) || defined(OFFBAND_WEB_PORTAL)

#include "../../examples/companion_radio/MyMesh.h"
#include <helpers/wifi/MinimalWebPortal.h>
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

#if defined(ESP32) && (defined(OFFBAND_WEB_PORTAL) || defined(OFFBAND_OBSERVER))
  const char* node_name = mesh_ ? mesh_->getNodeName() : "MeshCore";
  MinimalWebPortal::begin(board_, node_name, "Observer", nullptr);
#endif
}

void OffbandExtension::onWifiDisconnected() {
  wifi_connected_ = false;
}

void OffbandExtension::loop() {
#if defined(OFFBAND_OBSERVER)
  wifiObserverLoop();
#endif
}

} // namespace offband

#endif
