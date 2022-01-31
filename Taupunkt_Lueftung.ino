// Dieser Code benötigt zwingend die folgenden Libraries:
#include "DHT.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>

#define RELAIPIN 6 // Anschluss des Lüfter-Relais
#define DHTPIN_1 5 // Datenleitung für den DHT-Sensor 1 (innen)
#define DHTPIN_2 4 // Datenleitung für den DHT-Sensor 2 (außen)

#define RELAIS_EIN LOW
#define RELAIS_AUS HIGH
bool rel;

#define DHTTYPE_1 DHT22 // DHT 22 
#define DHTTYPE_2 DHT22 // DHT 22  

// *******  Korrekturwerte der einzelnen Sensorwerte  *******
#define Korrektur_t_1  -3 // Korrekturwert Innensensor Temperatur
#define Korrektur_t_2  -4 // Korrekturwert Außensensor Temperatur
#define Korrektur_h_1  0  // Korrekturwert Innensensor Luftfeuchtigkeit
#define Korrektur_h_2  0  // Korrekturwert Außensensor Luftfeuchtigkeit
//***********************************************************

#define SCHALTmin   5.0 // minimaler Taupunktunterschied, bei dem das Relais schaltet
#define HYSTERESE   1.0 // Abstand von Ein- und Ausschaltpunkt
#define TEMP1_min  10.0 // Minimale Innentemperatur, bei der die Lüftung aktiviert wird
#define TEMP2_min -10.0 // Minimale Außentemperatur, bei der die Lüftung aktiviert wird

DHT dht1(DHTPIN_1, DHTTYPE_1); //Der Innensensor wird ab jetzt mit dht1 angesprochen
DHT dht2(DHTPIN_2, DHTTYPE_2); //Der Außensensor wird ab jetzt mit dht2 angesprochen

LiquidCrystal_I2C lcd(0x27,20,4); // LCD: I2C-Addresse und Displaygröße setzen

bool fehler = true;

void setup() {
  wdt_enable(WDTO_8S); // Watchdog timer auf 8 Sekunden stellen
  
  pinMode(RELAIPIN, OUTPUT);          // Relaispin als Output definieren
  digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten
  
  Serial.begin(9600);  // Serielle Ausgabe, falls noch kein LCD angeschlossen ist
  Serial.println(F("Teste Sensoren.."));

  lcd.init();
  lcd.backlight();                      
  lcd.setCursor(2,0);
  lcd.print(F("Teste Sensoren.."));
  
  byte Grad[8] = {B00111,B00101,B00111,B0000,B00000,B00000,B00000,B00000};      // Sonderzeichen ° definieren
  lcd.createChar(0, Grad);
  byte Strich[8] = {B00100,B00100,B00100,B00100,B00100,B00100,B00100,B00100};   // Sonderzeichen senkrechter Strich definieren
  lcd.createChar(1, Strich);
    
  dht1.begin(); // Sensoren starten
  dht2.begin();   
}

void loop() {

  float h1 = dht1.readHumidity()+Korrektur_h_1;       // Innenluftfeuchtigkeit auslesen und unter „h1“ speichern
  float t1 = dht1.readTemperature()+ Korrektur_t_1;   // Innentemperatur auslesen und unter „t1“ speichern
  float h2 = dht2.readHumidity()+Korrektur_h_2;       // Außenluftfeuchtigkeit auslesen und unter „h2“ speichern
  float t2 = dht2.readTemperature()+ Korrektur_t_2;   // Außentemperatur auslesen und unter „t2“ speichern
  
  if (fehler == true)  // Prüfen, ob gültige Werte von den Sensoren kommen
  {
    fehler = false; 
    if (isnan(h1) || isnan(t1) || h1 > 100 || h1 < 1 || t1 < -40 || t1 > 80 )  {
      Serial.println(F("Fehler beim Auslesen vom 1. Sensor!"));
      lcd.setCursor(0,1);
      lcd.print(F("Fehler Sensor 1"));
      fehler = true;
    }else {
     lcd.setCursor(0,1);
     lcd.print(F("Sensor 1 in Ordnung"));
   }
  
    delay(2000);  // Zeit um das Display zu lesen
  
      if (isnan(h2) || isnan(t2) || h2 > 100 || h2 < 1 || t2 < -40 || t2  > 80)  {
        Serial.println(F("Fehler beim Auslesen vom 2. Sensor!"));
        lcd.setCursor(0,2);
        lcd.print(F("Fehler Sensor 2"));
        fehler = true;
      } else {
        lcd.setCursor(0,2);
        lcd.print(F("Sensor 2 in Ordnung"));
     }

    delay(2000);  // Zeit um das Display zu lesen
  }
  if (isnan(h1) || isnan(t1) || isnan(h2) || isnan(t2)) fehler = true;
   
 if (fehler == true) {
    digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten 
    lcd.setCursor(0,3);
    lcd.print(F("CPU Neustart....."));
    while (1);  // Endlosschleife um das Display zu lesen und die CPU durch den Watchdog neu zu starten
 }
 wdt_reset();  // Watchdog zurücksetzen

//**** Taupunkte errechnen********
float Taupunkt_1 = taupunkt(t1, h1);
float Taupunkt_2 = taupunkt(t2, h2);

// Werteausgabe auf Serial Monitor
 Serial.print(F("Sensor-1: " ));
  Serial.print(F("Luftfeuchtigkeit: "));
  Serial.print(h1);                     
  Serial.print(F("%  Temperatur: "));
  Serial.print(t1);
  Serial.print(F("°C  "));
  Serial.print(F("  Taupunkt: "));
  Serial.print(Taupunkt_1);
  Serial.println(F("°C  "));

  Serial.print("Sensor-2: " );
  Serial.print(F("Luftfeuchtigkeit: "));
  Serial.print(h2);
  Serial.print(F("%  Temperatur: "));
  Serial.print(t2);
  Serial.print(F("°C "));
  Serial.print(F("   Taupunkt: "));
  Serial.print(Taupunkt_2);
  Serial.println(F("°C  "));


 Serial.println();

  // Werteausgabe auf dem I2C-Display
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("S1: "));
  lcd.print(t1); 
  lcd.write((uint8_t)0); // Sonderzeichen °C
  lcd.write(('C'));
  lcd.write((uint8_t)1); // Sonderzeichen |
  lcd.print(h1);
  lcd.print(F(" %"));

  lcd.setCursor(0,1);
  lcd.print(F("S2: "));
  lcd.print(t2); 
  lcd.write((uint8_t)0); // Sonderzeichen °C
  lcd.write(('C'));
  lcd.write((uint8_t)1); // Sonderzeichen |
  lcd.print(h2);
  lcd.print(F(" %"));

  lcd.setCursor(0,2);
  lcd.print(F("Taupunkt 1: "));
  lcd.print(Taupunkt_1); 
  lcd.write((uint8_t)0); // Sonderzeichen °C
  lcd.write(('C'));

  lcd.setCursor(0,3);
  lcd.print(F("Taupunkt 2: "));
  lcd.print(Taupunkt_2); 
  lcd.write((uint8_t)0); // Sonderzeichen °C
  lcd.write(('C'));

delay(6000); // Zeit um das Display zu lesen
wdt_reset(); // Watchdog zurücksetzen

  lcd.clear();
  lcd.setCursor(0,0);
  
float DeltaTP = Taupunkt_1 - Taupunkt_2;

if (DeltaTP > (SCHALTmin + HYSTERESE))rel = true;
if (DeltaTP < (SCHALTmin))rel = false;
if (t1 < TEMP1_min )rel = false;
if (t2 < TEMP2_min )rel = false;

if (rel == true)
{
  digitalWrite(RELAIPIN, RELAIS_EIN); // Relais einschalten
  lcd.print(F("Lueftung AN"));  
} else {                             
  digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten
  lcd.print(F("Lueftung AUS"));
}

 lcd.setCursor(0,1);
 lcd.print("Delta TP: ");
 lcd.print(DeltaTP);
 lcd.write((uint8_t)0); // Sonderzeichen °C
 lcd.write('C');

 delay(4000);   // Wartezeit zwischen zwei Messungen
 wdt_reset();   // Watchdog zurücksetzen 
 
}

float taupunkt(float t, float r) {
  
float a, b;
  
  if (t >= 0) {
    a = 7.5;
    b = 237.3;
  } else if (t < 0) {
    a = 7.6;
    b = 240.7;
  }
  
  // Sättigungsdampfdruck in hPa
  float sdd = 6.1078 * pow(10, (a*t)/(b+t));
  
  // Dampfdruck in hPa
  float dd = sdd * (r/100);
  
  // v-Parameter
  float v = log10(dd/6.1078);
  
  // Taupunkttemperatur (°C)
  float tt = (b*v) / (a-v);
  return { tt };  
}


 void software_Reset() // Startet das Programm neu, nicht aber die Sensoren oder das LCD 
  {
    asm volatile ("  jmp 0");  
  }
