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

#if IS_RTC_ENABLED

#define RTC_PIN_ENA 9
#define RTC_PIN_CLK 7
#define RTC_PIN_DAT 8

Ds1302 rtc(RTC_PIN_ENA, RTC_PIN_CLK, RTC_PIN_DAT);

const char *createTimeStamp ()
{
	Ds1302::DateTime now;
	static char timeStr[sizeof "YYYY-MM-DD hh:mm:ss"];

	rtc.getDateTime(&now);

	snprintf(
		timeStr, sizeof timeStr,
		"%04d-%02d-%02d %02d:%02d:%02d",
		now.year + 2000,
		now.month,
		now.day,
		now.hour,
		now.minute,
		now.second
	);

	return timeStr;
}


void startRTC()
{
	rtc.init();

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("RTC initialized."));

	#if IS_USB_DEBUG_ENABLED
		Serial.println(F("RTC OK"));
	#endif

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

static uint8_t today = 0;
static Ds1302::DateTime now;
static bool _hasDayChanged = true;
static bool _isFirstDaySinceStart = true;

void getNow() {
	rtc.getDateTime(&now);

	_hasDayChanged = (today != now.day) ? true : false;

	// No day change, if it is not initialized.
	if (_hasDayChanged && today != 0)
	{
		_isFirstDaySinceStart = false;
	}

	today = now.day;
}

bool hasDayChanged() {
	return _hasDayChanged;
}

unsigned int getMinutesOfDay() {
	return now.hour * 60 + now.minute;
}

unsigned int getSecondsOfDay() {
	return now.hour * 3600 + now.minute * 60 + now.second;
}

bool isFirstDaySinceStart() {
	return _isFirstDaySinceStart;
}

#else

const char *createTimeStamp () { return F("No time available"); }
void startRTC() {}
void getNow() {}
bool hasDayChanged() { return false; }

#endif // IS_RTC_ENABLED
