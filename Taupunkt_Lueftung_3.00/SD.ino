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

// Pin for CS connection.
#define SD_CS_PIN 10

bool checkSD()
{
	bool success = true;

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Searching SD card."));
	lcd.setCursor(2, 1);

	if (SD.begin(SD_CS_PIN))
	{
		lcd.print(F("SD card found."));

		if (useSerial)
			Serial.println(F("SD card: OK"));

		if (!SD.exists(LOG_FILE_NAME))
		{
			// File for csv file.
			File logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
			logFile.println(Headerzeile);
			logFile.close();
		}
	}
	else
	{
		lcd.print(F("SD card not found!"));

		if (useSerial)
			Serial.println(F("SD card: ERROR"));

		success = false;
	}

	delay(3000);
	wdt_reset();

	return success;
}

void saveToSD(const String &logStr_, bool dayChange_)
{
	// Last save time, to calculate correct interval.
	static unsigned long lastSaveTime = 0;
	unsigned long t = millis() / 60000;

	if (lastSaveTime == 0)
	{
		lastSaveTime = t;
	}

	// If a new day has started, or the interval is reached save the data.
	if (t < lastSaveTime + LOG_INTERVAL_MIN && !dayChange_)
	{
		return;
	}

	lastSaveTime = t;

	if (checkSD() == false)
	{
		return;
	}

	lcd.clear();
	lcd.print(F("Saving data."));

	File logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
	logFile.print(createTimeStamp());
	logFile.println(';' + logStr_);
	logFile.close();

	delay(3000);

	lcd.clear();
	lcd.setCursor(1, 0);
}
