# Electronic lock with multilevel access control 

Electronic lock with multilevel access control is a project made for course "Projekt Inżynierski" at Uniwersytet Pedagogiczny im. Komisji Edukacji Narodowej w Krakowie.

Main purpose for this project:
- create project with engineering aspects, like tests and analisys before implemeting solution
- learn how to create proper documentation for projects with LaTeX 


Project desctiption: (PL)
>Konstrukcja układu zamka elektronicznego z wielopoziomową kontrolą dostępu (kod alfanumeryczny, hasło dźwiękowe, karta RFID, itp.).
Zadaniem konstruktora jest zaprojektowanie i zmontowanie układu z trójstopniową
kontrolą dostępu. System powinien obsługiwać klawiaturę, wyświetlacz ciekłokrystaliczny
i symulować mechanizm zamka poprzez sterowanie serwomechanizmem. Wybór pozostałych
typów blokad należy do projektanta.
Poziom blokad i zabezpieczeń oraz postęp w ich wyłączaniu powinien być sygnalizowany przez diody LED i ewentualnie sygnał dźwiękowy. Dodatkowo system powinien mieć
możliwość uzbrajania poprzez sygnał WiFi np. aplikacja mobilna (smartfon).
Do obsługi czujników i transmisji można wykorzystać dostępne biblioteki programistyczne.


## Instalation
In `espmain/espMain.cpp` in line [726](https://github.com/Aveneid/engineeringProject2020/blob/master/espmain/espMain.cpp#L726) change `TEST` and `TEST1234` to your Wi-Fi BSSID and password.

Compile files using for example Arduino IDE and upload to your device. There's also precompiled binary available.


### Wiring diagram
Feel free to not stick hardly to this schema, this is an example wiring that is used in this project. You can always change pin configuration.
![Wiring diagram](https://github.com/Aveneid/engineeringProject2020/blob/master/wiring_diagram.png?raw=true)

## Usage

When device is powered for first time, it will have to be configured using simple wizard.

For this step you will need:
- RFID tag
- pin code (max 8 characters, 0-9 only)

When first run wizard ends, device is ready. It will accept cards, pin codes or barcodes (using scanner), checks if they are known and let someone in or not. After 3 incorrect tries device will **LOCK** for time defined by lock time (default 5 minutes).

Video [Google  Drive](https://drive.google.com/file/d/1IMRKisFa_bshOIxeV2hHRolmsU0VJJ5W/view?usp=sharing)

**:warning: Warning!** 
Remember to change administrator password in admin panel! Default password is not secure!

#### Adding or deleting cards

First you have to obtain master access (with card / token used in first run wizard), then bring card near RFID module to scan it. If card is present in device storage - will be deleted, otherwise will be added.

Cards can be deleted using administrator panel via WWW too.

#### Changing settings

RFID module, pin code or barcode scanner can be disabled using administrator panel. Lock time, pin code or administrator password can be changed there as well. 
Just open `http://smartlock/` to access administrator panel, login with adminstrator password (default 'admin') and here you go.

#### Resetting to default 
If master card was stolen, pin was lost or something else theres always an option. 
By shoting `D0` to +5V during boot an EEPROM clear will be performed, also default values of flags and passwords will be set.

**:warning: Warning!**
All stored cards will be erased too! Be sure to add them after reset!
 

## What's inside?

### Hardware used
 - [Arduino Nano](https://abc-rc.pl/product-pol-12737-NANO-V3-16MHz-USB-ATmega168P-odpowiednik-CH340-Klon-kompatybilny-z-Arduino.html) (AtMega328P)
 - [NodeMcu V3](https://abc-rc.pl/product-pol-7348-Modul-WIFI-ESP8266-NodeMcu-V3-CH340-Arduino-ESP12E.html) (ESP8266MOD)
 - Generic [16x2 LCD](https://abc-rc.pl/product-pol-6181-Wyswietlacz-LCD-2x16-niebieski-ze-sterownikiem-HD44780-QC1602A.html) with [I2C converter](https://abc-rc.pl/product-pol-6192-Konwerter-I2C-do-wyswietlacza-LCD-HD44780.html) (blue)
 - [RFID module](https://botland.com.pl/pl/moduly-i-tagi-rfid/8240-modul-rfidnfc-pn532-1356mhz-i2cspi-karta-i-brelok.html) - PN532 based
 - keypad from [Nano E](https://nano.novitus.pl/)
 - Barcode scanner - compatible with PS/2 standard
 
[Documentation](https://aveneid.github.io/engineeringProject2020/)

### Libraries used

##### espmain.ino
- LiquidCrystal_I2C
- Wire
- Adafruit_PN532
- EEPROM
- ESP8266WebServer
- pgmspace
- ESP8266WiFi
- SoftwareSerial
##### keyboard_listener.ino 
- SoftwareSerial
- PS2Keyboard
- Keypad

## License

MIT
