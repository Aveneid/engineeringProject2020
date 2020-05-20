#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <EEPROM.h>
#include "esppins.h"
#include <ESP8266WebServer.h>
#include <pgmspace.h>

#include <ESP8266WiFi.h>


#include <SoftwareSerial.h>

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
  | masterCardID             |            19 |
  | userCardsCount           |            24 |
  | userCards (each 4 bytes) |            25 |
  +--------------------------+---------------+
 **/

#define FIRSTRUN 0
#define ADMINPASS 1
#define NFCENABLED 9
#define PASSENABLED 10
#define SCANNERENABLED 12
#define PINCODE 12
#define MASTERCARDID 20
#define LOCKTIME 24
#define USERCARDSCOUNT 25
#define USERCARDS 26

#define OUT D5
#define CARDSIZE 4

LiquidCrystal_I2C lcd(0x27, 16, 2);  //create object for LCD class
ESP8266WebServer server(80);       //enable server on port 80
SoftwareSerial bs(D7, 50); // RX, TX

Adafruit_PN532 nfc(D8, D6);          //rq rst




//MAIN VARIABLES
uint8_t tries = 0;
int locktime = 5;
bool cfg[3];                         // disable pin, disable nfc, disable scanner
bool MasterAccess = false;           //master access flag
uint8_t MasterID[4];                 //master card ID - to prevent future reads from eeprom to save some time and power
unsigned long t2;                    //counter for card scanner
unsigned long retryTimer;
unsigned long curMilis;
int retryTimerSeconds = 0;
//counter for invalid card / passwords
uint8_t uid[4], uidLen;              //card data
String password = "";                //password holder
char c;                              //incoming character holder
String barcode = "";


/*
    switches display background for short period of time
*/
void blk() {
  lcd.noBacklight();
  delay(100);
  lcd.backlight();
  delay(100);
}
/*
  compare two arrays - type uint8_t - check cards ids
  returns: 1 if arrays are the same
       0 if not
*/
bool arrcmp(uint8_t a[4], uint8_t b[4]) {
  for (int i = 0; i < 4; i++)
    if (a[i] != b[i])
      return false;
  return true;
}
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

/*
    performs EEPROM clear and fill with default values
*/
void clearData() {
  lcd.clear();
  for (int i = 0; i < EEPROM.length(); i++)
    EEPROM.put(i, 0);

  EEPROM.put(FIRSTRUN, 1);
  EEPROM.put(1, 'a'); EEPROM.put(2, 'd'); EEPROM.put(3, 'm'); EEPROM.put(4, 'i'); EEPROM.put(5, 'n'); EEPROM.put(6, '\0'); //default admin password for web server
  EEPROM.put(NFCENABLED, (uint8_t)1);
  EEPROM.put(PASSENABLED, (uint8_t)1);
  EEPROM.put(SCANNERENABLED, (uint8_t)1);
  EEPROM.put(LOCKTIME, (uint8_t)5);
  lcd.print("RESETTING...");
  EEPROM.commit();
  delay(1000);
  lcd.clear();
}
/*
  tries to find scanned card in memory
  returns: 1 if card found and deleted
       0 if card not found and added
*/
bool addDeleteCard(uint8_t a[4]) {

  uint8_t cardsCount = EEPROM.read(USERCARDSCOUNT);
  uint8_t cards[cardsCount][4];

  for (int i = 0; i < cardsCount; i++)
    for (int j = 0; j < 4; j++)
      cards[i][j] = EEPROM.read(USERCARDS + CARDSIZE * i + j);
  for (int i = 0; i < cardsCount; i++)
    if (arrcmp(cards[i], a)) {
      cards[i][0] = cards[cardsCount - 1][0];
      cards[i][1] = cards[cardsCount - 1][1];
      cards[i][2] = cards[cardsCount - 1][2];
      cards[i][3] = cards[cardsCount - 1][3];
      cardsCount -= 1;

      for (int j = 0; j < cardsCount; j++)
        for (int k = 0; k < 4; k++)
          EEPROM.put(USERCARDS + j * CARDSIZE + k, cards[j][k]);
      EEPROM.put(USERCARDSCOUNT, cardsCount);

      EEPROM.commit();
      delay(150);
      uidLen = 0;
      uid[0] = 0; uid[1] = 0; uid[2] = 0; uid[3] = 0;
      return true;
    }

  cardsCount++;
  for (int k = 0; k < 4; k++)
    EEPROM.put(USERCARDS + CARDSIZE * (cardsCount - 1) + k, a[k]);

  EEPROM.put(USERCARDSCOUNT, cardsCount);
  EEPROM.commit();

  delay(150);

  uidLen = 0;
  uid[0] = 0; uid[1] = 0; uid[2] = 0; uid[3] = 0;
  return false;
}

/*  checks if scanned card is known
  returns: 1 if yes
       0 if not
*/
bool findCard(uint8_t a[4]) {

  uidLen = 0;
  uint8_t cardsCount = EEPROM.read(USERCARDSCOUNT);
  uint8_t card[4];

  for (int i = 0; i < cardsCount; i++) {
    for (int j = 0; j < 4; j++)
      card[j] = EEPROM.read(USERCARDS + CARDSIZE * i + j);
    if (arrcmp(card, a)) {
      return true;
    }
  }

  return false;

}
/*
    checks if given given password is correct
    returns: 1 if yes
         0 if no
*/
bool checkPass(String pass) {
  String mainPass;
  for (int i = 0; i < 8; i++) {
    char c = EEPROM.read(PINCODE + i);
    if (c != '\0')
      mainPass += (char)EEPROM.read(PINCODE + i);
    else {
      break;
    }
  }
  mainPass = String(mainPass);
  if (mainPass == pass)
    return true;
  return false;
}

/*
    main function that grants access
*/
void accessGranted() {
  lcd.clear();
  lcd.print("ACCESS GRANTED");
  Serial.flush();
  tries = 0;
  retryTimerSeconds = 0;
  retryTimer = 0;
  password = "";
  barcode = "";
  digitalWrite(OUT, HIGH);
  delay(3000);
  digitalWrite(OUT, LOW);
  lcd.clear();
  lcd.print("ENTER PASS:");
  lcd.setCursor(0, 1);

}
/*
    main function that refuses access
*/
void accessDenied() {
  lcd.clear();
  lcd.print("ACCESS DENIED");
  password = "";
  barcode = "";
  retryTimerSeconds = 0;
  retryTimer = 0;
  Serial.flush();
  tries = tries + 1;
  uidLen = 0;
  if (tries == 3) {
    retryTimer = millis();
    lcd.clear();
    lcd.print("LOCKED");
    return;
  }
  delay(3000);
  lcd.clear();
  lcd.print("ENTER PASS:");
  lcd.setCursor(0, 1);
}
/*
    function that provides `first run config` for owner / administrator
*/

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

  lcd.setCursor(0, 1);
  while (1) {
    if (bs.available()) {
      c = bs.read();
      if ((c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9' ) && password.length() < 9) {
        password += c;
        lcd.print(c);

      } else if (c == 'C') {
        password = password.substring(0, password.length() - 1);
        lcd.setCursor(0, 1);
        lcd.print("                        ");
        lcd.setCursor(0, 1);
        // for (int i = 0; i < password.length(); i++)
        lcd.print(password);
        //lcd.print("X");

      } else if (c == 'O') {
        lcd.clear();
        lcd.print("Password saved");     lcd.setCursor(0, 1);
        lcd.print(password);
        int plen = password.length();
        for (int u = 0; u < plen ; u++) {
          EEPROM.put(PINCODE + u, password.charAt(u));
        }
        EEPROM.put(FIRSTRUN, (uint8_t)0);
        break;
      }
    }
  }
  EEPROM.commit();
  delay(1000);
  lcd.clear();
}
/*

    WEB SERVER STUFF

*/
bool auth() {

  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      return true;
    }
  }
  return false;
}

void handleNotFound() {

  String msg = "Not found\n\n";
  //  msg += server.uri;
  server.send(404, "text/plain", msg);
}

void changePass() {
  if (!auth) {
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n");
    return;
  }
  if (server.hasArg("PASS")) {
    String p = server.arg("PASS");
    if (p.length() > 0 && p.length() < 9) {
      int pl = p.length();
      for (int i = 0; i < pl; i++) {
        //Serial.println(p.substring(0, 1));
        EEPROM.put(ADMINPASS + i, p.charAt(i));

      }
      EEPROM.commit();
    }
  }
  server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
}
void changePin() {
  if (!auth) {
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n");
    return;
  }
  if (server.hasArg("PIN")) {
    String p = server.arg("PIN");
    if (p.length() > 0 && p.length() < 9) {
      int pl = p.length();
      for (int i = 0; i < pl; i++) {
        Serial.println((uint8_t)p.substring(0, 1).toInt());
        EEPROM.put(PINCODE + i, p.charAt(i));
      }
      EEPROM.commit();
    }
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
  }
}
void changeLockTime() {
  if (!auth) {
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n");
    return;
  }
  if (server.hasArg("LOCKTIME")) {
    String p = server.arg("LOCKTIME");
    // Serial.println(p);
    // Serial.println(p.length());
    if (p.length() > 0 && p.length() < 4) {
      for (int i = 0; i < p.length(); i++)
        if (!('0' <= p.charAt(i) && (int)p.charAt(i) <= '9')) {
          //  Serial.println(p.charAt(i));
          server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
          return;
        }
      // Serial.println(p);
      char buf[p.length()];
      for (int i = 0; i < p.length(); i++) buf[i] = p.charAt(i);
      //for (int i = 0;i < p.length(); i++)
      //  Serial.println(buf[i]);
      uint8_t lock = atoi(buf);
      // Serial.println(lock);
      EEPROM.put(LOCKTIME, lock);
      EEPROM.commit();
      locktime = lock;
    }
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
  }
}

void handleRoot() {
  String h = "";
  if (!auth()) {
    h = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(h);
    return;
  }
  int cardsCount = EEPROM.read(USERCARDSCOUNT);
  server.sendContent(indexHTML);
  for (int i = 0; i < cardsCount; i++) {
    String data = String(EEPROM.read(USERCARDS + i * CARDSIZE)) + ":" + String(EEPROM.read(USERCARDS + i * CARDSIZE + 1)) + ":" + String(EEPROM.read(USERCARDS + i * CARDSIZE + 2)) + ":" + String(EEPROM.read(USERCARDS + i * CARDSIZE + 3));
    h += "<tr class='table-active'><td>" + String(i) + "</td><td>" + data + "</td><td><button type='button' onClick='cD(\"" + data + "\" )' class='btn btn-sm btn-danger'>Usuń</button></td></tr>";
  }
  server.sendContent(h);
  server.sendContent(indexHTML2);
  char buf[8];
  String g = dtostrf((int)((float)cardsCount / 120 * 10 ), 2, 2, buf); if (g == "inf") g = "0";
  server.sendContent(g); server.sendContent(indexHTML3);  server.sendContent(g.substring(0, g.length() - 3)); server.sendContent(indexHTML4);
  server.sendContent(String(locktime)); server.sendContent(indexHTML5);
  h = "<input onClick='toggleNFC()' type='checkbox'"; if (cfg[0]) h += "checked"; h += "/> Włącz obsługę kart</label></div>";
  h += "<input onClick='togglePIN()' type='checkbox'"; if (cfg[1]) h += "checked"; h += "/> Włącz obsługę kodu PIN</label> ";
  h += "<input onClick='toggleScanner()' type='checkbox'"; if (cfg[2]) h += "checked"; h += "/> Włącz obsługę skanera</label> ";
  server.sendContent(h);
  server.sendContent(indexHTML6);
  server.send(200);
}
void dumpCards() {
  int cardsCount = EEPROM.read(USERCARDSCOUNT);
  Serial.print(cardsCount);
  for (int i = 0; i < cardsCount; i++) {
    Serial.print("\r\n");
    Serial.print(i); Serial.print(": \r\n");
    for (int j = 0; j < 4; j++) {
      Serial.print(EEPROM.read(USERCARDS + i * CARDSIZE + j)); Serial.print(":");
    }
  }
}
void del() {
  if (auth()) {
    if (server.hasArg("ID")) {
      // if (String(server.arg("ID")).length() == 8) {
      int cardsCount = EEPROM.read(USERCARDSCOUNT);
      uint8_t cards[cardsCount][4];

      for (int i = 0; i < cardsCount; i++)
        for (int j = 0; j < 4; j++)
          cards[i][j] = EEPROM.read(USERCARDS + i * CARDSIZE + j);

      uint8_t card[] = {0, 0, 0, 0};
      String id = server.arg("ID");
      int idx = 0;

      for (int i = 0; i < 4; i++) {
        card[i] = (uint8_t)id.substring(0, id.indexOf(':')).toInt();
        id = id.substring(id.indexOf(':') + 1, id.length());
      }
      for (int i = 0; i < cardsCount; i++) {
        if (arrcmp(cards[i], card)) {
          if (i == cardsCount - 1) {
            cardsCount -= 1;
          } else {

            cards[i][0] = cards[cardsCount - 1][0];
            cards[i][1] = cards[cardsCount - 1][1];
            cards[i][2] = cards[cardsCount - 1][2];
            cards[i][3] = cards[cardsCount - 1][3];
            cardsCount -= 1;

          }
          //Serial.print("\r\nbefore commit\r\n");
          dumpCards();
          for (int j = 0; j < cardsCount; j++)
            for (int k = 0; k < 4; k++)
              EEPROM.put(USERCARDS + j * CARDSIZE + k, cards[j][k]);
          EEPROM.put(USERCARDSCOUNT, (uint8_t)cardsCount);
          break;
        }
      }



      EEPROM.commit();
    }
    //Serial.print("\r\nafter commit\r\n");
    dumpCards();
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
  }
}
void login() {
  String hd = "";
  if (server.hasArg("LOGOUT")) {
    //hd = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    //server.sendContent(hd);
    server.sendHeader("Location", "/login");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
    server.send(301);
    return;

  } else if (server.hasArg("PASS")) {
    String pass;
    for (int i = 0; i < 8; i++) {
      char c = EEPROM.read(ADMINPASS + i);
      if (c != '\0')
        pass += c;
    }
    pass = String(pass);
    if (server.arg("PASS") == pass) {
      //hd = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendHeader("Location", "/");
      server.sendHeader("Cache-Control", "no-cache");
      server.sendHeader("Set-Cookie", "ESPSESSIONID=1");
      server.send(301);
      server.sendContent(hd);
      return;
    } else server.send(200, "text/html", "<html><body><center><b>Wrong password :v</b></center></body></html>");

  } else
    server.send(200, "text/html", "<html><body><form>Password: <input type='password' name='PASS'><input type='submit'></form></body></html>");

}
void toggleNFC() {
  if (auth()) {
    cfg[0] = !cfg[0];
    if (cfg[0])
      EEPROM.put(NFCENABLED, (uint8_t)1);
    else
      EEPROM.put(NFCENABLED, (uint8_t)0);
    server.sendContent("HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
    EEPROM.commit();
  } else server.sendContent("HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n");
}
void togglePIN() {
  if (auth()) {
    cfg[1] = !cfg[1];
    if (cfg[1])
      EEPROM.put(PASSENABLED, (uint8_t)1);
    else
      EEPROM.put(PASSENABLED, (uint8_t)0);
    server.sendContent("HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
    EEPROM.commit();
  } else server.sendContent("HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n");
}

void toggleScanner() {
  if (auth()) {
    cfg[2] = !cfg[2];
    if (cfg[2])
      EEPROM.put(SCANNERENABLED, (uint8_t)1);
    else
      EEPROM.put(SCANNERENABLED, (uint8_t)0);
    server.sendContent("HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
    EEPROM.commit();
  } else server.sendContent("HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n");
}

String h2i(String d) {
  String r = "";
  for (int i = 0; i < 4; i++) {
    r += strtol(d.substring(0, d.indexOf(':')).c_str(), 0, 16);
    d = d.substring(d.indexOf(':') + 1, d.length());
    r += ":";
  }
  return r;
}


void barcodeCheck(String code) {
  //Serial.println("barcode check: " + code);
  if (code.length() > 0) {
    code = h2i(code);

    int cardsCount = EEPROM.read(USERCARDSCOUNT);
    uint8_t cards[cardsCount][4];

    for (int i = 0; i < cardsCount; i++)
      for (int j = 0; j < 4; j++)
        cards[i][j] = EEPROM.read(USERCARDS + i * CARDSIZE + j);

    uint8_t card[] = {0, 0, 0, 0};

    for (int i = 0; i < 4; i++) {
      card[i] = (uint8_t)code.substring(0, code.indexOf(':')).toInt();
      code = code.substring(code.indexOf(':') + 1, code.length());
    }
    for (int i = 0; i < cardsCount; i++)
      if (arrcmp(cards[i], card))
        accessGranted();
    return;
  }
  accessDenied();
}




/*
    main setup function
*/
void setup() {
  Serial.begin(74880);                 //serial for keyboard
  EEPROM.begin(512);                  //eeprom for config and data
  delay(50);
  //while (!Serial);

  bs.begin(9600);
  //while (!bs);
  lcd.init();
  lcd.backlight();

  nfc.begin();
  nfc.SAMConfig();

  pinMode(D0, INPUT);
  pinMode(OUT, OUTPUT);


  if (digitalRead(D0) == 1) {
    clearData();
  }
  if (EEPROM.read(FIRSTRUN) == 1) {
    firstRunConfig();
  }
  int q = 0;
  WiFi.mode(WIFI_STA);
  WiFi.hostname("smartlock");
  WiFi.begin("TEST", "TEST1234");
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(500);
  } Serial.println("IP: " + WiFi.localIP());
  WiFi.softAPdisconnect (true);


  for (int l = 0; l < 4; l++)
    MasterID[l] = (uint8_t)EEPROM.read(MASTERCARDID + l);
  // Serial.println(MasterID[l]);

  server.on("/", handleRoot);
  server.on("/login", login);
  server.on("/delete", del);
  server.on("/changePass", changePass);
  server.on("/changePin", changePin);
  server.on("/togglePIN", togglePIN);
  server.on("/toggleNFC", toggleNFC);
  server.on("/toggleScanner", toggleScanner);
  server.on("/changeLockTime", changeLockTime);

  server.onNotFound(handleNotFound);

  const char * headerkeys[] = {
    "Cookie"
  };


  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char * );
  server.collectHeaders(headerkeys, headerkeyssize);
  server.begin();


  cfg[0] = EEPROM.read(NFCENABLED);
  cfg[1] = EEPROM.read(PASSENABLED);
  cfg[2] = EEPROM.read(SCANNERENABLED);

  locktime = EEPROM.read(LOCKTIME);
  t2 = millis();
  uidLen = 0;
  password = "";
  lcd.clear();
  lcd.print("ENTER PASS:");
  lcd.setCursor(0, 1);
  /* Serial.print("\r\n");
    for (int i = 0; i < 30; i++) {
     Serial.print(EEPROM.read(i));
     Serial.print(" ");
    }*/
  Serial.flush();
}
/*
    main loop function
    provides control for RFID reader, password entry, admin access and others
*/
void loop() {

  server.handleClient();

  if (tries < 3) {
    if (millis() - t2 >= 500 && cfg[0])                                                             // check for new card
    {
      nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen, 100);
      if (uidLen != 0) {
        if (arrcmp(uid, MasterID)) {
          if (!MasterAccess) {
            MasterAccess = true;
            lcd.clear();

            lcd.clear();
            lcd.print("Scan card to ");
            lcd.setCursor(0, 1);
            lcd.print("add or delete");
            delay(1000);

          } else {
            MasterAccess = false;
            lcd.clear();
            lcd.print("EXITING....");
            delay(1000);
            lcd.clear();
            lcd.print("ENTER PASS:");

          }
          uid[0] = 0;
          uid[1] = 0;
          uid[2] = 0;
          uid[3] = 0;
          uidLen = 0;
          return;
        }
      }
      t2 = millis();
    }
    if (MasterAccess) {                         //MASTER HAS ACCESS
      if (uidLen != 0) {
        if (!arrcmp(MasterID, uid)) {
          if (EEPROM.read(USERCARDSCOUNT) < 120) {
            if (addDeleteCard(uid)) {                       //card deleted
              lcd.clear();
              lcd.print("Card deleted");
            } else {                                                        //card added
              lcd.clear();
              lcd.print("Card added");
            }
            delay(1500);
            lcd.clear();
            lcd.print("Scan card to ");
            lcd.setCursor(0, 1);
            lcd.print("add or delete");
            return;
          }
        }
      }                                 //MASTER ACCESS ENDED
      return;
    } else {
      if (cfg[0]) {                                                      //NFC ENABLED
        if (uidLen != 0) {
          if (findCard(uid))
            accessGranted();
          else
            accessDenied();
        }
      }
      if (cfg[1]) {                                                       //PASSWORD ENABLED
        if (bs.available()) {
          c = bs.read();
          if ((c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9' ) && password.length() < 9) {
            password += c;
            lcd.print("X");
          }
          else if (c == 'C') {
            password = password.substring(0, password.length() - 1);
            lcd.setCursor(0, 1);
            lcd.print("                        ");
            lcd.setCursor(0, 1);
            for (int i = 0; i < password.length(); i++)
              lcd.print('X');
          } else if (c == 'O') {
            if (checkPass(password))
              accessGranted();
            else
              accessDenied();
          }
        }
      }
      if (cfg[2]) {                                                 //SCANNER ENABLED
        if (Serial.available() > 0) {
          char bc = Serial.read();
          if (bc == '\r') {
            barcodeCheck(barcode);
          }
          else
            barcode += bc;
          // Serial.flush();
        }
      }
    }
  } else {                            //TOO MANY TRIES - WAITING  `LOCKTIME`
    curMilis = millis();
    if (curMilis - retryTimer  >= 1000) {
      retryTimerSeconds++;
      retryTimer = millis();
    }
    //  Serial.println(locktime * 60);
    if (retryTimerSeconds >= locktime * 60) {
      tries = 0;
      retryTimer = 0;
      retryTimerSeconds = 0;
      lcd.clear(); lcd.print("ENTER PASS:");
    }
    //delay(700);
  }
  uidLen = 0;
  uid[0] = 0; uid[1] = 0; uid[2] = 0; uid[3] = 0;
}
//#endif
