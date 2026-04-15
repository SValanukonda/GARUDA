#include <Arduino.h>
#include <OneButton.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Project Header Files
#include "inputnodes.h"
#include "beaconSpam.h"
#include "deauthAttack.h"

// Menu Configuration
const char* menuItems[] = { "Beacon Attack", "Deauth Attack" };
const int totalItems = sizeof(menuItems) / sizeof(menuItems[0]);

// Display Initialization
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Button Initialization (Active Low = true)
OneButton buttonUp(BUTTON_UP, true);
OneButton buttonDown(BUTTON_DOWN, true);
OneButton buttonSelect(BUTTON_SELECT, true);

// Navigation Variables
int currentIndex = 0;    
int topIndex = 0;        
bool isInAttackMode; // Shared with class files

// Class Pointers
beaconSpam* spammer = nullptr;
deauthAttack* attacker = nullptr;

// --- ISR Handlers for Buttons ---
void IRAM_ATTR checkUpTicks()   { buttonUp.tick(); }
void IRAM_ATTR checkDownTicks() { buttonDown.tick(); }
void IRAM_ATTR checkSelTicks()  { buttonSelect.tick(); }

// --- Function Prototypes ---
void drawMenu();
void setupMainMenuButtons();

/**
 * Exits any active attack and returns to the GARUDA main menu
 */
void exitAttackMode() {
    Serial.println(F("Stopping Attack... Exiting to Main Menu"));
    isInAttackMode = false; 
    
    // Free memory from the heap
    if (spammer) { delete spammer; spammer = nullptr; }
    if (attacker) { delete attacker; attacker = nullptr; }

    // Reconfigure buttons for main menu navigation
    setupMainMenuButtons();
    drawMenu();
}

/**
 * Action for Item 0: Beacon Spam
 */
void actionItem1() {
    Serial.println(F("Starting Beacon Attack Sequence..."));
    isInAttackMode = true; 

    // Disable menu clicks
    buttonUp.attachClick(NULL);
    buttonDown.attachClick(NULL);
    
    // Set long press to exit
    buttonSelect.setPressTicks(800);
    buttonSelect.attachLongPressStart(exitAttackMode);

    if (spammer == nullptr) {
        spammer = new beaconSpam(&display);
    }
    
    spammer->initDevice();
    spammer->executeScan();
}

/**
 * Action for Item 1: Deauth Attack
 */
void actionItem2() {
    Serial.println(F("Starting Deauth Attack Sequence..."));
    // NOTE: Do NOT set isInAttackMode here.
    // It must remain false so handleSelection() can trigger runAttack().
    // runAttack() itself will set isInAttackMode = true before the loop.

    // Disable menu clicks
    buttonUp.attachClick(NULL);
    buttonDown.attachClick(NULL);
    
    // Set long press to exit
    buttonSelect.setPressTicks(800);
    buttonSelect.attachLongPressStart(exitAttackMode);

    if (attacker == nullptr) {
        attacker = new deauthAttack(&display);
    }
    
    attacker->init(); // Sets WiFi to Injection Mode
    attacker->scan(); // Starts Scan -> Menu -> Attack flow
}

/**
 * Directs the menu selection to the correct function
 */
void performAction(int index) {
    if(index == 0) actionItem1();
    else if(index == 1) actionItem2();
    else Serial.println(F("Not Implemented"));
}

/**
 * Renders the GARUDA Main Menu
 */
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

// --- Navigation Handlers ---
void upPressed() {
    if (currentIndex > 0) { 
        currentIndex--; 
        if (currentIndex < topIndex) topIndex--; 
    }
    drawMenu();
}

void downPressed() {
    if (currentIndex < totalItems - 1) { 
        currentIndex++; 
        if (currentIndex >= topIndex + 4) topIndex++; 
    }
    drawMenu();
}

void setupMainMenuButtons() {
    buttonSelect.setPressTicks(800);
    buttonUp.attachClick(upPressed);
    buttonDown.attachClick(downPressed);
    buttonSelect.attachClick([](){ performAction(currentIndex); });
    buttonSelect.attachLongPressStart(NULL); 
}

// --- Core Arduino Functions ---
void setup() {
    Serial.begin(115200);
    
    // I2C Display Setup
    Wire.begin(DISPLAY_SDA, DISPLAY_SCK);
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    // Hardware Pins
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);

    // Attach Interrupts for responsive UI
    attachInterrupt(digitalPinToInterrupt(BUTTON_UP), checkUpTicks, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN), checkDownTicks, CHANGE);
    attachInterrupt(digitalPinToInterrupt(BUTTON_SELECT), checkSelTicks, CHANGE);

    setupMainMenuButtons();
    drawMenu();
    Serial.println(F("System Ready."));
}

void loop() {
    // Keep OneButton state machine running
    buttonUp.tick();
    buttonDown.tick();
    buttonSelect.tick();
    
    yield(); 
}