#include <Adafruit_PN532.h>
#include <EEPROM.h>   //eeprom do przechowywania dancych jak kod dostepu i znane karty nfc
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  //do obslugi wyswietlacza
#include "esppins.h"

#define OUTPIN D5


LiquidCrystal_I2C lcd(0x27, 16, 2);
//rq rst
Adafruit_PN532 nfc(D8, D7);
bool firstRun = true;

bool masterCardAccess = false;

uint8_t success = 0;
uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0, 0};
uint8_t uidLen;

int passAddr = sizeof(bool) + 4 * sizeof(uint8_t);


char passW[6];
int passIndex = 0;

void setup() {
  EEPROM.begin(512);
  Serial.begin(9600);
  nfc.begin();
  nfc.SAMConfig();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  pinMode(OUTPIN, OUTPUT);


  //EEPROM.get(0,firstRun);
  if (firstRun)
  {
    lcd.print("First run config");
    lcd.setCursor(0, 1);
    masterCard();
    delay(3000);
    lcd.clear();
    initPassword();
    //firstRun = false;
  }

}
void masterCard() {
  lcd.print("Scan master card");

  while (uidLen < 1) {
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLen);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card scanned");
  lcd.setCursor(0, 1);
  lcd.print("ID:");
  for (uint8_t i = 0; i < uidLen; i++)
  {
    lcd.print(uid[i]);
    EEPROM.put(sizeof(bool) + i * sizeof(uint8_t), uid[i]);
  }
  delay(1000);
  lcd.clear();/*
  for (uint8_t i = 0; i < uidLen; i++)
    lcd.print((uint8_t)EEPROM.read(sizeof(bool) + i * sizeof(uint8_t)));*/
  memset(uid, 0, sizeof(uid));
  uidLen = 0;

}
void initPassword() {
  char c;
  char password[4];
  short passIndex = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter pass: ");
  lcd.setCursor(0, 1);
  while (1) {
    if (passIndex < 4) {
      if (Serial.available()) {
        c = Serial.read();
        switch (c) {
          case '0': password[passIndex] = '0';  passIndex++; lcd.print("X"); break;
          case '1': password[passIndex] = '1';  passIndex++; lcd.print("X"); break;
          case '2': password[passIndex] = '2';  passIndex++; lcd.print("X"); break;
          case '3': password[passIndex] = '3';  passIndex++; lcd.print("X"); break;
          case '4': password[passIndex] = '4';  passIndex++; lcd.print("X"); break;
          case '5': password[passIndex] = '5';  passIndex++; lcd.print("X"); break;
          case '6': password[passIndex] = '6';  passIndex++; lcd.print("X"); break;
          case '7': password[passIndex] = '7';  passIndex++; lcd.print("X"); break;
          case '8': password[passIndex] = '8';  passIndex++; lcd.print("X"); break;
          case '9': password[passIndex] = '9';  passIndex++; lcd.print("X"); break;
          case 'C': passIndex--; lcd.setCursor(passIndex, 1); lcd.print(" "); lcd.setCursor(passIndex, 1); break;
          default: break;
        }
      }
    } else break;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Password is:"); lcd.setCursor(0, 1);
  for (int i = 0; i < 4; i++) {
    lcd.print(password[i]);
    EEPROM.put(passAddr + i * sizeof(char), password[i]);
  }
  delay(3000);
  lcd.clear();
}
void accessGranted() {
  lcd.clear();
  lcd.print("ACCESS GRANTED");

  passIndex = 0;
  memset(uid, 0, sizeof(uid));
  delay(3000);
  lcd.clear();
}
void accessDenied() {
  lcd.clear();
  lcd.print("Access denied");

  passIndex = 0;
  memset(uid, 0, sizeof(uid));
  delay(3000);
  lcd.clear();
}
int checkPass(char p[4]) {
  for (int i = 0; i < 4; i++)
    if (p[i] != EEPROM.read(passAddr + i * sizeof(char)))
      return 0;
  return 1;
}
int checkCard(uint8_t c[4]) {
  for (int i = 0; i < 4; i++)
    if (c[i] != EEPROM.read(sizeof(bool) + i * sizeof(uint8_t)))
      return 0;
  return 1;
}

void loop() {
  //lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter pass:");
  lcd.setCursor(0, 1);

  for (int i = 0; i < passIndex; i++) lcd.print("X");
  if (Serial.available()) {
    char c = Serial.read();
    if (c != 'O') {
      if (passIndex < 6)
        switch (c) {
          case '0': passW[passIndex] = '0';  passIndex++; lcd.print("X"); break;
          case '1': passW[passIndex] = '1';  passIndex++; lcd.print("X"); break;
          case '2': passW[passIndex] = '2';  passIndex++; lcd.print("X"); break;
          case '3': passW[passIndex] = '3';  passIndex++; lcd.print("X"); break;
          case '4': passW[passIndex] = '4';  passIndex++; lcd.print("X"); break;
          case '5': passW[passIndex] = '5';  passIndex++; lcd.print("X"); break;
          case '6': passW[passIndex] = '6';  passIndex++; lcd.print("X"); break;
          case '7': passW[passIndex] = '7';  passIndex++; lcd.print("X"); break;
          case '8': passW[passIndex] = '8';  passIndex++; lcd.print("X"); break;
          case '9': passW[passIndex] = '9';  passIndex++; lcd.print("X"); break;
          case 'C': passIndex--; lcd.setCursor(passIndex, 1); lcd.print(" "); lcd.setCursor(passIndex, 1); break;
        }
    } else {
      if (checkPass(passW))
        accessGranted();
      else
        accessDenied();
    }
  } else {
    /*
      success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLen);
      if (uidLen > 0)
      if (checkCard(uid))
        accessGranted();
      else
        accessDenied();
    */
  }


}
