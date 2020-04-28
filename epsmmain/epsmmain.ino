#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <EEPROM.h>
#include "esppins.h"
#include <ESP8266WebServer.h>

/***
  EEPROM  DATA SCHEMA
  +--------------------------+---------------+
  |      Variable name       | Offset(bytes) |
  +--------------------------+---------------+
  | firstRun                 |             0 |
  | adminPass                |             1 |
  | nfcEnabled               |             9 |
  | passEnabled              |            10 |
  | pinCode                  |            11 |
  | masterCardID             |            17 |
  | userCardsCount           |            21 |
  | userCards (each 4 bytes) |            22 |
  +--------------------------+---------------+
 **/

#define FIRSTRUN 0
#define ADMINPASS 1
#define NFCENABLED 9
#define PASSENABLED 10
#define PINCODE 11
#define MASTERCARDID 19
#define USERCARDSCOUNT 23
#define USERCARDS 24

#define OUT D5
#define CARDSIZE 4
#define LOCKTIME 1000*60*5

LiquidCrystal_I2C lcd(0x27, 16, 2);          //create object for LCD class
//ESP8266WebServer server(80);            //enable server on port 80

///NFC CONFIG
//rq rst
Adafruit_PN532 nfc(D8, D7);


bool cfg[2];                    // disable pin, disable nfc,
bool MasterAccess = false;              //master access flag
uint8_t MasterID[4];                //master card ID - to prevent future reads from eeprom to save some time and power
unsigned long t1;                 //counter for wifi
unsigned long t2;                 //counter for card scanner
unsigned long retryTimer;
uint8_t tries = 0;
uint8_t uid[4], uidLen;               //card data


void blk() {                    //blink display
  lcd.noBacklight();
  delay(100);
  lcd.backlight();
}

bool arrcmp(uint8_t a[4], uint8_t b[4]) {     //compare two arrays - type uint8_t - check cards ids
  for (int i = 0; i > 4; i++)
    if (a[i] != b[i])
      return false;
  return true;
}

bool addDeleteCard(uint8_t a[4]) {

  /***

    returns true if card found and deleted
    return false if card not found and added

  ***/

  uint8_t cardsCount = EEPROM.read(USERCARDSCOUNT);
  uint8_t cards[cardsCount][4];

  for (int i = 0; i < cardsCount; i++)
    for (int j = 0; j < 4; j++)
      cards[i][j] = EEPROM.read(USERCARDS + CARDSIZE * i + j);

  for (int i = 0; i < cardsCount; i++)
    if (arrcmp(cards[i], a)) {
      cards[i][0] = cards[cardsCount][0];
      cards[i][1] = cards[cardsCount][1];
      cards[i][2] = cards[cardsCount][2];
      cards[i][3] = cards[cardsCount][3];
      EEPROM.put(USERCARDSCOUNT, cardsCount--);
      for (int j = 0; j < cardsCount - 1; j++)
        for (int k = 0; k < 4; k++)
          EEPROM.put(USERCARDS + j * CARDSIZE + k, cards[j][k]);
      EEPROM.commit();
      return true;
    }
  for (int k = 0; k < 4; k++)
    EEPROM.put(USERCARDS + CARDSIZE * (cardsCount + 1) + k, a[k]);

  EEPROM.put(USERCARDSCOUNT, cardsCount++);
  EEPROM.commit();
  return false;
}

bool findCard(uint8_t a[4]) {
  uint8_t cardsCount = EEPROM.read(USERCARDSCOUNT);
  uint8_t card[4];

  for (int i = 0; i < cardsCount; i++) {
    for (int j = 0; j < 4; j++)
      card[j] = EEPROM.read(USERCARDS + CARDSIZE * i + j);

    if (arrcmp(card, a))
      return true;
  }
  return false;
}

void clearData() {                    //perform EEPROM clear and fill with default values
  for (int i = 0; i < EEPROM.length(); i++)
    EEPROM.put(i, 0);                 //clear whole EEPROM

  EEPROM.put(0, 1);                   //first config flag
  EEPROM.put(1, 'a'); EEPROM.put(2, 'd'); EEPROM.put(3, 'm'); EEPROM.put(4, 'i'); EEPROM.put(5, 'n'); EEPROM.put(6, '\0'); //default admin password for web server
  EEPROM.put(9, 1);                 //enable nfc
  EEPROM.put(10, 1);                //enable password
  lcd.print("RESETTING...");
  EEPROM.commit();                  //write changes to EEPROM
  delay(1000);
  lcd.clear();
}

void accessGranted(){lcd.clear();lcd.print("ACCESS GRANTED"); digitalWrite(OUT,HIGH); delay(5000); digitalWrite(OUT,LOW); tries=0;}
void accessDenied(){lcd.clear();lcd.print("ACCESS DENIED"); delay(5000);tries++; }

void setup() {

  Serial.begin(9600);                 //serial for keyboard
  EEPROM.begin(512);                  //eeprom for config and data

  lcd.init();
  lcd.backlight();

  nfc.begin();
  nfc.SAMConfig();

  pinMode(D4, INPUT);
  pinMode(OUT, OUTPUT);

  if (!digitalRead(D4))
    clearData();

  if (EEPROM.read(FIRSTRUN) == 1) {
    firstRunConfig();
  }

  t1 = millis();
  t2 = millis();
}

void firstRunConfig() {
  lcd.print("Scan master card");
  //wait for card
  while (uidLen < 1) {
    nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen);
  }

  lcd.clear();
  lcd.print("Master ID:");
  lcd.setCursor(0, 1);

  for (int i = 0; i < 4; i++) {       //print master card ID
    lcd.print(uid[i]);
    EEPROM.put(MASTERCARDID + i, uid[i]); //put master id to EEPROM
    MasterID[i] = uid[i];         //and to var in ram
  }

  delay(3000); blk(); lcd.clear();

  lcd.print("Enter PIN: ");
  lcd.setCursor(0, 1);

  String password = "";
  char c;
  lcd.setCursor(0, 1);

  while (1) {
    if (Serial.available()) {
      c = Serial.read();
      if ((c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9' ) && password.length() < 9) {
        password += c;
        lcd.print(c);

      } else if (c == 'C') {
        password = password.substring(password.length() - 1);
        lcd.setCursor(0, 1);
        lcd.print("                        ");

        for (int i = 0; i < password.length(); i++)
          lcd.print(password);
        //lcd.print("X");

      } else if (c == 'O') {
        lcd.clear();
        lcd.print("Password saved");     lcd.setCursor(0, 1);
        lcd.print(password);


        EEPROM.put(PINCODE, password);
        EEPROM.put(FIRSTRUN, 0);
        break;
      }
    }
  }
  EEPROM.commit();
}

//WiFiClient client;

void loop() {
  if(tries <3){

  
  if (millis() - t1 >= 1000) {
//    client = server.available();                  //check for new request on port 80
    t1 = millis();
  }


  if (millis() - t2 >= 500)                 // check for new card
  {
    nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen);
    if (arrcmp(uid, MasterID)) {
    MasterAccess = true;
    lcd.clear();
      uid[0] = 0;
      uid[1] = 0;
      uid[2] = 0;
      uid[3] = 0;
      uidLen = 0;
    }
    t2 = millis();
         return;
  }


  if (MasterAccess) {
    lcd.clear();
    lcd.print("Scan card to ");
    lcd.setCursor(0, 1);
    lcd.print("add or delete");

    if (!arrcmp(MasterID, uid)) {
      if (addDeleteCard(uid)) {           //card deleted
        lcd.clear();
        lcd.print("Card deleted");
      } else {                  //card added
        lcd.clear();
        lcd.print("Card added");
      }
      delay(500);
    } else {                  //master card scanned - exit from masterMode
      lcd.clear();
      lcd.print("EXITING....");
      MasterAccess = false;
      delay(1000);
      lcd.clear();
      return;
    }
  } else {
    if (cfg[0]) {                 //NFC ENABLED
      if (findCard(uid))
          accessGranted();
          else
            accessDenied();
      }
  if (cfg[1]) {                 //PASSWORD ENABLED

    }
  }}
  else {
    if(
    lcd.clear();
    lcd.print("LOCKED");
    
  }
}
