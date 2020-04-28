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

LiquidCrystal_I2C lcd(0x27, 16, 2);  //create object for LCD class
ESP8266WebServer server(80);       //enable server on port 80

Adafruit_PN532 nfc(D8, D7);          //rq rst


//MAIN VARIABLES
bool cfg[2];                         // disable pin, disable nfc,
bool MasterAccess = false;           //master access flag
uint8_t MasterID[4];                 //master card ID - to prevent future reads from eeprom to save some time and power
unsigned long t1;                    //counter for wifi
unsigned long t2;                    //counter for card scanner
unsigned long retryTimer;
int retryTimerSeconds = 0;
uint8_t tries = 0;                   //counter for invalid card / passwords
uint8_t uid[4], uidLen;              //card data
String password = "";                //password holder
char c;                              //incoming character holder



/*
*		switches display background for short period of time
*/
void blk() {
  lcd.noBacklight();
  delay(100);
  lcd.backlight();
}
/*
*	compare two arrays - type uint8_t - check cards ids
*	returns: 1 if arrays are the same
*			 0 if not
*/
bool arrcmp(uint8_t a[4], uint8_t b[4]) {     
  for (int i = 0; i > 4; i++)
    if (a[i] != b[i])
      return false;
  return true;
}
/*
*		performs EEPROM clear and fill with default values
*/
void clearData() { 
  lcd.clear();                  
  for (int i = 0; i < EEPROM.length(); i++)
    EEPROM.put(i, 0);                 

  EEPROM.put(FIRSTRUN, 1);                   
  EEPROM.put(1, 'a'); EEPROM.put(2, 'd'); EEPROM.put(3, 'm'); EEPROM.put(4, 'i'); EEPROM.put(5, 'n'); EEPROM.put(6, '\0'); //default admin password for web server
  EEPROM.put(NFCENABLED, 1);                 
  EEPROM.put(PASSENABLED, 1);               
  lcd.print("RESETTING...");
  EEPROM.commit();                  
  delay(1000);
  lcd.clear();
}



/*
*	tries to find scanned card in memory
*	returns: 1 if card found and deleted
*			 0 if card not found and added
*/
bool addDeleteCard(uint8_t a[4]) {
  uidLen = 0;
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

/*	checks if scanned card is known
*	returns: 1 if yes
			 0 if not
*/
bool findCard(uint8_t a[4]) {
  uidLen = 0;
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
/*
*		checks if given given password is correct
*		returns: 1 if yes
*				 0 if no
*/
bool checkPass(String pass) {
  String mainPass;
  for (int i = 0; i < 8; i++) {
    char c = EEPROM.read(PINCODE + i);
    if (c != '\0')
      mainPass += (char)EEPROM.read(PINCODE + i);
    else break;
  }
  if (mainPass == pass)
    return true;
  return false;
}





/*
*		main function that grants access
*/
void accessGranted() {
  lcd.clear();
  lcd.print("ACCESS GRANTED");

  tries = 0;
  retryTimerSeconds = 0;
  retryTimer = 0;
  password = "";

  digitalWrite(OUT, HIGH);
  delay(5000);
  digitalWrite(OUT, LOW);
}

/*
*		main function that refuses access
*/
void accessDenied() {
  lcd.clear();
  lcd.print("ACCESS DENIED");
  password = "";

  tries++;
  if (tries == 3) retryTimer = millis();
  delay(5000);
}

/*
*		function that provides `first run config` for owner / administrator
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
    if (Serial.available()) {
      c = Serial.read();
      if ((c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9' ) && password.length() < 9) {
        password += c;
        lcd.print(c);

      } else if (c == 'C') {
        password = password.substring(0,password.length() - 1);
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

/*
*
*		WEB SERVER STUFF
*
*/

bool auth(){
	if(server.hasHeader("Cookie") && server.header("Cookie").indexOf("SESSID=1") != -1) return true;
	return false;
}

void handleNotFound(){
	
	String msg = "Not found\n\n";
	msg += server.uri;	
	server.send(404,"text/plain",msg);
}


const char indexHTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>SmartLock - System zarządzania</title><script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script><link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css"><script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js"></script><script type="text/javascript">function cD(id){if(confirm("Usunąć karte o ID: " + id + "?"))window.location = "delete?ID=" + id;} function toggleNFC(){window.location ="toggleNFC";} function togglePIN(){window.location = "togglePIN";} </script></head><body><div class="container-fluid"><div class="row"><div class="col-md-12"><div class="page-header"><h1>SmartLock <small>System zarządzania</small></h1></div><dl><dt>Panel administracyjny</dt><dd>W tym panelu możesz zarządzać zamkiem, sprawdzić rozpoznawane karty oraz ilość dostępnego miejsca dla nich. <br \>Możesz również zmienić ustawienia dostępu do zamka, zmienić PIN, włączyć / wyłączyć możliwość odczytu kart czy dostępu kodem PIN.</dd></dl><div class="row"><div class="col-md-7"><h3>Rozpoznawane karty:</h3><table class="table table-sm"><thead><tr><th>#</th><th>Identyfikator</th><th>Akcja</th></tr></thead><tbody>)rawliteral";
const char indexHTML2[] PROGMEM = R"rawliteral(</tbody></table> </div><div class="col-md-5"><h3>Dostępna pamięć kart:</h3><div class="progress"><div class="progress-bar" style="width: )rawliteral";
const char indexHTML3[] PROGMEM = R"rawliteral("% !important"></div><label>)rawliteral";
const char indexHTML4[] PROGMEM = R"rawliteral("%</label><form role="form" action="changePass"><div class="form-group"> <h4> Zmiana hasła</h4><label for="exampleInputPassword1">Nowe hasło</label><input type="password" class="form-control" id="exampleInputPassword1" name="PASS"/></div><button type="submit" class="btn btn-primary">Zmień</button></form><br /><div class="checkbox"><label>)rawliteral";
const char indexHTML5[] PROGMEM = R"rawliteral("</div></div></div></div></div></body></html>)rawliteral";



void index(){
	String header;
	if(!auth()){
		header = "HTTP/1.1 301 OK\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
		server.sendContent(header);
		return;
	}
	int cardsCount = EEPROM.read(USERCARDSCOUNT);
	header = indexHTML;
	for(int i=0;i<cardsCount;i++){
		String data = String(EEPROM.read(USERCARDS + i * CARDSIZE)) + String(EEPROM.read(USERCARDS + i * CARDSIZE+1)) + String(EEPROM.read(USERCARDS + i * CARDSIZE+2)) + String(EEPROM.read(USERCARDS + i * CARDSIZE+3));
		header += "<tr class='table-active'><td>"+ String(i) + "</td><td>"+data+"</td><td><button type='button' onClick='cD("+data+")' class='btn btn-sm btn-danger'>Usuń</button></td></tr>";
	}
	header += indexHTML2 + String((int)cardsCount / 120) + indexHTML3 + String((int)cardsCount / 120) + indexHTML4;
	header += "<input onClick='toggleNFC()' type='checkbox'"; if(cfg[0]) header += "checked"; header += "/> Włącz obsługę kart</label></div>";
	header += "<input onClick='togglePIN()' type='checkbox'"; if(cfg[1]) header += "checked"; header += "/> Włącz obsługę kodu PIN</label></div> ";
	header += indexHTML5;

	server.send(200,"text/html",header);
}


void del(){
	if(server.hasArg("ID")){
		if(server.arg("ID").length == 8){
			int cardsCount = EEPROM.read(USERCARDSCOUNT);
			uint8_t cards[cardsCount][4];
			
			for(int i=0;i<cardsCount;i++)
				for(jnt j=0;j<4;j++)
					cards[i][j] = EEPROM.read(USERCARDS + i * CARDSIZE + j);
		
			uint8_t card[4];
			for(int i=0;i<4;i++)
				card[i] = (uint8_t)atoi(server.arg("ID").substring(0,2));    //converting url parameter to int array (card data)
				
			
			for(int i=0;i<cardsCount;i++)
				if(arrcmp(cards[i],card)){
					for(int a =0;a<4;i++)
						cards[i][a] = cards[cardsCount][a];
					
					for(int i=0;i<cardsCount--;i++)
						EEPROM.put(USERCARDS + i * CARDSIZE + j, cards[i][j]);
					
					EEPROM.put(USERCARDSCOUNT,cardsCount--);

					break;
				}
			
			
			EEPROM.commit();
		}
	}
}

void login(){
	server.send(200,"text/html","<html><body><form action='/login'>Password: <input type='password' name='PASS'><input type='submit'></form></body></html>");		
	String hd="";
	if(server.hasArg("LOGOUT")){ hd = "HTTP/1.1 301 OK\r\nSet-Cookie: SESSID=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n"; server.sendContent(hd);}
	if(server.hasArg("PASS")){
		String pass;
		for (int i = 0; i < 8; i++) {
			char c = EEPROM.read(ADMINPASS + i);
			if (c != '\0')
				pass += c;
		}
		
		if(server.arg("PASS") == pass){
			hd = "HTTP/1.1 301 OK\r\nSet-Cookie: SESSID=1\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
			server.sendContent(hd);
			return;
		}
		hd = "Wrong password :v";
	}
	String msg = "<html><body><center><b>"+hd+"</b></center></body></html>";
	server.send(200,"text/html",msg);
}

void toggleNFC(){
	cfg[0] = !cfg[0];
	if(cfg[0])
		EEPROM.put(NFCENABLED,1);
	else
		EEPROM.put(NFCENABLED,0);
	server.sendContent("HTTP/1.1 301 OK\r\nSet-Cookie: SESSID=0\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
	EEPROM.commit();
}


void togglePIN(){
	cfg[1] = !cfg[1];
		if(cfg[1])
		EEPROM.put(PASSENABLED,1);
	else
		EEPROM.put(PASSENABLED,0);
	server.sendContent("HTTP/1.1 301 OK\r\nSet-Cookie: SESSID=0\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n");
	EEPROM.commit();
}




/* 
*		main setup function
*/
void setup() {
  Serial.begin(9600);                 //serial for keyboard
  EEPROM.begin(512);                  //eeprom for config and data

  lcd.init();
  lcd.backlight();

  nfc.begin();
  nfc.SAMConfig();

  pinMode(D4, INPUT);
  pinMode(OUT, OUTPUT);


  server.on("/",index);
  server.on("/login",login);
  server.on("/delete",del);
  server.on("/changePass",changePass);
  server.on("/togglePIN",togglePIN);
  server.on("/toggleNFC",toggleNFC);
  server.onNotFound(handleNotFound);
  
      const char * headerkeys[] = {
        "Cookie"
    };
	
	
	size_t headerkeyssize = sizeof(headerkeys) / sizeof(char * );
	server.collectHeaders(headerkeys, headerkeyssize);
    server.begin();

  if (!digitalRead(D4))
    clearData();

  if (EEPROM.read(FIRSTRUN) == 1) {
    firstRunConfig();
  }
  
  cfg[0] = EEPROM.read(NFCENABLED);
  cfg[1] = EEPROM.read(PASSENABLED);

  t1 = millis();
  t2 = millis();
}




/*
*		main loop function
*		provides control for RFID reader, password entry, admin access and others
*/
void loop() {

	server.handleClient();

  if (tries < 3) {                                                    
    if (millis() - t1 >= 1000) {
      //    client = server.available();                                                  //check for new request on port 80
      t1 = millis();
    }
    if (millis() - t2 >= 500)                                                             // check for new card
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


    if (MasterAccess) {													//MASTER HAS ACCESS
      lcd.clear();
      lcd.print("Scan card to ");
      lcd.setCursor(0, 1);
      lcd.print("add or delete");

      if (!arrcmp(MasterID, uid)) {
		  if(EEPROM.read(USERCARDSCOUNT)<120)
			if (addDeleteCard(uid)) {        								//card deleted
			lcd.clear();
			lcd.print("Card deleted");
			} else {                                                        //card added
			lcd.clear();
			lcd.print("Card added");
			}
        delay(500);
      } else {                                                        	 //master card scanned - exit from masterMode
        lcd.clear();
        lcd.print("EXITING....");
        MasterAccess = false;
        delay(1000);
        lcd.clear();
        return;
      }																	//MASTER ACCESS ENDED
	  
    } else {
      if (cfg[0]) {                                                  		 //NFC ENABLED
        if (uidLen != 0) {
          if (findCard(uid))
            accessGranted();
          else
            accessDenied();
        }
      }
      if (cfg[1]) {                                                  		  //PASSWORD ENABLED
        if (Serial.available()) {
          c = Serial.read();
          if ((c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9' ) && password.length() < 9) {
            password += c;
            lcd.print("X");
          }
          else if (c == 'C') {
            password = password.substring(0,password.length() - 1);
            lcd.setCursor(0, 1);
            lcd.print("                        ");

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
    }
  }
  else {                            //TOO MANY TRIES - WAITING  `LOCKTIME`
    lcd.clear();
    lcd.print("LOCKED");
    if (retryTimer - millis() >= 1000)
      retryTimerSeconds++;
    if (retryTimerSeconds == 300) {
      tries = 0;
      retryTimer = 0;
      retryTimerSeconds = 0;
    }
    delay(700);
  }
}
