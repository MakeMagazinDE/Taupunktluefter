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

#define IS_LOGGING_ENABLED 1

#if IS_LOGGING_ENABLED

#define LOG_FILE_NAME     F("fan.csv")
#define LOG_INTERVAL_MIN  10
#define CSV_HEADER        F("Date/Time;Temperature T_I;Humidity H_I;Dew point DP_I;Temperature T_O;Humidity H_O;Dew point DP_O;Fan state;Fan time;Reboot;")


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

		#if IS_SERIAL_DEBUG_ENABLED
			Serial.println(F("SD card: OK"));
		#endif

		if (!SD.exists(LOG_FILE_NAME))
		{
			// File for csv file.
			File logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
			logFile.println(CSV_HEADER);
			logFile.close();
		}
	}
	else
	{
		lcd.print(F("SD card not found!"));

		#if IS_SERIAL_DEBUG_ENABLED
			Serial.println(F("SD card: ERROR"));
		#endif

		success = false;
	}

	delay(3000);
	wdt_reset();

	return success;
}

static void saveStateToSD(const String &logStr_)
{
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

void writeMsgToSD(const String &msg_)
{
	if (checkSD() == false)
	{
		return;
	}

	File logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
	logFile.print(createTimeStamp());
	logFile.println(msg_);
	logFile.close();
}

static bool shouldBeLogged()
{
	// Last save time, to calculate correct interval.
	static unsigned long lastSaveTime = 0;
	unsigned long t = millis() / 60000;

	if (lastSaveTime == 0)
	{
		lastSaveTime = t;
	}

	if (t < lastSaveTime + LOG_INTERVAL_MIN)
	{
		#if IS_USB_DEBUG_ENABLED
			Serial.println(F("should not log (t < lastSaveTime + LOG_INTERVAL_MIN)"));
		#endif
		return false;
	}

	#if IS_USB_DEBUG_ENABLED
		Serial.println(F("should log"));
	#endif

	lastSaveTime = t;

	return true;
}


void logValuesToSD(const MeasurePoint &mp_, bool isRelaisOn_, bool hasStateChanged_)
{
	static unsigned int fanStartMinutes = 0; // Start time of fan.
	static unsigned int fanTime = 0;         // Run time of fan.

	// Calculation for day summary only.
	if (isRelaisOn_ == true)
	{
		if (fanStartMinutes <= 0)
		{
			fanStartMinutes = getMinutesOfDay();
		}
	}
	else
	{
		if (fanStartMinutes > 0)
		{
			fanTime += getMinutesOfDay() - fanStartMinutes;
			fanStartMinutes = 0;
		}
	}

	#if IS_USB_DEBUG_ENABLED
		Serial.print(F("hasStateChanged_: "));
		Serial.print(hasStateChanged_);
		Serial.print(F(", hasDayChanged: "));
		Serial.println(hasDayChanged());
	#endif

	if (hasStateChanged_ == false && hasDayChanged() == false)
	{
		if (shouldBeLogged() == false)
		{
			#if IS_USB_DEBUG_ENABLED
				Serial.print(F("no log"));
			#endif
			return;
		}
	}

	char buf[sizeof "0.1"];
	String dataLogStr = "";

	dtostrf(mp_.temperatureI, 2, 1, buf);
	dataLogStr += buf;
	dataLogStr += ';';

	dtostrf(mp_.humidityI, 2, 1, buf);
	dataLogStr += buf;
	dataLogStr += ';';

	dtostrf(mp_.dewPointI, 2, 1, buf);
	dataLogStr += buf;
	dataLogStr += ';';

	dtostrf(mp_.temperatureO, 2, 1, buf);
	dataLogStr += buf;
	dataLogStr += ';';

	dtostrf(mp_.humidityO, 2, 1, buf);
	dataLogStr += buf;
	dataLogStr += ';';

	dtostrf(mp_.dewPointO, 2, 1, buf);
	dataLogStr += buf;
	dataLogStr += ';';

	if (isRelaisOn_ == true)
	{
		dataLogStr += "1;";
	}
	else
	{
		dataLogStr += "0;";
	}

	// A new day has started.
	char fanTimeStr[sizeof "1440"];
	fanTimeStr[0] = '\0';

	if (hasDayChanged())
	{
		// On first run, this might be true, so recheck here, if we had
		// realy a day change.
		if (isFirstDaySinceStart() == false)
		{
			// If fan is running, calculate fan time.
			if (fanStartMinutes > 0)
			{
				// 24h * 60m = 1440
				fanTime += 1440 - fanStartMinutes;
			}

			snprintf(fanTimeStr, sizeof fanTimeStr, "%d", fanTime);
		}
		else
		{
			// No day change.
			strcpy(fanTimeStr, "0");
		}

		fanTime = 0;
	}

	dataLogStr += fanTimeStr;
	dataLogStr += ';';
	dataLogStr += '0';
	dataLogStr += ';';

	saveStateToSD(dataLogStr);
}

#else

bool checkSD() { return true; }
void writeMsgToSD(const String &) {}
void logValuesToSD(const MeassurePoint &, bool , bool ) {}

#endif // IS_LOGGING_ENABLED
