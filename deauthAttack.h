#ifndef DEAUTHATTACK_H
#define DEAUTHATTACK_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <OneButton.h>

// External button objects defined in main/inputnodes file
extern OneButton buttonUp;
extern OneButton buttonDown;
extern OneButton buttonSelect;
extern bool isInAttackMode;

#define MAX_CLIENTS 20

enum DeauthPhase {
  PHASE_NETWORK_SELECT,
  PHASE_CLIENT_SCAN,
  PHASE_CLIENT_SELECT,
  PHASE_ATTACKING
};

class deauthAttack {
public:
  deauthAttack(Adafruit_SSD1306 *displayHandle);

  // Core Methods
  static void init();
  static void scan();
  static void render();
  static void scanClients(int networkIndex);
  static void renderClients();
  static void runAttack(int clientIndex);

  // Button Handlers
  static void handleNavigateUp();
  static void handleNavigateDown();
  static void handleSelection();

private:
  static Adafruit_SSD1306 *displayVar;
  static int _networksFound;
  static int _selected;
  static int _top;
  static DeauthPhase _phase;
  static int _selectedNetwork;
  static int _clientSelected;
  static int _clientTop;
};

#endif