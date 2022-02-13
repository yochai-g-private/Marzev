/*
 Name:		main.cpp
 Created:	13/02/2022
 Author:	YLA
*/

#include "main.h"

State gbl_state = { 
        STARTUP, 
        NOT_SIGNALED,
        false };

#include "RedGreenLed.h"
#include "IOutput.h"
#include "Toggler.h"

#if 0
#include "Timer.h"
#include "MicroController.h"
#endif

//-----------------------------------------------------
//	OUTPUT elements
//-----------------------------------------------------

static DigitalOutputPin			buzzer(BUZZER_PIN);
static DigitalOutputPin			warning_led(WARNING_LED_PIN);
static RedGreenLed				led(RED_LED_PIN, GREEN_LED_PIN);
static DigitalOutputPin			pump(POMP_RELAY_PIN);

//static Toggler					warning_toggler;	// used to toggle the warning led

//-----------------------------------------------------
//	Other...
//-----------------------------------------------------

#if 0
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
#endif

DigitalOutputPin			BuiltinLed(LED_BUILTIN);
Toggler						BuiltinLedToggler;

//-----------------------------------------------------
//	PREDECLARATIONs
//-----------------------------------------------------

static void treat_serial_input();
static void on_STARTUP();
static void on_DRY();
static void on_BOTTOM_LEVEL_SIGNALED();
static void on_TOP_LEVEL_SIGNALED();
static void on_DRAIN();

typedef void (*StateFunc)();

static StateFunc    state_funcs[] = { on_STARTUP,on_DRY, on_BOTTOM_LEVEL_SIGNALED,on_TOP_LEVEL_SIGNALED, on_DRAIN };

/*
static void toggle_warning(bool& warning, const char* message);
static void start_drain_timer();
static bool start_pumping(bool force = false);
static void stop_pumping();
static void drain();
static void delay_pumping();
static bool start_pumping(bool force = false);
static void blink_green();
static void set_next_drain_timer();
*/
//--------------------------------------------------------------
void setup()
{
	Logger::Initialize();

	buzzer.On();		delay(500);			buzzer.Off();
    warning_led.On();   delay(2000);        warning_led.Off();
	led.SetGreen();		delay(2000);		led.SetOff();
	led.SetRed();		delay(2000);		led.SetOff();

	pump.Off();

	//drain_timer.StartOnce(1, MINS);

	//blink_green();

    UpdateSensorsState();
	BuiltinLedToggler.StartOnOff(BuiltinLed, 1000);

	LOGGER << "Ready" << NL;
}
//--------------------------------------------------------------
void loop()
{
	BuiltinLedToggler.Toggle();

	if (Serial.available())
		treat_serial_input();

    state_funcs[gbl_state.state_id]();
}
static void treat_serial_input()
{

}
//--------------------------------------------------------------
static void on_STARTUP()
{

}
//--------------------------------------------------------------
static void on_DRY()
{

}
//--------------------------------------------------------------
static void on_BOTTOM_LEVEL_SIGNALED()
{

}
//--------------------------------------------------------------
static void on_TOP_LEVEL_SIGNALED()
{

}
//--------------------------------------------------------------
static void on_DRAIN()
{

}
//--------------------------------------------------------------
//--------------------------------------------------------------
/*
void loop()
{
	BuiltinLedToggler.Toggle();

    switch(gbl_state.state_id)
    {
        #define TREATE_CASE()
        case STARTUP:   
            on_
    }
/*
	delay(10);

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

*/
