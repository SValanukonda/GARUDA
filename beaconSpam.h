#ifndef BEACONSPAM_H
#define BEACONSPAM_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <OneButton.h>

extern OneButton buttonUp;
extern OneButton buttonDown;
extern OneButton buttonSelect;

class beaconSpam {
  public:
    beaconSpam(Adafruit_SSD1306* displayHandle);
    
    void initDevice();          
    void executeScan();         
    
    // Now this is static so handleNavigate can call it directly
    static void renderInterface();     
    
    static void handleNavigateUp();    
    static void handleNavigateDown();  
    static void handleSelection();     
    
    void runBeaconAttack(String targetSSID, int channel);

    static Adafruit_SSD1306* displayVar;
    static int _networksFound;
    static int _currentSelected;
    static int _TopStart;
};

#endif