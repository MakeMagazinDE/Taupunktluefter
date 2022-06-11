//////////////////////////////////////////////////////////////////////////////
// Das Taupunkt-Lüftungssystem
// mit Datenlogging
//
// veröffentlicht in der MAKE 1/2022 und 2/2022
//
// Ulrich Schmerold
// 3/2022
//
//
// Refactored von Manfred Kral
//
// Manfred Kral
// 6/2022
//////////////////////////////////////////////////////////////////////////////
#define Software_version "Version: 3.00"

// Libraries
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>
#include <Ds1302.h>
#include <SD.h>
#include <SPI.h>


// Config
#define CORR_T_I      -1.1   // Korrekturwert Innensensor Temperatur
#define CORR_H_I      -17.0  // Korrekturwert Innensensor Luftfeuchtigkeit
#define CORR_T_O       0.2   // Korrekturwert Außensensor Temperatur
#define CORR_H_O      -5.0   // Korrekturwert Außensensor Luftfeuchtigkeit

#define SWITCH_LIMIT   2.0   // minimaler Taupuntunterschied, bei dem das Relais schaltet
#define HYSTERESE      1.0   // Abstand von Ein- und Ausschaltpunkt
#define TEMP_I_MIN     10.0  // Minimale Innentemperatur, bei der die Lüftung aktiviert wird
#define TEMP_O_MIN    -10.0  // Minimale Außentemperatur, bei der die Lüftung aktiviert wird
#define RING_BUFFER_SIZE 8  // Size of the ring buffer

#define CHANGE_INTERVAL_MINUTES 10

#define Headerzeile F("Date/Time;Temperature T_I;Humidity H_I;Dew point DP_I;Temperature T_O;Humidity H_O;Dew point DP_O;Fan on/off;Fan time;Reboot;")

#define LOG_FILE_NAME     F("fan.csv")
#define LOG_INTERVAL_MIN  10

#define LOGGING_ENABLE    true
#define WRITE_MSG_TO_USB  false

// Uncomment for time setting, bugt also change struct below.
/* #define SET_TIME 1 */
#if SET_TIME
Ds1302::DateTime setTimeStruct = {
	.year = 22,
	.month = Ds1302::MONTH_MAY,
	.day = 31,
	.hour = 19,
	.minute = 52,
	.second = 0,
	.dow = Ds1302::DOW_TUE,
};
#endif


// Hardware
#define RELAIS_ON  HIGH
#define RELAIS_OFF LOW

#define RELAIS_PIN 6
#define DHT_PIN_1  5
#define DHT_PIN_2  4

#define RTC_PIN_ENA 9
#define RTC_PIN_CLK 7
#define RTC_PIN_DAT 8

Ds1302 rtc(RTC_PIN_ENA, RTC_PIN_CLK, RTC_PIN_DAT);

#define DHTTYPE_I  DHT22
#define DHTTYPE_O  DHT22

DHT dhtI(DHT_PIN_1, DHTTYPE_I);
DHT dhtA(DHT_PIN_2, DHTTYPE_O);

LiquidCrystal_I2C lcd(0x27, 20, 4);

// Variables
bool useSerial = WRITE_MSG_TO_USB;
static bool isLoggingEnabled = LOGGING_ENABLE;

typedef struct {
	float temperatureI;
	float temperatureO;
	float humidityI;
	float humidityO;
	float dewPointI;
	float dewPointO;
} MeasurePoint;

static MeasurePoint measurePoints[RING_BUFFER_SIZE] = {};

void setup()
{
	// Enable watchdog with 8s.
	wdt_enable(WDTO_8S);

	// Configure relais pin as output.
	pinMode(RELAIS_PIN, OUTPUT);
	// Turn off relais pin.
	digitalWrite(RELAIS_PIN, RELAIS_OFF);

	// Init LCD.
	lcd.init();
	lcd.backlight();
	lcd.clear();

	// Reset watchdog.
	wdt_reset();

	// Show software version
	lcd.setCursor(0, 1);
	lcd.print(Software_version);
	delay(1000);

	// Logging
	if (isLoggingEnabled == true)
	{
		// Init RTC.
		startRTC();

		if (useSerial)
			Serial.begin(9600);

		// Check SD card. If there is an error, also stop logging.
		isLoggingEnabled = checkSD();
	}

	// Create special char °
	byte Grad[8] = {
		B00111,
		B00101,
		B00111,
		B00000,
		B00000,
		B00000,
		B00000,
		B00000
	};
	lcd.createChar(0, Grad);

	// Create special char |
	byte Strich[8] = {
		B00100,
		B00100,
		B00100,
		B00100,
		B00100,
		B00100,
		B00100,
		B00100
	};
	lcd.createChar(1, Strich);

	// Start sensors
	dhtI.begin();
	dhtA.begin();

	for(size_t i = 0; i < sizeof measurePoints / sizeof measurePoints[0]; i++)
	{
		measurePoints[i] = {
			1000.0,
			1000.0,
			1000.0,
			1000.0,
			1000.0,
			1000.0,
		};
	}
}

void loop()
{
	static size_t measurePointCursor = 0;
	static uint8_t today = 0;
	static Ds1302::DateTime now;
	static bool inited = false;
	static String error("Init");

	rtc.getDateTime(&now);

	// Log reboot. Logging may have changed due to an error.
	if (inited == false && isLoggingEnabled == true)
	{
		File logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
		logFile.print(createTimeStamp());
		logFile.println(F(";;;;;;;;;reboot"));
		logFile.close();
		inited = true;
	}

	// Read sensor values.
	float hI = dhtI.readHumidity() + CORR_H_I;
	float tI = dhtI.readTemperature() + CORR_T_I;
	float hO = dhtA.readHumidity() + CORR_H_O;
	float tO = dhtA.readTemperature() + CORR_T_O;

	// Is also true on the first run.
	for(size_t i = 0; i < 3 && error.length() != 0; i++)
	{
		lcd.clear();
		lcd.setCursor(2, 0);
		lcd.print(F("Sensor test "));
		lcd.print(String(i + 1));
		error = F("");

		if (isnan(hI) || isnan(tI))
		{
			error += F("I: hI=NAN ");
			error += F("I: tI=NAN ");
		}
		else
		{
			if (hI > 100)
				error += F("I: hI>100 ");
			if (hI < 1)
				error += F("I: hI<1 ");
			if (tI < -40)
				error += F("I: tI<-40 ");
			if (tI > 80)
				error += F("I: tI>80 ");
		}

		if (error.length() != 0)
		{
			lcd.setCursor(0, 1);
			lcd.print(F("Sensor I: ERROR"));

			if (useSerial)
				Serial.println(F("Sensor I: ERROR"));

			lcd.setCursor(0, 3);
			lcd.print(error);

			// Display time
			delay(3000);
			wdt_reset();
			continue;
		}
		else
		{
			lcd.setCursor(0, 1);
			lcd.print(F("Sensor I: OK"));

			if (useSerial)
				Serial.println(F("Sensor I: OK"));
		}

		if (isnan(hO) || isnan(tO))
		{
			error += F("O: hO=NAN ");
			error += F("O: tO=NAN ");
		}
		else
		{
			if (hO > 100)
				error += F("O: hO>100 ");
			if (hO < 1)
				error += F("O: hO<1 ");
			if (tO < -40)
				error += F("O: tO<-40 ");
			if (tO  > 80)
				error += F("O: tO>80 ");
		}

		if (error.length() != 0)
		{
			lcd.setCursor(0, 2);
			lcd.print(F("Sensor O: ERROR"));

			if (useSerial)
				Serial.println(F("Sensor O: ERROR"));

			lcd.setCursor(0, 3);
			lcd.print(error);
		}
		else
		{
			lcd.setCursor(0, 2);
			lcd.print(F("Sensor O: OK"));

			if (useSerial)
				Serial.println(F("Sensor O: OK"));
		}

		// Display time
		delay(3000);
		wdt_reset();
	}

	if (error.length() != 0)
	{
		digitalWrite(RELAIS_PIN, RELAIS_OFF); // Relais ausschalten
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print(F("Reboot error:"));
		lcd.setCursor(1, 0);
		lcd.print(error);
		if (isLoggingEnabled == true)
		{
			File logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
			logFile.print(createTimeStamp());
			logFile.print(F(";;;;;;;;;reboot error;"));
			logFile.println(error);
			logFile.close();
			inited = true;
		}

		// Wait for watchdog.
		while (1)
			;
	}

	wdt_reset();

	// Calculate dew point.
	float dewPointI = calcDewPoint(tI, hI);
	float dewPointO = calcDewPoint(tO, hO);

	displayValues(tI, hI, tO, hO, dewPointI, dewPointO);

	measurePoints[measurePointCursor++ % RING_BUFFER_SIZE] = {
		tI,
		tO,
		hI,
		hO,
		dewPointI,
		dewPointO,
	};

	MeasurePoint avgMp = calculateAverage(measurePoints, sizeof measurePoints / sizeof measurePoints[0]);
	displayValuesPageAvg(avgMp);

	static unsigned int fanStartMinutes = 0;            // Start time of fan.
	static unsigned int fanTime = 0;                    // Run time of fan.
	static unsigned int stateStartSeconds = 0;          // Start time of actual state.
	static unsigned int stateTimeSeconds = 0;           // Run time of the actual state.

	static bool isRelaisOn = false;

	if (today != now.day)
		stateStartSeconds = 0;

	unsigned int nowSecs = now.hour * 3600 + now.minute * 60 + now.second;
	stateTimeSeconds += nowSecs - stateStartSeconds;

	if (useSerial)
	{
		Serial.print(F("stateTimeSeconds: "));
		Serial.println(stateTimeSeconds);
		Serial.print(F("nowSecs: "));
		Serial.println(nowSecs);
		Serial.print(F("stateStartSeconds: "));
		Serial.println(stateStartSeconds);
		Serial.print(F("hour: "));
		Serial.print(now.hour);
		Serial.print(F(" min: "));
		Serial.print(now.minute);
		Serial.print(F(" sec: "));
		Serial.println(now.second);
	}

	stateStartSeconds = nowSecs;

	bool isStateOnHold = false;
	if (stateTimeSeconds < CHANGE_INTERVAL_MINUTES * 60)
		isStateOnHold = true;

	float deltaDP = avgMp.dewPointI - avgMp.dewPointO;
	if (isStateOnHold == false)
	{
		if (useSerial)
			Serial.println(F("onHold = false"));
		bool shouldRelaisBeOn = calculateRelaisState(avgMp.temperatureI, avgMp.temperatureO, deltaDP);
		if (shouldRelaisBeOn != isRelaisOn)
		{
			if (useSerial)
				Serial.println(F("state change"));
			stateTimeSeconds = 0;
			stateStartSeconds = now.hour * 3600 + now.minute * 60 + now.second;
		}

		isRelaisOn = shouldRelaisBeOn;
	}

	displayStatPage(deltaDP, stateTimeSeconds / 60, isRelaisOn, isStateOnHold);


	// Calculation for day summary only.
	if (isRelaisOn == true)
	{
		if (fanStartMinutes <= 0)
		{
			fanStartMinutes = now.hour * 60 + now.minute;
		}
	}
	else
	{
		if (fanStartMinutes > 0)
		{
			fanTime += now.hour * 60 + now.minute - fanStartMinutes;
			fanStartMinutes = 0;
		}
	}

	if (isLoggingEnabled == true)
	{
		char buf[sizeof "0.1"];
		String dataLogStr = "";

		dtostrf(avgMp.temperatureI, 2, 1, buf);
		dataLogStr += buf;
		dataLogStr += ';';

		dtostrf(avgMp.humidityI, 2, 1, buf);
		dataLogStr += buf;
		dataLogStr += ';';

		dtostrf(avgMp.dewPointI, 2, 1, buf);
		dataLogStr += buf;
		dataLogStr += ';';

		dtostrf(avgMp.temperatureO, 2, 1, buf);
		dataLogStr += buf;
		dataLogStr += ';';

		dtostrf(avgMp.humidityO, 2, 1, buf);
		dataLogStr += buf;
		dataLogStr += ';';

		dtostrf(avgMp.dewPointO, 2, 1, buf);
		dataLogStr += buf;
		dataLogStr += ';';

		if (isRelaisOn == true)
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
		bool dayChange = false;
		if (today != now.day)
		{
			dayChange = true;

			// On first run today is '0', and we don't have to log
			// fan time.
			if (today)
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
			today = now.day;
		}
		else
		{
			// No day change.
			strcpy(fanTimeStr, "0");
		}

		dataLogStr += fanTimeStr;
		dataLogStr += ';';
		dataLogStr += '0';
		dataLogStr += ';';

		saveToSD(dataLogStr, dayChange || stateTimeSeconds == 0);
	}

	if (isRelaisOn == true)
	{
		digitalWrite(RELAIS_PIN, RELAIS_ON); // Relais einschalten
	}
	else
	{
		digitalWrite(RELAIS_PIN, RELAIS_OFF); // Relais ausschalten
	}
}


float calcDewPoint(float t, float h)
{
	float a = 0.0;
	float b = 0.0;

	if (t >= 0)
	{
		a = 7.5;
		b = 237.3;
	}
	else if (t < 0)
	{
		a = 7.6;
		b = 240.7;
	}

	// Sättigungsdampfdruck in hPa
	float sdd = 6.1078 * pow(10, (a * t) / (b + t));

	// Steam preassure in hPa
	float dd = sdd * (h / 100);

	// v-parameter
	float v = log10(dd / 6.1078);

	// Dew point temperature (°C)
	return (b * v) / (a - v);
}

// Restart program, but not sensors or lcd.
void restartProgram()
{
	asm volatile ("  jmp 0");
}

void displayValues(
	float tI, float hI,
	float tO, float hO,
	float dewPointI, float dewPointO
)
{
	// Display values to LCD.
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("TI: "));
	lcd.print(tI);
	lcd.write((uint8_t)0); // Special char °
	lcd.write(('C'));
	lcd.write((uint8_t)1); // Special char |
	lcd.print(hI);
	lcd.print(F(" %"));

	lcd.setCursor(0, 1);
	lcd.print(F("TO: "));
	lcd.print(tO);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));
	lcd.write((uint8_t)1); // Special char |
	lcd.print(hO);
	lcd.print(F(" %"));

	lcd.setCursor(0, 2);
	lcd.print(F("DpI: "));
	lcd.print(dewPointI);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));

	lcd.setCursor(0, 3);
	lcd.print(F("DpO: "));
	lcd.print(dewPointO);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));

	delay(5000);    // Zeit um das Display zu lesen
	wdt_reset();    // Watchdog zurücksetzen

	lcd.clear();
}

MeasurePoint calculateAverage(const MeasurePoint *measurePoints, size_t cnt)
{
	MeasurePoint measurePoint = {};

	size_t i = 0;
	for (i = 0; i < cnt; i++)
	{
		if (       measurePoints[i].temperatureI > 500
			|| measurePoints[i].temperatureO > 500
			|| measurePoints[i].humidityI > 500
			|| measurePoints[i].humidityO > 500
			|| measurePoints[i].dewPointI > 500
			|| measurePoints[i].dewPointO > 500
		)
			break;

		/* if (useSerial) */
		/* { */
		/* 	Serial.print(F("i: ")); */
		/* 	Serial.println(i); */
		/* 	Serial.print(F("tI: ")); */
		/* 	Serial.println(measurePoints[i].temperatureI); */
		/* 	Serial.print(F("tO: ")); */
		/* 	Serial.println(measurePoints[i].temperatureO); */
		/* 	Serial.print(F("hI: ")); */
		/* 	Serial.println(measurePoints[i].humidityI); */
		/* 	Serial.print(F("hO: ")); */
		/* 	Serial.println(measurePoints[i].humidityO); */
		/* 	Serial.print(F("dI: ")); */
		/* 	Serial.println(measurePoints[i].dewPointI); */
		/* 	Serial.print(F("dO: ")); */
		/* 	Serial.println(measurePoints[i].dewPointO); */
		/* } */

		measurePoint.temperatureI += measurePoints[i].temperatureI;
		measurePoint.temperatureO += measurePoints[i].temperatureO;
		measurePoint.humidityI += measurePoints[i].humidityI;
		measurePoint.humidityO += measurePoints[i].humidityO;
		measurePoint.dewPointI += measurePoints[i].dewPointI;
		measurePoint.dewPointO += measurePoints[i].dewPointO;
	}

	if (i == 0)
		return measurePoint;

	measurePoint.temperatureI = measurePoint.temperatureI / i;
	measurePoint.temperatureO = measurePoint.temperatureO / i;
	measurePoint.humidityI = measurePoint.humidityI / i;
	measurePoint.humidityO = measurePoint.humidityO / i;
	measurePoint.dewPointI = measurePoint.dewPointI / i;
	measurePoint.dewPointO = measurePoint.dewPointO / i;

	return measurePoint;
}

void displayValuesPageAvg(const MeasurePoint measurePoint)
{
	// Display values to LCD.
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("TI: "));
	lcd.print(measurePoint.temperatureI);
	lcd.write((uint8_t)0); // Special char °
	lcd.write(('C'));
	lcd.write((uint8_t)1); // Special char |
	lcd.print(measurePoint.humidityI);
	lcd.print(F(" %"));

	lcd.setCursor(0, 1);
	lcd.print(F("TO: "));
	lcd.print(measurePoint.temperatureO);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));
	lcd.write((uint8_t)1); // Special char |
	lcd.print(measurePoint.humidityO);
	lcd.print(F(" %"));

	lcd.setCursor(0, 2);
	lcd.print(F("DpI: "));
	lcd.print(measurePoint.dewPointI);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));

	lcd.setCursor(0, 3);
	lcd.print(F("DpO: "));
	lcd.print(measurePoint.dewPointO);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));

	lcd.setCursor(17, 3);
	lcd.print(F("AVG"));


	delay(5000);    // Zeit um das Display zu lesen
	wdt_reset();    // Watchdog zurücksetzen
}

void displayStatPage(float deltaDP, unsigned int stateTimeMinutes, bool isRelaisOn, bool isStateOnHold)
{
	lcd.clear();
	lcd.setCursor(0, 0);

	if (isRelaisOn)
	{
		lcd.print(F("Fan ON "));

		if (useSerial)
			Serial.println(F("Fan ON"));
	}
	else
	{
		lcd.print(F("Fan OFF "));

		if (useSerial)
			Serial.println(F("Fan OFF"));
	}

	lcd.print("T: ");

	char buf[sizeof "999999"] = "";
	snprintf(buf, sizeof buf, "%u", stateTimeMinutes);
	lcd.print(String(buf));

	lcd.print(" ");
	lcd.print(isStateOnHold ? "H" : "-");

	lcd.setCursor(0, 1);
	lcd.print(F("Delta DP: "));
	lcd.print(deltaDP);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write('C');

	lcd.setCursor(0, 2);
	if (isLoggingEnabled == true)
		lcd.print(F("Logging ON"));
	else
		lcd.print(F("Logging OFF"));

	lcd.setCursor(0, 3);
	lcd.print(createTimeStamp());

	delay(4000);    // Wartezeit zwischen zwei Messungen
	wdt_reset();    // Watchdog zurücksetzen
}

bool calculateRelaisState(float tI, float tO, float deltaDP)
{
	if (SWITCH_LIMIT + HYSTERESE < deltaDP)
		return true;
	if (deltaDP < SWITCH_LIMIT)
		return false;
	if (tI < TEMP_I_MIN )
		return false;
	if (tO < TEMP_O_MIN )
		return false;
}
