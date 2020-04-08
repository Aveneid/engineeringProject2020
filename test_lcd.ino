//Program testujacy dzialanie wyswietlacza LCD - Drukuje tekst wyslany przez port COM

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
	lcd.begin();
	lcd.backlight();
	Serial.begin(9600);
}

void loop()
{
	if (Serial.available()) {
		delay(100);
		lcd.clear();
		while (Serial.available() > 0) {
			lcd.write(Serial.read());
		}
	}
}
