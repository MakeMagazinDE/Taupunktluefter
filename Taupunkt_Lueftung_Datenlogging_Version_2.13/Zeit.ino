//////////////////////////////////////////////////////////////////////////////
// Das Taupunkt-Lüftungssystem 
// mit Datenlogging
// 
// Routinen zur Erzeugung eines Zeitstempels
// 
// veröffentlicht in der MAKE 2/2022
//
// Ulrich Schmerold
// 3/2022
//////////////////////////////////////////////////////////////////////////////


void make_time_stamp ()
{
  RTC.read(tm);
  snprintf(stamp,sizeof(stamp),"%02d.%02d.%d %02d:%02d",tm.Day,tm.Month,tmYearToCalendar(tm.Year),tm.Hour,tm.Minute);
}


bool RTC_start()
{ 
 if (logging == true)
 { 
  if (RTC.read(tm)) 
  {     
    make_time_stamp ();
    lcd.setCursor(2,0); 
    lcd.print(stamp);
  } else {
    if (RTC.chipPresent()) {
      lcd.clear();
      lcd.print(F("RTC hat keine Zeit"));
      delay(2000);
    } else {
      lcd.clear();
      lcd.print(F("Kein Signal vom RTC"));
      delay(2000);
    }
    logging = false;
     return(false);
  }
 }
}
