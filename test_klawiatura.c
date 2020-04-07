const byte rows = 4; 
const byte cols = 6; 
char keys[rows][cols] = {
  {'F1','7','8','9','W','C'},
  {'F2','4','5','6','X','^'},
  {'F3','1','2','3','F','v'},
  {'F4','0','.','#','OK',''}
};
byte rowPins[rows] = {2, 4, 6, 8}; //connect to the row pinouts of the keypad
byte colPins[cols] = {1, 3, 5,7,9,10}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, rows, cols );

void setup(){
  Serial.begin(9600);
}

void loop(){
  char key = keypad.getKey();

  if (key != NO_KEY){
    Serial.println(key);
  }
}
