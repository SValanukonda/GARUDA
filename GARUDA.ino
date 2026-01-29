#include <Arduino.h>
#include <OneButton.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "inputnodes.h"
#include "beaconSpam.h"

// --- Global Objects ---
const char* menuItems[] = { "Beacon Attack", "Deauth Attack" };
const int totalItems = sizeof(menuItems) / sizeof(menuItems[0]);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

OneButton buttonUp(BUTTON_UP, true);
OneButton buttonDown(BUTTON_DOWN, true);
OneButton buttonSelect(BUTTON_SELECT, true);

// --- State Management ---
int currentIndex = 0;    
int topIndex = 0;        
bool isInAttackMode = false; 

beaconSpam* spammer = nullptr;

// --- 1. Interrupt Service Routines (ISRs) ---
// These MUST have IRAM_ATTR to run from RAM for speed/stability
void IRAM_ATTR checkUpTicks()   { buttonUp.tick(); }
void IRAM_ATTR checkDownTicks() { buttonDown.tick(); }
void IRAM_ATTR checkSelTicks()  { buttonSelect.tick(); }

// --- Forward Declarations ---
void drawMenu();
void setupMainMenuButtons();

// ------------------ Actions ------------------

void exitAttackMode() {
    Serial.println(F("Exiting to Main Menu..."));
    isInAttackMode = false; 
    setupMainMenuButtons();
    drawMenu();
}

void actionItem1() {
    Serial.println(F("Starting Beacon Attack..."));
    isInAttackMode = true; 

    // Reassign buttons for Attack Mode
    buttonUp.attachClick(NULL);
    buttonDown.attachClick(NULL);
    buttonSelect.attachClick(NULL);
    
    buttonSelect.setPressTicks(800);
    buttonSelect.attachLongPressStart(exitAttackMode);

    if (spammer == nullptr) {
        spammer = new beaconSpam(&display);
    }
    
    spammer->initDevice();
    spammer->executeScan();
}

void performAction(int index) {
  if(index == 0) actionItem1();
  else Serial.println(F("Not Implemented"));
}

// ------------------ UI Drawing ------------------

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  const char* header = "GARUDA";
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(header, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w)/2, 0);
  display.println(header);

  display.setTextSize(1);
  for (int i = 0; i < 4; i++) {
    int itemIndex = topIndex + i;
    if (itemIndex >= totalItems) break;
    display.setCursor(0, 24 + i*12);
    display.print(itemIndex == currentIndex ? "> " : "  ");
    display.print(itemIndex + 1);
    display.print(".");
    display.println(menuItems[itemIndex]);
  }
  display.display();
}

// ------------------ Menu Button Handlers ------------------

void upPressed() {
  if (currentIndex > 0) { currentIndex--; if (currentIndex < topIndex) topIndex--; }
  drawMenu();
}

void downPressed() {
  if (currentIndex < totalItems - 1) { currentIndex++; if (currentIndex >= topIndex + 4) topIndex++; }
  drawMenu();
}

void setupMainMenuButtons() {
    buttonUp.attachClick(upPressed);
    buttonDown.attachClick(downPressed);
    buttonSelect.attachClick([](){ performAction(currentIndex); });
    buttonSelect.attachLongPressStart(NULL); 
}

void setup() {
    Serial.begin(115200);
    Wire.begin(DISPLAY_SDA, DISPLAY_SCK);
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

    // --- 2. Initialize Interrupts ---
    // We attach the interrupt to the CHANGE mode so it catches both press and release
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(BUTTON_UP), checkUpTicks, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN), checkDownTicks, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT), checkSelTicks, CHANGE);

    setupMainMenuButtons();
    drawMenu();
}

void loop() {
    // We still keep these here to handle the internal timing/callback logic
    // but the interrupts will make sure transitions are caught instantly.
    buttonUp.tick();
    buttonDown.tick();
    buttonSelect.tick();
    yield(); // Keep WiFi stack happy
}