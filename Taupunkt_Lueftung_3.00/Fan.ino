float calculateFanState(
	const MeasurePoint &measurePoint_,
	bool *isFanOn_,
	bool *hasStateChanged_,
	bool *isStateOnHold_,
	unsigned int *stateTimeSeconds_
)
{
	float deltaDP = measurePoint_.dewPointI - measurePoint_.dewPointO;

#if IS_RTC_ENABLED
	static unsigned int stateStartSeconds = 0; // Start time of actual state.

	if (hasDayChanged())
		stateStartSeconds = 0;

	unsigned int nowSecs = getSecondsOfDay();
	*stateTimeSeconds_ += nowSecs - stateStartSeconds;

	#if IS_USB_DEBUG_ENABLED
		Serial.print(F("stateTimeSeconds: "));
		Serial.println(*stateTimeSeconds_);
		Serial.print(F("nowSecs: "));
		Serial.println(nowSecs);
		Serial.print(F("stateStartSeconds: "));
		Serial.println(stateStartSeconds);
		Serial.print(F(" min: "));
		Serial.print(getMinutesOfDay());
		Serial.print(F(" sec: "));
		Serial.println(getSecondsOfDay());
	#endif

	stateStartSeconds = nowSecs;

	*isStateOnHold_ = false;
	if (*stateTimeSeconds_ < MIN_STATE_CHANGE_INTERVAL_MINUTES * 60)
		*isStateOnHold_ = true;

	if (*isStateOnHold_ == true)
	{
		return deltaDP;
	}

	#if IS_USB_DEBUG_ENABLED
		Serial.println(F("onHold = false"));
	#endif
#endif // IS_RTC_ENABLED

	bool shouldFanBeOn = calculateFanState(
		measurePoint_.temperatureI,
		measurePoint_.temperatureO,
		deltaDP
	);

	if (shouldFanBeOn == *isFanOn_)
	{
		return deltaDP;
	}

	#if IS_USB_DEBUG_ENABLED
		Serial.print(F("state change "));
		Serial.print(*isFanOn_);
		Serial.print(F(" -> "));
		Serial.println(shouldFanBeOn);
	#endif

	*isFanOn_ = shouldFanBeOn;
	*hasStateChanged_ = true;

#if RTC_IS_ENABLED
	*stateTimeSeconds_ = 0;
	stateStartSeconds = getSecondsOfDay();
#endif // RTC_IS_ENABLED

	return deltaDP;
}

static bool calculateFanState(float tI_, float tO_, float deltaDP_)
{
	if (SWITCH_OFF_LIMIT + HYSTERESE < deltaDP_)
		return true;
	if (deltaDP_ < SWITCH_OFF_LIMIT)
		return false;
	if (tI_ < TEMP_I_MIN )
		return false;
	if (tO_ < TEMP_O_MIN )
		return false;
}
