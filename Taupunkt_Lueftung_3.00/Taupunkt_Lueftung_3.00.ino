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
#define IS_RTC_ENABLED        1
#define IS_LOGGING_ENABLED    1
#define IS_USB_DEBUG_ENABLED  0

#if IS_LOGGING_ENABLBED && IS_RTC_ENABLED == 0
# error "Logging is enabled, but RTC is disabled!"
#endif


#define CORRECTION_T_I       -1.1   // Korrekturwert Innensensor Temperatur
#define CORRECTION_H_I       -17.0  // Korrekturwert Innensensor Luftfeuchtigkeit
#define CORRECTION_T_O        0.2   // Korrekturwert Außensensor Temperatur
#define CORRECTION_H_O       -5.0   // Korrekturwert Außensensor Luftfeuchtigkeit

#define SWITCH_OFF_LIMIT      2.0   // Minimaler Taupuntunterschied, bei dem das Relais ausschaltet.
#define HYSTERESE             1.0   // Minimaler Unterschied + Hysterese ergibt den Einschaltpunkt.
#define TEMP_I_MIN            10.0  // Minimale Innentemperatur, bei der die Lüftung nicht mehr aktiviert wird.
#define TEMP_O_MIN           -10.0  // Minimale Außentemperatur, bei der die Lüftung nicht mehr aktiviert wird.

#define RING_BUFFER_SIZE      8     // Size of the ring buffer.

#define MIN_STATE_CHANGE_INTERVAL_MINUTES 10 // Minimal time in minutes, when the state of the fan is changed.


// Uncomment for time setting. ATTENTION: Also change struct below!
/* #define SET_TIME 1 */
#if SET_TIME && IS_RTC_ENABLED
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


// Hardware Config (Config of RTC and SD is in corresponding ino-Files.
#define FAN_ON  HIGH
#define FAN_OFF LOW

#define FAN_PIN 6
#define DHT_PIN_I  5
#define DHT_PIN_O  4

#define DHTTYPE_I  DHT22
#define DHTTYPE_O  DHT22

DHT dhtI(DHT_PIN_I, DHTTYPE_I);
DHT dhtA(DHT_PIN_O, DHTTYPE_O);


LiquidCrystal_I2C lcd(0x27, 20, 4);

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

	// Configure fan pin as output.
	pinMode(FAN_PIN, OUTPUT);

	// Turn off fan pin.
	digitalWrite(FAN_PIN, FAN_OFF);

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

	// Init RTC.
	startRTC();

	// Check and show state of SD Card.
	checkSD();

	#if IS_USB_DEBUG_ENABLED
		Serial.begin(9600);
	#endif

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
	static bool inited = false;
	static String error("Init");

	getNow();

	// Log reboot. Logging may have changed due to an error.
	if (inited == false)
	{
		writeMsgToSD(F(";;;;;;;;;reboot"));
		inited = true;

		checkSensors();
	}

	MeasurePoint mp = makeMeasurement();

	displaySensorValues(mp);

	measurePoints[measurePointCursor++ % RING_BUFFER_SIZE] = mp;

	MeasurePoint avgMp = calculateAverage(
		measurePoints,
		sizeof measurePoints / sizeof measurePoints[0]
	);
	displayValuesPageAvg(avgMp);

	static unsigned int stateTimeSeconds = 0;  // Run time of the actual state.
	static bool isFanOn = false;
	bool hasStateChanged = false;
	bool isStateOnHold = false;
	float deltaDP = calculateFanState(
		avgMp,
		&isFanOn,
		&hasStateChanged,
		&isStateOnHold,
		&stateTimeSeconds
	);

	displayStatPage(deltaDP, stateTimeSeconds / 60, isFanOn, isStateOnHold);

	logValuesToSD(avgMp, isFanOn, hasStateChanged);

	if (isFanOn == true)
	{
		digitalWrite(FAN_PIN, FAN_ON); // Turn fan on
	}
	else
	{
		digitalWrite(FAN_PIN, FAN_OFF); // Turn fan off
	}
}


// Restart program, but not sensors or lcd.
void restartProgram()
{
	asm volatile ("  jmp 0");
}

void displaySensorValues(const MeasurePoint &mp_)
{
	// Display values to LCD.
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("TI: "));
	lcd.print(mp_.temperatureI);
	lcd.write((uint8_t)0); // Special char °
	lcd.write(('C'));
	lcd.write((uint8_t)1); // Special char |
	lcd.print(mp_.humidityI);
	lcd.print(F(" %"));

	lcd.setCursor(0, 1);
	lcd.print(F("TO: "));
	lcd.print(mp_.temperatureO);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));
	lcd.write((uint8_t)1); // Special char |
	lcd.print(mp_.humidityO);
	lcd.print(F(" %"));

	lcd.setCursor(0, 2);
	lcd.print(F("DpI: "));
	lcd.print(mp_.dewPointI);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));

	lcd.setCursor(0, 3);
	lcd.print(F("DpO: "));
	lcd.print(mp_.dewPointO);
	lcd.write((uint8_t)0); // Special char °C
	lcd.write(('C'));

	delay(5000);    // Zeit um das Display zu lesen
	wdt_reset();    // Watchdog zurücksetzen

	lcd.clear();
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

void displayStatPage(float deltaDP, unsigned int stateTimeMinutes, bool isFanOn, bool isStateOnHold)
{
	lcd.clear();
	lcd.setCursor(0, 0);

	if (isFanOn)
	{
		lcd.print(F("Fan ON "));

		#if IS_USB_DEBUG_ENABLED
			Serial.println(F("Fan ON"));
		#endif
	}
	else
	{
		lcd.print(F("Fan OFF "));

		#if IS_USB_DEBUG_ENABLED
			Serial.println(F("Fan OFF"));
		#endif
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
#if IS_LOGGING_ENABLED
	lcd.print(F("Logging ON"));
#else
	lcd.print(F("Logging OFF"));
#endif

	lcd.setCursor(0, 3);
	lcd.print(createTimeStamp());

	delay(4000);    // Wartezeit zwischen zwei Messungen
	wdt_reset();    // Watchdog zurücksetzen
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
