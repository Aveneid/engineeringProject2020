 # include < WiFi.h >
 # include < LiquidCrystal_I2C.h >
 # include < Wire.h >
 # include < Adafruit_PN532.h > 
 # include < EEPROM.h >
 # include "esppins.h"
 # include < ESP8266WebServer.h >

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

 # define FIRSTRUN 0
 # define ADMINPASS 1
 # define NFCENABLED 9
 # define PASSENABLED 10
 # define PINCODE 11
 # define MASTERCARDID 19
 # define USERCARDSCOUNT 23
 # define USERCARDS 24

 # define OUT D5
 # define CARDSIZE 4

LiquidCrystal_I2C lcd(0x27, 16, 2);					//create object for LCD class
ESP8266WebServer server(80);						//enable server on port 80

///NFC CONFIG
//rq rst
Adafruit_PN532 nfc(D8, D7);


bool cfg[2]; 										// disable pin, disable nfc,
bool MasterAccess = false;							//master access flag
uint8_t MasterID[4];								//master card ID - to prevent future reads from eeprom to save some time and power
unsigned long t1;									//counter for wifi 
unsigned long t2;									//counter for card scanner

void blk() {										//blink display
    lcd.noBacklight();
    delay(100);
    lcd.backlight();
}

bool arrcmp(uint8_t a[4], uint8_t b[4]) {			//compare two arrays - type uint8_t - check cards ids
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
        for (int j = 0; i < 4; j++)
            cards[i][j] = EEPROM.read(USERCARDS + CARDSIZE * i + j);

    for (int i = 0; i < cardsCount; i++)
        if (arrcmp(cards[i], a)) {
            cards[i][0] = cards[cardsCount][0];
            cards[i][1] = cards[cardsCount][1];
            cards[i][2] = cards[cardsCount][2];
            cards[i][3] = cards[cardsCount][3];
            EEPROM.put(USERCARDSCOUNT, cardsCount--);
            for (int j = 0; j < cardsCount; j++)
                for (int k = 0; k < 4; k++)
                    EEPROM.put(USERCARDS + j * CARDSIZE + k, cards[j][k]);
            EEPROM.commit();
            return true;
        }
    for (int k = 0; k < 4; k++)
        EEPROM.put(USERCARDS + CARDSIZE * cardsCount + k, a[k]);
    EEPROM.put(USERCARDSCOUNT, cardsCount++);
    EEPROM.commit();
    return false;
}

void clearData() {										//perform EEPROM clean - fill with default values
    for (int i = 0; i < EEPROM.length(); i++)
        EEPROM.put(i, 0);
    
	EEPROM.put(0, 1);   								//first config flag
	
	EEPROM.put(1, 'a');EEPROM.put(2, 'd');EEPROM.put(3, 'm');EEPROM.put(4, 'i');EEPROM.put(5, 'n'); //default admin password for web server 
	
	EEPROM.put(9,1);									//enable nfc
	EEPROM.put(10,1);									//enable password
	
    lcd.print("RESETTING...");
    EEPROM.commit();									//write changes to EEPROM
	delay(1000);
    lcd.clear();
}

void setup() {

    Serial.begin(9600); 								//serial for keyboard
    EEPROM.begin(512); 									//eeprom for config and data

    lcd.init();
    lcd.backlight();

    nfc.begin();
    nfc.SAMConfig();

    pinMode(D4, INPUT);
    pinMode(OUT, OUTPUT);
	
    if (!digitalRead(D4))
        clearData();

    if(EEPROM.read(0) == 1){
    firstRunConfig();
    }
    
	onesecond = millis();
}

void firstRunConfig() {
    lcd.print("Scan master card");
    uint8_t uidLen = 0,
    uid[4];

    //wait for card
    while (uidLen < 1) {
        nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen);
    }

    lcd.clear();
    lcd.print("Master ID:");
    lcd.setCursor(0, 1);
    for (int i = 0; i < 4; i++)
        lcd.print(uid[i]);
    Serial.println();
    for (int i = 0; i < 4; i++) {
        Serial.print(uid[i]);
        EEPROM.put(MASTERCARDID + i, uid[i]);
        MasterID[i] = uid[i];
    }
    delay(3000);
    blk();
    lcd.clear();
    lcd.print("Enter PIN: ");
    lcd.setCursor(0, 1);
    String password = "";
    char c;
    lcd.setCursor(0, 1);
    Serial.println(password.length());
    while (password.length() < 8) {
        if (Serial.available()) {
            c = Serial.read();
            Serial.println(c);
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
                password += c;
                lcd.print(c);
                Serial.println(password);
            } else if (c == 'C') {
                password = password.substring(password.length() - 1);
                lcd.setCursor(0, 1);
                lcd.print("                        ");
                for (int i = 0; i < password.length(); i++)
                 lcd.print(password);   
				//lcd.print("X");
            } else if (c == 'O') {
                EEPROM.put(PINCODE, password);
                lcd.clear();
                lcd.print("Password saved");
                lcd.print(password);
                EEPROM.put(FIRSTRUN,0);
                break;
            }
        }
    }
    EEPROM.commit();
}

WiFiClient client;
void loop() {
    uint8_t uidLen, uid[4];
    if (millis() - t1 >= 1000) {
        client = server.available(); //check for new request on port 80
        t1 = millis();
    }
	if(millis() - t2 >= 500)
	{
		 nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen);
		
	}

    if (client & client.connected() & client.available()) { //if there is client
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println("Connection: close");
        client.println();

    }

    if (MasterAccess) {
        lcd.clear();
        lcd.print("Scan card to ");
        lcd.setCursor(0, 1);
        lcd.print("add or delete");
        while (uidLen < 1) {
            if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen) {
                if (!arrcmp(MasterID, uid)) {
                    if (addDeleteCard(uid))
                        lcd.

                }

            }
        }

    } else {

        if (cfg[0]) { //NFC ENABLED
            uint8_t uidLen = 0,
            uid[4];
            if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A,  & uid[0],  & uidLen) {
                if (arrcmp(uid, EEPROM.read(MASTERCARDID)) {
                    MasterAccess = true;
                    lcd.clear();
                }
                    if (
            }
        }
        if (cfg[1]) { //PIN CODE ENABLED

        }
    }
}
