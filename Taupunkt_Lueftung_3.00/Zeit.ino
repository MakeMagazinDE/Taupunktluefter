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


const char *createTimeStamp ()
{
	Ds1302::DateTime now;
	static char timeStr[sizeof "YYYY-MM-DD hh:mm:ss"];

	rtc.getDateTime(&now);

	snprintf(
		timeStr, sizeof(timeStr),
		"%04d-%02d-%02d %02d:%02d:%02d",
		now.year + 2000,
		now.month,
		now.day,
		now.hour,
		now.minute,
		now.second
	);

	/* if (useSerial) */
	/* 	Serial.println(timeStr); */

	return timeStr;
}


void startRTC()
{
	rtc.init();

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("RTC initialized."));

	if (useSerial)
	{
		Serial.println(F("RTC OK"));
	}

	delay(1000);

#if SET_TIME
	rtc.setDateTime(&setTimeStruct);

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("RTC time set."));
	delay(1000);
#endif

	wdt_reset();
	lcd.clear();
}
