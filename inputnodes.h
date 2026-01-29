#ifndef INPUTNODES_H
#define INPUTNODES_H

#define BUTTON_UP     D5
#define BUTTON_DOWN   D6
#define BUTTON_SELECT D7

#define DISPLAY_SDA D2
#define DISPLAY_SCK D1

#define OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define MENU_ITEMS_PER_PAGE 4 


extern OneButton buttonUp;
extern OneButton buttonDown;
extern OneButton buttonSelect;


#endif
