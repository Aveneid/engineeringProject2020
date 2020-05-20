#include <SoftwareSerial.h>
#include <PS2Keyboard.h>
#include <Keypad.h>

#define ROWS 4
#define COLS 5

char keys[ROWS][COLS] = {
  {'7', '8', '9', '&', 'C'},
  {'4', '5', '6', 'X', '^'},
  {'1', '2', '3', 'F', 'v'},
  {'0', '.', 'Q', 'O', '/'}
};
byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {12, 11, 10, 9, 8};

//create keypad instance
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// RX, TX for software serial
SoftwareSerial bs(50, 4);

PS2Keyboard keyboard;

void setup() {
  //start SoftwareSerial for keyboard
  bs.begin(9600);
  //start Serial for barcode scanner
  Serial.begin(74880);
  //start PS2keyboard for barcode scanner
  scanner.begin(2, 3);
}
int cn = 0;
void loop() {
  //Check if scanner is available
  if (scanner.available()) {
    //get character from scanner
    char c = scanner.read();
    if (c != NULL) {

      if (c == PS2_ENTER) {
        Serial.println();
      } else if (c == PS2_TAB || c == PS2_ESC || c == PS2_PAGEDOWN ||
                 c == PS2_PAGEUP || c == PS2_LEFTARROW || c == PS2_LEFTARROW ||
                 c ==  PS2_UPARROW || c == PS2_DOWNARROW || c == PS2_DELETE);
      else {
        if (c != ':')
          Serial.write(c);
        else {
          cn++;
          Serial.print(c);
        }
        if (cn == 3) {
          Serial.println();
          cn = 0;
        }
      }
    }
  }
  //get key from Keypad
  char key = keypad.getKey();
  if (key != NO_KEY) {
    bs.print(key);
  }
  //wait some time
  delay(10);
}
