//////////////////////////////////////////////////////////////////////////////
// Das Taupunkt-Lüftungssystem 
// mit Datenlogging
// 
// veröffentlicht in der MAKE 1/2022 und 2/2022
//
// Ulrich Schmerold
// 3/2022
//////////////////////////////////////////////////////////////////////////////
#define Software_version "Version: 2.13"

// Dieser Code benötig zwingend die folgenden Libraries:
#include "DHT.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>
#include <DS1307RTC.h>
#include <SD.h>
#include <SPI.h>

tmElements_t tm;

#define RELAIPIN 6     // Anschluss des LüfterRelais
#define DHTPIN_1 5     // Datenleitung für den DHT Sensor 1 (innen)
#define DHTPIN_2 4     // Datenleitung für den DHT Sensor 2 (außen)

#define RELAIS_EIN LOW
#define RELAIS_AUS HIGH
bool rel;
bool fehler = true;

#define DHTTYPE_1 DHT22   // DHT 22 
#define DHTTYPE_2 DHT22   // DHT 22  

// ***************************   Korrekturwerte der einzelnen Sensorwerten.  ***************
#define Korrektur_t_1  -3   // Korrekturwert Innensensor Temperatur
#define Korrektur_t_2  -4   // Korrekturwert Außensensor Temperatur
#define Korrektur_h_1  0    // Korrekturwert Innensensor Luftfeuchtigkeit
#define Korrektur_h_2  0    // Korrekturwert Außensensor Luftfeuchtigkeit
//******************************************************************************************

#define SCHALTmin   5.0   // minimaler Taupuntunterschied, bei dem das Relais schaltet
#define HYSTERESE   1.0   // Abstand von Ein- und Ausschaltpunkt
#define TEMP1_min  10.0   // Minimale Innentemperatur, bei der die Lüftung aktiviert wird
#define TEMP2_min -10.0   // Minimale Außentemperatur, bei der die Lüftung aktiviert wird

DHT dht1(DHTPIN_1, DHTTYPE_1);  //Der Innensensor wird ab jetzt mit dht1 angesprochen
DHT dht2(DHTPIN_2, DHTTYPE_2);  //Der Außensensor wird ab jetzt mit dht2 angesprochen

LiquidCrystal_I2C lcd(0x27,20,4); // LCD Display I2C Addresse und Displaygröße setzen

//++++++++++++++++++++++++++++++++ Variablen für das Datenlogging ***************************************
#define Headerzeile F("Datum|Zeit;Temperatur_S1;Feuchte_H1;Taupunkt_1;Temperatur_S2;Feuchte_H2;Taupunkt_2;Luefter_Ein/Aus;Laufzeit_Luefter;")

#define logFileName F("Luefter1.csv")  // Name der Datei zum Abspeichern der Daten (Dateinamen-Format: 8.3)!!!!
#define LogInterval 10                   // Wie oft werden die Messwerte aufgezeichnet ( 5 = alle 5 Minuten)

bool logging = true;                    // Sollen die Daten überhaupt protokolliert werden?
String LogData = "" ;                   // Variable zum Zusammensetzen des Logging-Strings.
char stamp[17];                         // Variable für den Zeitstempel.
unsigned int LuefterStart = 0;          // Wann wurde der Lüfter eingeschaltet?
unsigned int LuefterLaufzeit = 0;       // Wie lange lief der Lüfter?
char StrLuefterzeit[6];                 // Lüfterlaufzeit als String zur weiteren Verwendung.
uint8_t Today = 0;                      // Das heutige Datum (nur Tag), zur Speicherung bei Tageswechsel.
bool Tageswechsel=false;
//********************************************************************************************************

void setup() {
  wdt_enable(WDTO_8S); // Watchdog timer auf 8 Sekunden stellen
  
  pinMode(RELAIPIN, OUTPUT);          // Relaispin als Output definieren
  digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten
  
  lcd.init();
  lcd.backlight();
  lcd.clear();

   wdt_reset();  // Watchdog zurücksetzen
   
  //--------------------- Logging ------------------------------------------------------------------------------------  
  if (logging == true)
  { 
    lcd.setCursor(0,1);
    lcd.print(Software_version);  // Welche Softwareversion läuft gerade
    RTC_start();     // RTC-Modul testen. Wenn Fehler, dann kein Logging
    delay (4000);    // Zeit um das Display zu lesen
    lcd.clear(); 
     wdt_reset();  // Watchdog zurücksetzen
    test_SD();       // SD-Karte suchen. Wenn nicht gefunden, dann kein Logging ausführen
    Today = tm.Day ;
    //------------------------------------------------ Neustart aufzeichnen -------------------------------------
   if (logging == true) // kann sich ja geändert haben wenn Fehler bei RTC oder SD
   {   
    make_time_stamp();   
    File logFile = SD.open(logFileName, FILE_WRITE);
    logFile.print(stamp);
    logFile.println(F(": Neustart"));                    // Damit festgehalten wird, wie oft die Steuerung neu gestartet ist
    logFile.close();  
   }
  } //---------------------------------------------------------------------------------------------------------------------
    
  byte Grad[8] = {B00111,B00101,B00111,B0000,B00000,B00000,B00000,B00000};      // Sonderzeichen ° definieren
  lcd.createChar(0, Grad);
  byte Strich[8] = {B00100,B00100,B00100,B00100,B00100,B00100,B00100,B00100};   // Sonderzeichen senkrechter Strich definieren
  lcd.createChar(1, Strich);
       
  dht1.begin(); // Sensoren starten
  dht2.begin(); 
}

void loop() {
  RTC.read(tm);    
  float h1 = dht1.readHumidity()+Korrektur_h_1;       // Innenluftfeuchtigkeit auslesen und unter „h1“ speichern
  float t1 = dht1.readTemperature()+ Korrektur_t_1;   // Innentemperatur auslesen und unter „t1“ speichern
  float h2 = dht2.readHumidity()+Korrektur_h_2;       // Außenluftfeuchtigkeit auslesen und unter „h2“ speichern
  float t2 = dht2.readTemperature()+ Korrektur_t_2;   // Außentemperatur auslesen und unter „t2“ speichern
     
  if (fehler == true)  // Prüfen, ob gültige Werte von den Sensoren kommen
  {   
      lcd.clear();
      lcd.setCursor(2,0);
      lcd.print(F("Teste Sensoren.."));
    fehler = false; 
    if (isnan(h1) || isnan(t1) || h1 > 100 || h1 < 1 || t1 < -40 || t1 > 80 )  {
      // Serial.println(F("Fehler beim Auslesen vom 1. Sensor!"));
      lcd.setCursor(0,1);
      lcd.print(F("Fehler Sensor 1"));
      fehler = true;
    }else {
     lcd.setCursor(0,1);
     lcd.print(F("Sensor 1 in Ordnung"));
   }
  
    delay(2000);  // Zeit um das Display zu lesen
  
      if (isnan(h2) || isnan(t2) || h2 > 100 || h2 < 1 || t2 < -40 || t2  > 80)  {
       // Serial.println(F("Fehler beim Auslesen vom 2. Sensor!"));
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
 
delay(5000); // Zeit um das Display zu lesen
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
  if (LuefterStart <=0 && logging == true){ LuefterStart = tm.Hour*60 + tm.Minute;}
} else {                             
  digitalWrite(RELAIPIN, RELAIS_AUS); // Relais ausschalten
  lcd.print(F("Lueftung AUS"));
  if( LuefterStart > 0 && logging == true){
    LuefterLaufzeit += (tm.Hour*60 + tm.Minute - LuefterStart);
    LuefterStart = 0;
  }
}   
  lcd.setCursor(0,1);
 lcd.print(F("Delta TP: "));
 lcd.print(DeltaTP);
 lcd.write((uint8_t)0); // Sonderzeichen °C
 lcd.write('C');
 
 make_time_stamp();
 lcd.setCursor(0,3);
 lcd.print(stamp);
 lcd.setCursor(0,2);
 if (logging == true)lcd.print(F("Logging AN")); else lcd.print(F("Logging AUS"));

 delay(4000);   // Wartezeit zwischen zwei Messungen
 wdt_reset();   // Watchdog zurücksetzen 

 //--------------------------------------------logging-----------------------------------------------------
if (logging == true)
 { 
  if  ( Today  != tm.Day)                                                     // Tageswechsel ==> Lüfterzeit abspeichern
   {  
      Tageswechsel = true;                                                    // ==> Sofort speichern (siehe SD.ino) ==> Nicht erst wenn LogIntervall abgelaufen ist
       if (LuefterStart > 0 ) LuefterLaufzeit += (1440 - LuefterStart);       // ==>Lüfter läuft gerade
       snprintf(StrLuefterzeit,sizeof(StrLuefterzeit),"%d;",LuefterLaufzeit); 
      Today = tm.Day;
      LuefterLaufzeit = 0;
    } else {
      strcpy( StrLuefterzeit , "0;");    // Kein Tageswechsel, nur Platzhalter abspeichern
   }

  char buff[4];
  LogData="";
  dtostrf(t1, 2, 1, buff); LogData += buff ; LogData += ';';
  dtostrf(h1, 2, 1, buff); LogData += buff ; LogData += ';';
  dtostrf(Taupunkt_1, 2, 1, buff); LogData += buff ; LogData += ';';
  dtostrf(t2, 2, 1, buff); LogData += buff ; LogData += ';';
  dtostrf(h2, 2, 1, buff); LogData += buff;LogData += ';';
  dtostrf(Taupunkt_2, 2, 1, buff); LogData += buff;LogData += ';';
  if (rel == true) LogData +="1;"; else LogData += "0;";
  LogData += StrLuefterzeit;
  
  save_to_SD(); // Daten auf die SD Karte speichern
 }
}
//--------------------------------------------------------------------------------------------------------

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
