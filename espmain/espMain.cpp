/** 
 *  @file   espMain.cpp 
 *  @brief  Main NodeMcu file \n
 *  Takes care of validating data entered by user \n
 *  Stores cards data and passwords \n
 *  Serves administrator panel via http over Wi-Fi \n
 *  
 *  @author Bartlomiej Ferenc, Tomasz Kaczmarczyk
 *  @date   2020-03-31
 *  Copyright (c) 2020 Bartlomiej Ferenc (github.com/Aveneid) & Tomasz Kaczmarczyk (github.com/Egeneza151)
 ***********************************************/


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



///Address of first run flag stored in EEPROM
#define FIRSTRUN 0
///Address of admin password address stored in EEPROM
#define ADMINPASS 1
///Address of NFC enabled fla, stored in EEPROM
#define NFCENABLED 9
///Address of password enabled flag stored in EEPROM
#define PASSENABLED 10
///Address of scanner enabled flag stored in EEPROM
#define SCANNERENABLED 12
///Address of pin code value stored in EEPROM
#define PINCODE 12
///Address of Master card ID stored in EEPROM
#define MASTERCARDID 20
///Address of lock time in minutes stored in EEPROM
#define LOCKTIME 24
///Address of count of reconized user cards stored in EEPROM
#define USERCARDSCOUNT 25
///Address of beginning of user cards data stored in EEPROM
#define USERCARDS 26
///Pin definition, will be pulled HIGH if access is granted
#define OUT D5
///Size of card in bytes
#define CARDSIZE 4

/**
	*  \brief Instance of LCD class, used to drive LCD display
 *  
 *  \param [in] 0x27 Address of LCD in I2C bus
 *  \param [in] 16 Number of columns
 *  \param [in] 2 number of lines
 */
LiquidCrystal_I2C lcd(0x27, 16, 2);
/**
	*  \brief Instance of ESP8266WebServer class, used for administrator panel
 *  
 *  \param [in] 80 Port for webserver
 *  \return Return description
 *  
 *  
 */
ESP8266WebServer server(80);
/**
	*  \brief Instance of SoftwareSerial class, used for keyboard communication
 *  
 *  \param [in] D7 Receive pin
 *  \param [in] 50 Transmit pin (not used)
 */
 
SoftwareSerial bs(D7, 50);
/**
	*  \brief Instance of Adafruit_PN532 class, used for driving RFID module
 *  
 *  \param [in] D8 RQ pin
 *  \param [in] D6 RST pin

 */
Adafruit_PN532 nfc(D8, D6);




///Counter for tries, if entered data is incorrect, it will increment by 1
uint8_t tries = 0;
///Current lock time (in minutes)
int locktime = 5;

///Feature flags, each one is for NFC, password and barcode scanner respectively
bool cfg[3];
///Flag for master access, filps when master card was readed
bool MasterAccess = false;

///Master card ID - to prevent future reads from EEPROM to save some time and power
uint8_t MasterID[4];

///Counter for card scanner
unsigned long t2;

///Variable for timing
unsigned long retryTimer;
///Variable for timing
unsigned long curMilis;
///Counter that count how many seconds pass when lock triggered
int retryTimerSeconds = 0;


///Card data - used when new card is present and scanned
uint8_t uid[4], uidLen;
///Password holder
String password = "";
///Incoming character holder
char c;
///Barcode holder - used when new barcode was readed
String barcode = "";


/**
   \brief  Switches display background for short period of time
**/
void blk() {
  lcd.noBacklight();
  delay(100);
  lcd.backlight();
  delay(100);
}


/**
	*  \brief  Compares two arrays of uint8_t type
 *  \param [in] a first array
 *  \param [in] b second array
 *  \return true if arrays are the same
 *  \return false if arrays are different
 *  
 */
bool arrcmp(uint8_t a[4], uint8_t b[4]) {
  for (int i = 0; i < 4; i++)
    if (a[i] != b[i])
      return false;
  return true;
}

/**
    \brief Performs EEPROM clear and fill with default values
**/
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
/**
  \brief Tries to find given as parameter card in EEPROM\n 
  \param a array - card data - to add or delete
  \returns  true if card found and deleted \n 
       false if card not found and added
**/
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

/**
*  \brief Checks if scanned card is known\n 
 *  
 *  \return true if known\n
 *  \return false if not
 *  
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

/**
	  \brief Checks if given given password is correct\n 
 *  
 *  \param [in] pass Password to be checked
 *  \return true if is valid
 *  \return false if is not valid
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

/**
    \brief Function that grants access to protected zone when called
**/
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
/**
   \brief  Function that refuses access  to protected zone when called
**/
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
/**
   \brief  Function that provides `first run config` for owner / administrator \n 
	Triggered by hardware button
**/

void firstRunConfig() {

  lcd.print("Scan master card");
  //wait for card
  while (uidLen < 1) {
    nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen);
  }

  lcd.clear();
  lcd.print("Master ID:");
  lcd.setCursor(0, 1);

  //print master card ID
  for (int i = 0; i < 4; i++) {
    lcd.print(uid[i]);
    //put master id to EEPROM
    EEPROM.put(MASTERCARDID + i, uid[i]);
    //and to var in ram
    MasterID[i] = uid[i];
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

        lcd.print(password);


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
/**
    \brief Function that handles user authentication via WWW \n 
	\returns true if specific cookie is set \n
	 \returns false if not
**/
bool auth() {

  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      return true;
    }
  }
  return false;
}
/**
  \brief Function that handles 404 error on server, applies when someone want to acces file (or path) that is not found
 */
void handleNotFound() {

  String msg = "Not found\n\n";
  server.send(404, "text/plain", msg);
}

/**
  \brief Function that handles admin password change for administrator panel via WWW, works only if user is admin
 **/
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
        EEPROM.put(ADMINPASS + i, p.charAt(i));

      }
      EEPROM.commit();
    }
  }
  server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
}
/**
  \brief Function that changes access pin code, works only if user is admin  
 **/
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
/**
	\brief Function that changes lock time(in minutes), works only if user is admin
 **/
void changeLockTime() {
  if (!auth) {
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n");
    return;
  }
  if (server.hasArg("LOCKTIME")) {
    String p = server.arg("LOCKTIME");
    if (p.length() > 0 && p.length() < 4) {
      for (int i = 0; i < p.length(); i++)
        if (!('0' <= p.charAt(i) && (int)p.charAt(i) <= '9')) {
          server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
          return;
        }
      char buf[p.length()];
      for (int i = 0; i < p.length(); i++) buf[i] = p.charAt(i);
      uint8_t lock = atoi(buf);
      EEPROM.put(LOCKTIME, lock);
      EEPROM.commit();
      locktime = lock;
    }
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
  }
}
/**
  \brief Function that handles main index file via WWW, prompts for password, if correct will redirect to admin panel  
 **/
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
/**
  \brief Function that dumps all saved cards to Serial port
 **/
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
/**
  \brief Function that delete card from storage if card is present, works only if user is admin
 **/
void del() {
  if (auth()) {
    if (server.hasArg("ID")) {
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
    dumpCards();
    server.sendContent("HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
  }
}
/** 
  \brief Function that handles admin panel login\n 
  Redirects to admin panel if login was successful
 **/
void login() {
  String hd = "";
  if (server.hasArg("LOGOUT")) {
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
/**
  \brief Function that toggles NFC from http request, works only if user is admin 
 **/
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
/**
  \brief Function that toggles pin code from http request, works only if user is admin 
 **/
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
/** 
  \brief Funciton that toggles scanner from http request, works only if user is admin
 **/
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

/**
*  \brief Function that converts string in HEX format\n 
  for example: AC:3D:FF:A0 to its DEC format\n 
  like: 172:61:255:160 
 *  
 *  \param [in] d valid HEX string
 *  \return integer representation of HEX values in string
 */
String h2i(String d) {
  String r = "";
  for (int i = 0; i < 4; i++) {
    r += strtol(d.substring(0, d.indexOf(':')).c_str(), 0, 16);
    d = d.substring(d.indexOf(':') + 1, d.length());
    r += ":";
  }
  return r;
}

/**
 *  \brief  Function that handles barcode scanner code check \n 
 *  Grants or denies access to protected zone regarding to arrcmp() return value
 *  
 *  \param [in] code String in format aa:bb:cc:dd where aa,bb,cc,dd in range of 00-FF
 */
void barcodeCheck(String code) {
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




/**
*   \brief  Main setup function\n 
*	Sets parameters for class objects, connects to Wi-Fi\n 
*	Reads data from EEPROM to RAM
**/
void setup() {
  //serial for barcode scanner
  Serial.begin(74880);
  //eeprom for config and data
  EEPROM.begin(512);
  delay(50);
  //serial for keyboard
  bs.begin(9600);

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
  // Wait for the Wi-Fi to connect
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  } Serial.println("IP: " + WiFi.localIP());
  WiFi.softAPdisconnect (true);


  for (int l = 0; l < 4; l++)
    MasterID[l] = (uint8_t)EEPROM.read(MASTERCARDID + l);


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

  Serial.flush();
}
/**
   \brief Main loop function\n 
   Provides control for RFID reader \n 
   password entry, admin access and others
**/
void loop() {

  server.handleClient();

  if (tries < 3) {
    // check for new card
    if (millis() - t2 >= 500 && cfg[0])
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
    //MASTER HAS ACCESS
    if (MasterAccess) {
      if (uidLen != 0) {
        if (!arrcmp(MasterID, uid)) {
          if (EEPROM.read(USERCARDSCOUNT) < 120) {
            if (addDeleteCard(uid)) {
              //card deleted
              lcd.clear();
              lcd.print("Card deleted");

            } else {
              //card added
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
      }
      return;
      
    } else {
      //NFC ENABLED
      if (cfg[0]) {
        if (uidLen != 0) {
          if (findCard(uid))
            accessGranted();
          else
            accessDenied();
        }
      }
      //PASSWORD ENABLED 
      if (cfg[1]) {
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
      //SCANNER ENABLED
      if (cfg[2]) {
        if (Serial.available() > 0) {
          char bc = Serial.read();
          if (bc == '\r') {
            barcodeCheck(barcode);
          }
          else
            barcode += bc;
        }
      }
    }
  } else
    //TOO MANY TRIES - WAITING  `LOCKTIME`
  {
    curMilis = millis();
    if (curMilis - retryTimer  >= 1000) {
      retryTimerSeconds++;
      retryTimer = millis();
    }
    if (retryTimerSeconds >= locktime * 60) {
      tries = 0;
      retryTimer = 0;
      retryTimerSeconds = 0;
      lcd.clear(); lcd.print("ENTER PASS:");
    }
  }
  uidLen = 0;
  uid[0] = 0; uid[1] = 0; uid[2] = 0; uid[3] = 0;
}
