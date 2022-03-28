//////////////////////////////////////////////////////////////////////////////
// Das Taupunkt-Lüftungssystem 
// mit Datenlogging
// 
// Routinen zur Speicherung auf einer SD-Karte
//
// veröffentlicht in der MAKE 2/2022
//
// Ulrich Schmerold
// 3/2022
//////////////////////////////////////////////////////////////////////////////

File logFile;                    // Variable für die csv-Datei
#define CS_PIN 10                // An diesem Pin ist die CS-Leitung angeschlossen

unsigned long TimeDaten = 0;    // zur Berechnung, wann wieder gespeichert wird


void test_SD(){
  if (logging == true)
  {
    // --------- SD-Karte suchen  -------------------
  lcd.setCursor(0,0);
  lcd.print(F("Suche SD Karte.."));
  lcd.setCursor(2,1);

  if (!SD.begin(CS_PIN)){
   lcd.print(F("SD nicht gefunden!"));
   logging = false;
   } else {
    lcd.print(F("SD  gefunden"));
    if (not SD.exists(logFileName) )  
    {
      logFile = SD.open(logFileName, FILE_WRITE); 
      logFile.println(Headerzeile); // Header schreiben
      logFile.close();  
    }    
   }
   delay(3000);   // Zeit um das Display zu lesen
   wdt_reset();   // Watchdog zurücksetzen
  }
}


void save_to_SD()
{ 
  unsigned long t; 

 t = millis() / 60000;               
  if (  TimeDaten == 0 ) TimeDaten =  t;  

  // -------------------------  Sensorenwerte abspeichern ----------------------
  if (((TimeDaten + LogInterval) <= t)  or ( Tageswechsel ))
  {
    TimeDaten = t;
    Tageswechsel=false;
    test_SD();
    wdt_reset(); // Watchdog zurücksetzen
      lcd.clear();
      lcd.print(F("Speichere Datensatz"));   
      make_time_stamp();
      File logFile = SD.open(logFileName, FILE_WRITE);  // Oeffne Datei
      // Serial.print( stamp);  Serial.print( ';' + LogData );  Serial.println(Luefterzeit);
       logFile.print (stamp);
      logFile.println( ';' + LogData );
    logFile.close(); 
    delay(4000);
  } 
}
