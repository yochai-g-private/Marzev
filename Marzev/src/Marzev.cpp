/*
 Name:		Marzev.ino
 Created:	11/11/2019 6:11:16 PM
 Author:	YLA
*/

#define DRY_TEST	0

#if DRY_TEST	
	#define DELAY_MINUTES	1
	#define WATER_SENSOR_REVERSED	true
#else
	#define DELAY_MINUTES	5
	#define WATER_SENSOR_REVERSED	false
#endif

#include "NYG.h"
//?#include "Interface.h"

#include "Timer.h"
#include "Toggler.h"
#include "RedGreenLed.h"
#include "IInput.h"
#include "StableInput.h"
#include "IOutput.h"
#include "Observer.h"
//#include "Scheduler.h"
#include "MicroController.h"
#if _USE_EEPROM_JURNAL
#include "EepromJurnal.h"
#endif //_USE_EEPROM_JURNAL

using namespace NYG;

//-----------------------------------------------------
//	Pin numbers
//-----------------------------------------------------

enum PIN_NUMBERS
{
	TOP_LEVEL_PIN_1		= D4,
	TOP_LEVEL_PIN_2		= D3,
	TOP_LEVEL_PIN_3		= D2,
	//======================
	BOTTOM_LEVEL_PIN_1	= A0,
	BOTTOM_LEVEL_PIN_2	= A1,
	BOTTOM_LEVEL_PIN_3	= A2,
	//======================
	BUZZER_PIN			= D5,
	WARNING_LED_PIN		= D6,
	GREEN_LED_PIN		= D7,
	RED_LED_PIN			= D8,
	//======================
	POMP_RELAY_PIN		= D9,
	// SPI (MicroSD)	  ICSP pins: D11, D12, D13, D10
	// I2C (RTC)		  SDA=A4,  SCL=A5
};

//-----------------------------------------------------
//	WaterLevelSensor class
//-----------------------------------------------------

class WaterLevelSensor
{
public:

	#define WSR						WATER_SENSOR_REVERSED

	WaterLevelSensor(Pin pin) : sensor(pin, WSR), stable(sensor), observer(stable)	{	}

	bool TestChanged(bool& value)
	{
		return observer.TestChanged(value);
	}

private:

	DigitalPullupInputPin	sensor;
	typedef StableDigitalInput<500, 500, millis>	BouncingSensor;
	BouncingSensor			stable;
	DigitalObserver			observer;
};

//-----------------------------------------------------
//	WaterLevelSensorsBucket class
//-----------------------------------------------------

class WaterLevelSensorsBucket : public IDigitalInput
{
public:

	WaterLevelSensorsBucket(bool is_top, Pin pin1, Pin pin2, Pin pin3) :
		name(is_top ? "T" : "B"),
		s1(pin1), s2(pin2), s3(pin3)
	{
	}

	bool Get()
	{
		#define CHECK_SENSOR_CHANGED(n)	\
			bool v##n;\
			if (s##n.TestChanged(v##n))	LOGGER << name << "s" #n " is " << v##n << NL

		CHECK_SENSOR_CHANGED(1);
		CHECK_SENSOR_CHANGED(2);
		CHECK_SENSOR_CHANGED(3);

		#undef CHECK_SENSOR_CHANGED

		count = (int)v1 + (int)v2 + (int)v3;
		return count >= 2;
	}

	uint8_t GetCount()	const { return count; }

private:

	const char*				name;
	WaterLevelSensor		s1, s2, s3;
	uint8_t					count;

};

//-----------------------------------------------------
//	INPUT elements
//-----------------------------------------------------

static WaterLevelSensorsBucket	_bottom_level_sensor(false,	BOTTOM_LEVEL_PIN_1,		BOTTOM_LEVEL_PIN_2,		BOTTOM_LEVEL_PIN_3),
								_top_level_sensor	(true,	TOP_LEVEL_PIN_1,		TOP_LEVEL_PIN_2,		TOP_LEVEL_PIN_3);

typedef StableDigitalInput<5000, 1000, millis>	LevelInput;

static LevelInput				_bottom_level(_bottom_level_sensor),
								_top_level	 (_top_level_sensor);

static DigitalObserver			bottom_level_observer(_bottom_level),
								top_level_observer	 (_top_level);

//-----------------------------------------------------
//	OUTPUT elements
//-----------------------------------------------------

static DigitalOutputPin			buzzer(BUZZER_PIN);
static DigitalOutputPin			warning_led(WARNING_LED_PIN);
static RedGreenLed				led(RED_LED_PIN, GREEN_LED_PIN);
static DigitalOutputPin			pump(POMP_RELAY_PIN);

static Toggler					warning_toggler;	// used to toggle the warning led

//-----------------------------------------------------
//	Other...
//-----------------------------------------------------

static Timer					pumping_timer;		// used to start or stop the pump
static Timer					drain_timer;		// used to drain after the pump was stopped
unsigned long					drain_timer_delay_hours = 0;
static Toggler					toggler;			// used to show the pumping state, as follows: 
													// STOPPED			: GREEN led			freq.=300/15000 ms
													// DELAYED			: RED led			freq.=500/10000 ms
													// PUMPING			: pumping_signal	(RED led + buzzer)
													//						Bottom	:		freq.=1/5 s
													//						Top		:		freq.=1/2 s
													// SENSOR FAILURE	: pumping_signal	freq.=500 ms, NOT PUMPING

class Pumping : public IDigitalOutput
{
	bool Set(bool value)
	{
		if(value)		led.SetRed();
		else            led.SetOff();
		return buzzer.Set(value);
	}

	bool Get()	const
	{
		return buzzer.Get();
	}
}	pumping_signal;

#if !_USE_MICRO_SD
DigitalOutputPin			BuiltinLed(LED_BUILTIN);
Toggler						BuiltinLedToggler;
#endif

//-----------------------------------------------------
//	PREDECLARATIONs
//-----------------------------------------------------

#if _USE_MICRO_SD
static void open_log_file();
static void restart(void* ctx);
static bool micro_sd_OK;
#endif //_USE_MICRO_SD
static void toggle_warning(bool& warning, const char* message);
static void start_drain_timer();
static bool start_pumping(bool force = false);
static void stop_pumping();
static void drain();
static void delay_pumping();
static bool start_pumping(bool force = false);
static void treat_serial_input();
static void blink_green();
static void set_next_drain_timer();
//--------------------------------------------------------------
void setup()
{
	Logger::Initialize();

#if _USE_RTC
	if (!RTC::Begin())
	{
		LOGGER << "RTC init failed" << NL;

		bool RTC_initialization_failed = false;

		toggle_warning(RTC_initialization_failed, NULL);
		bool OK = RTC::SetFromSerial(0);

		if (OK)
		{
			toggle_warning(RTC_initialization_failed, NULL);
			MicroController::Restart();
		}
		else
		{
			LOGGER << "RTC not init." << NL;
			setTimeFromBuildTime();
		}
	}
#else
//	setTimeFromBuildTime();
#endif //_USE_RTC

#if _USE_EEPROM_JURNAL
    EepromJurnalWriter::Begin();
//	EepromJurnalWriter::Clean();
	EepromJurnalReader::PrintOut();

//	LOGGER << NL;

	EepromJurnalWriter::Write("==============");
	EepromJurnalWriter::SetLoggerAux();
#endif //_USE_EEPROM_JURNAL
    
//#if _USE_MICRO_SD
//	LOGGER << "MicroSD #1: " << MicroController::GetAvailableMemory() << NL;
//	micro_sd_OK = MicroSD::Begin();
//	LOGGER << "MicroSD #2: " << MicroController::GetAvailableMemory() << NL;
//
//	if (micro_sd_OK)
//	{
//		open_log_file();
//		LOGGER << "MicroSD #3: " << MicroController::GetAvailableMemory() << NL;
//		Scheduler::AddInSeconds(restart, NULL, NULL, 0);
//	}
//	else
//	{
//		bool micro_sd_FAILED = false;
//		toggle_warning(micro_sd_FAILED, "MicroSD failed");
//	}
//#endif //_USE_MICRO_SD

	buzzer.On();		delay(500);			buzzer.Off();
	led.SetGreen();		delay(2000);		led.SetOff();
	led.SetRed();		delay(2000);		led.SetOff();
	//pump.On();		delay(5000);		pump.Off();

	pump.Off();

	drain_timer.StartOnce(1, MINS);

	blink_green();

#if !_USE_MICRO_SD
	BuiltinLedToggler.StartOnOff(BuiltinLed, 1000);
#endif

	LOGGER << "Ready" << NL;
}
//--------------------------------------------------------------
static void blink_green()
{
	led.SetOff();
	toggler.Start(led.GetGreen(), Toggler::OnTotal(300, 15000));
}
//--------------------------------------------------------------
static void drain()
{
	if (pump.IsOff())
	{
		LOGGER << "Start drain" << NL;
		pump.On();
		enum { DRAIN_SECONDS = 10 };
		drain_timer.StartOnce(DRAIN_SECONDS, SECS);
	}
	else
	{
		pump.Off();
		LOGGER << "Stop drain" << NL;

		if (drain_timer_delay_hours)
		{
			if (drain_timer_delay_hours == HOURS_PER_DAY)
			{
				drain_timer_delay_hours = 0;
			}
			else
			{
				drain_timer_delay_hours = HOURS_PER_DAY;
				set_next_drain_timer();
			}
		}

		blink_green();
	}
}
//--------------------------------------------------------------
static void cancel_drain()
{
	bool drain_timer_started = drain_timer.IsStarted();

	if (!drain_timer_started)
		return;

	drain_timer.Stop();

	if (pump.IsOn())
	{
		pump.Off();
		LOGGER << "Drain pumping canceled" << NL;
	}
	else
	{
		LOGGER << "Drain timer stopped" << NL;
	}
}
//--------------------------------------------------------------
void loop()
{
#if !_USE_MICRO_SD
	BuiltinLedToggler.Toggle();
#endif

	delay(10);

	//Scheduler::Proceed();
	if (Serial.available())
	{
		treat_serial_input();
	}

	if (drain_timer.Test())
	{
		drain();
	}

	toggler.Toggle();
	warning_toggler.Toggle();

	// First get inputs
	bool bottom_overflow, bottom_overflow_changed,
		 top_overflow,	  top_overflow_changed;

	bottom_overflow = bottom_level_observer.Get(&bottom_overflow_changed);
	top_overflow	= top_level_observer.   Get(&top_overflow_changed);

	if (bottom_overflow_changed)
		LOGGER << "BL: " << bottom_overflow << NL;

	if (top_overflow_changed)
		LOGGER << "TL: " << top_overflow << NL;

	bool overflow_changed = bottom_overflow_changed || top_overflow_changed;

	// Check sensors
	{
		uint8_t bottom_level_sensor_count = _bottom_level_sensor.GetCount();
		uint8_t top_level_sensor_count	  = _top_level_sensor.	 GetCount();

		static bool bad_sensors = false;

		if (top_level_sensor_count && (bottom_level_sensor_count < 3))
		{
			if (!bad_sensors)
			{
				toggle_warning(bad_sensors, "BAD sensors");
				bad_sensors = true;
			}
		}
		else
		{
			if (bad_sensors)
			{
				toggle_warning(bad_sensors, "Sensors OK");
				bad_sensors = false;
			}
		}
	}

	static bool first_time = true;

	if (!(first_time || overflow_changed))
	{
		// No input changes, so
		// Check timers
		bool change_pumping_state = pumping_timer.Test();

		if (change_pumping_state)
		{
			if (pump.IsOn())
			{
				if (bottom_overflow)
				{
					delay_pumping();
				}
				else
				{
					stop_pumping();
				}
			}
			else
			{
				start_pumping();
			}
		}

		return;
	}

	first_time = false;

	// Apply changes
	int overflow_state = (int)bottom_overflow + ((int)top_overflow * 2);

	switch (overflow_state)
	{
		case 0 :
		{
			stop_pumping();
			break;
		}

		case 1:
		{
			// BOTTOM level overflow, TOP level is dry

			if (bottom_overflow_changed)
			{
				// The water level goes up
				delay_pumping();
			}
			else
			{
				// The water level goes down
				start_pumping();
			}

			break;
		}

		case 2:
		{
			// TOP level overflow, BOTTOM level is dry
			// Discrepancy
			stop_pumping();
			break;
		}

		case 3:
		{
			start_pumping();
			break;
		}
	}
}
//--------------------------------------------------------------
static void stop_pumping()
{
	bool is_pumping = pump.IsOn();
	bool pumping_timer_is_started = pumping_timer.IsStarted();

	pump.Off();
	led.SetOff();
	pumping_timer.Stop();

	if (!bottom_level_observer.GetValue() && top_level_observer.GetValue())
	{
		LOGGER << "Sensor failure" << NL;

		//if (_bottom_level_sensor.GetCount()) <<<< cannot be true when !bottom_level_observer.GetValue()
		//{
		//	delay(1000);
		//	start_pumping(true);
		//	return;
		//}

		toggler.StartOnOff(pumping_signal, 500);
	}
	else
	{
		if(is_pumping)
			LOGGER << "Pumping stopped" << NL;

		blink_green();
		start_drain_timer();
	}
}
//--------------------------------------------------------------
static void set_next_drain_timer()
{
	LOGGER << "Next drain in about " << drain_timer_delay_hours << " hour" << ((drain_timer_delay_hours == 1) ? "" : "s") << NL;
	drain_timer.StartOnce(drain_timer_delay_hours, HOURS);
}
//--------------------------------------------------------------
static void start_drain_timer()
{
	if (drain_timer.IsStarted())
		return;

	drain_timer_delay_hours = 1;
	set_next_drain_timer();
}
//--------------------------------------------------------------
static void delay_pumping()
{
	unsigned long delay_minutes = DELAY_MINUTES;

	led.SetOff();

	if (pump.IsOff())
	{
		LOGGER << "Pumping delay " << delay_minutes << "m" << NL;
	}
	else
	{
		pump.Off();
		LOGGER << "Pumping paused for " << delay_minutes << "m" << NL;
	}

	cancel_drain();

	pumping_timer.Stop();

	pumping_timer.StartOnce(delay_minutes, MINS);

	toggler.Start(led.GetRed(), Toggler::OnTotal(500, 10000));
}
//--------------------------------------------------------------
static bool start_pumping(bool force = false)
{
	cancel_drain();

	if (!bottom_level_observer.GetValue())
		return false;

	pump.On();
	led.SetOff();

	pumping_timer.Stop();

	if (top_level_observer.GetValue())
	{
		LOGGER << "Start pumping" << NL;
		toggler.StartOnOff(pumping_signal, 1, SECS);
	}
	else
	{
#if DRY_TEST
		unsigned long delay_seconds = 5;

		LOGGER << "Start pumping for max. " << delay_seconds << "s" << NL;

		pumping_timer.StartOnce(delay_seconds, SECS);
		toggler.StartOnOff(pumping_signal, 1, SECS);
#else
		unsigned long delay_minutes = DELAY_MINUTES;

		LOGGER << "Start pumping for max. " << delay_minutes << "m" << NL;

		pumping_timer.StartOnce(delay_minutes, MINS);
		toggler.Start(pumping_signal, Toggler::OnTotal(1, 5), SECS);
#endif
	}

	return true;
}
//--------------------------------------------------------------
#if _USE_MICRO_SD
static void open_log_file()
{
	//LOGGER << "open_log_file #" << __LINE__ << NL;
	int current_year = year();
	char fn[13];
	sprintf(fn, "%d.log", current_year);
	
	//LOGGER << "open_log_file #" << __LINE__ << NL;
	//LOGGER << "open_log_file " << fn << NL;
	if (!MicroSD::OpenLogFile(fn))
	{
		//LOGGER << "open_log_file #" << __LINE__ << NL;
		if (micro_sd_OK)
		{
			bool micro_sd_FAILED = false;
			toggle_warning(micro_sd_FAILED, NULL);
			micro_sd_OK = false;
		}

		return;
	}
}
//--------------------------------------------------------------
static void restart(void* ctx)
{
	MicroController::Restart();
}
#endif //_USE_MICRO_SD
//--------------------------------------------------------------
static void toggle_warning(bool& warning, const char* message)
{
	static int warnings_count = 0;
	
	warning = !warning;
	warnings_count += (warning) ? 1 : -1;

	warning_toggler.Stop();

	if(message)
		LOGGER << F("WARN: ") << message << NL;

	if(!warnings_count)
		return;

	unsigned long*	on_off_array;
	uint16_t		on_off_array_count;

	switch (warnings_count)
	{
		case 1:
		{
			static unsigned long a[] = { 500, 4500 };
			on_off_array = a;
			on_off_array_count = countof(a);

			break;
		}

		case 2:
		{
			static unsigned long a[] = { 400, 400, 400, 3800 };
			on_off_array = a;
			on_off_array_count = countof(a);

			break;
		}

		case 3:
		{
			static unsigned long a[] = { 300, 300, 300, 300, 300, 3500 };
			on_off_array = a;
			on_off_array_count = countof(a);

			break;
		}

		default:
		{
			static unsigned long a[] = { 300, 300 };
			on_off_array = a;
			on_off_array_count = countof(a);

			break;
		}

	}

	warning_toggler.Start(warning_led, on_off_array, on_off_array_count);
}
//--------------------------------------------------------------
static void treat_serial_input()
{
	String s = Serial.readString();

	LOGGER << "Command '" << s << "' got from serial" << NL;

#if _USE_RTC
    if (RTC::SetFromSerial(s))
		return;
#endif //_USE_RTC

	if (s == "RESTART")
	{
		MicroController::Restart();
		return;
	}

	if (s == "PUMP")
	{
		pump.On();
		return;
	}

	if (s == "NOPUMP")
	{
		pump.Off();
		return;
	}

	Serial.println("Unknown command");
}
//--------------------------------------------------------------
//extern const char* gbl_build_date = __DATE__;
//extern const char* gbl_build_time = __TIME__;

