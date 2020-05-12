#include <Keypad.h>
#define ROWS 4 
#define COLS 6 

char keys[ROWS][COLS] = {
  {'!','7','8','9','&','C'},
  {'@','4','5','6','X','^'},
  {'#','1','2','3','F','v'},
  {'$','0','.','Q','O','/'}
};
byte rowPins[ROWS] = {5, 4, 3, 2};
byte colPins[COLS] = {11,10,9, 8, 7, 6}; 

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS); 

void setup(){
  Serial.begin(74880);
}
void loop(){
  char key = keypad.getKey();
  if(key != NO_KEY){
    Serial.print(key);
  delay(100);
  }
}
