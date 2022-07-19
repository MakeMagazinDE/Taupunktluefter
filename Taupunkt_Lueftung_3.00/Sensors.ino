void checkSensors()
{
	MeasurePoint mp = readSensorValues();

	lcd.clear();
	lcd.setCursor(2, 0);
	lcd.print(F("Sensor test"));

	handleErrorResult(
		checkSensorValuesI(mp),
		F("Sensor I"),
		1,
		DHT_PIN_I
	);
	handleErrorResult(
		checkSensorValuesO(mp),
		F("Sensor O"),
		2,
		DHT_PIN_O
	);

	// Display time
	delay(3000);
	wdt_reset();
}

static void handleErrorResult(
	const String &error_,
	const String &sensorName_,
	int lineNumber_,
	int dhtPin_
)
{
	if (error_.length() == 0)
	{
		lcd.setCursor(0, lineNumber_);
		lcd.print(sensorName_);
		lcd.print(F(": OK"));

		#if IS_USB_DEBUG_ENABLED
			Serial.print(sensorName_);
			Serial.println(F(": OK"));
		#endif
		return;
	}

	// Turn fan off.
	digitalWrite(FAN_PIN, FAN_OFF);

	lcd.setCursor(0, lineNumber_);
	lcd.print(sensorName_);
	lcd.print(F(": ERROR"));

	#if IS_USB_DEBUG_ENABLED
		Serial.print(sensorName_);
		Serial.println(F(": ERROR"));
	#endif

	lcd.setCursor(0, lineNumber_ + 2);
	lcd.print(error_);

	// Kind of reset on DHT on read error.
	digitalWrite(dhtPin_, LOW);

	// Display time
	delay(3000);
	wdt_reset();

	// Reenable it so it starts writing, but the input will be ignored,
	// because we wait for the watchdog. Next read after reboot should
	// succeed.
	digitalWrite(dhtPin_, HIGH);

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("Reboot due to error"));

	writeMsgToSD(String(F(";;;;;;;;;reboot error;")));

	// Wait for watchdog.
	while (1)
		;
}

MeasurePoint makeMeasurement()
{
	MeasurePoint mp = readSensorValues();

	int dhtPin = 0;
	String error = checkSensorValuesI(mp);
	if (error.length())
	{
		dhtPin = DHT_PIN_I;
	}
	else
	{
		error = checkSensorValuesO(mp);
		if (error.length())
		{
			dhtPin = DHT_PIN_O;
		}
	}

	if (error.length() == 0)
	{
		wdt_reset();

		// Calculate dew point.
		mp.dewPointI = calcDewPoint(mp.temperatureI, mp.humidityI);
		mp.dewPointO = calcDewPoint(mp.temperatureO, mp.humidityO);

		return mp;
	}

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.println(F("Sensor error: "));
	lcd.print(error);

	String msg(F(";;;;;;;;;Sensor error: "));
	msg.concat(error);
	msg.concat(F(";"));
	writeMsgToSD(msg);

	// Turn fan off.
	digitalWrite(FAN_PIN, FAN_OFF);

	// Kind of reset on DHT on read error.
	digitalWrite(dhtPin, LOW);

	// Display time
	delay(3000);
	wdt_reset();

	// Reenable it so it starts writing, but the input will be ignored,
	// because we wait for the watchdog. Next read after reboot should
	// succeed.
	digitalWrite(dhtPin, HIGH);

	// Wait for Watchdog.
	while(1)
		;
}

static MeasurePoint readSensorValues()
{
	MeasurePoint mp {};

	// Read sensor values.
	mp.humidityI = dhtI.readHumidity() + CORRECTION_H_I;
	mp.temperatureI = dhtI.readTemperature() + CORRECTION_T_I;
	mp.humidityO = dhtA.readHumidity() + CORRECTION_H_O;
	mp.temperatureO = dhtA.readTemperature() + CORRECTION_T_O;

	return mp;
}

static String checkSensorValuesI(const MeasurePoint &mp_)
{
	String error;

	if (isnan(mp_.humidityI))
	{
		error += F("hI=NAN ");
	}
	else if(isnan(mp_.temperatureI))
	{
		error += F("tI=NAN ");
	}
	else
	{
		if (mp_.humidityI > 100)
			error += F("hI>100 ");
		if (mp_.humidityI < 1)
			error += F("hI<1 ");
		if (mp_.temperatureI < -40)
			error += F("tI<-40 ");
		if (mp_.temperatureI > 80)
			error += F("tI>80 ");
	}

	return error;
}

static String checkSensorValuesO(const MeasurePoint &mp_)
{
	String error;

	if (isnan(mp_.humidityO))
	{
		error += F("hO=NAN ");
	}
	else if (isnan(mp_.temperatureO))
	{
		error += F("tO=NAN ");
	}
	else
	{
		if (mp_.humidityO > 100)
			error += F("hO>100 ");
		if (mp_.humidityO < 1)
			error += F("hO<1 ");
		if (mp_.temperatureO < -40)
			error += F("tO<-40 ");
		if (mp_.temperatureO  > 80)
			error += F("tO>80 ");
	}

	return error;
}

static float calcDewPoint(float t, float h)
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
