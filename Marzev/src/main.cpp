/*
 Name:		main.cpp
 Created:	13/02/2022
 Author:	YLA
*/

#include "main.h"

State gbl_state;

#include "RedGreenLed.h"
#include "IOutput.h"
#include "Toggler.h"
#include "MicroController.h"

//-----------------------------------------------------
//	OUTPUT elements
//-----------------------------------------------------

static DigitalOutputPin			buzzer(BUZZER_PIN);
static DigitalOutputPin			warning_led(WARNING_LED_PIN);
static RedGreenLed				led(RED_LED_PIN, GREEN_LED_PIN);
static DigitalOutputPin			pump(POMP_RELAY_PIN);

//-----------------------------------------------------
//	Other...
//-----------------------------------------------------

#define DECLARE_TIMER_NAME(name)    static const char name[] = #name

static Timer                pump_activity_signal_timer;

static Timer                last_drain_timer;
DECLARE_TIMER_NAME(LAST_DRAIN);

static Timer                simulation_timer;
DECLARE_TIMER_NAME(SIMULATION);

DigitalOutputPin			BuiltinLed(LED_BUILTIN);
static Toggler				BuiltinLedToggler;
static Toggler				WarningLedToggler;	// used to toggle the warning led


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

#define set_state_id(id)  do {    \
    if(id != gbl_state.state_id)    {\
        gbl_state.state_id = id;    \
        LOGGER << S("State set to ") << #id << NL;\
        if(DRY > id)  StopTimer(last_drain_timer, LAST_DRAIN); } } while(0)

#define DEFINE_SET_STATE_FUNC(id)   static void set_state_to_##id() { set_state_id(id); }

DEFINE_SET_STATE_FUNC(DRY)
DEFINE_SET_STATE_FUNC(BOTTOM_LEVEL_SIGNALED)
DEFINE_SET_STATE_FUNC(TOP_LEVEL_SIGNALED)
DEFINE_SET_STATE_FUNC(DRAIN)

static void set_pumping(bool on)
{
    if(pump.SetAndLog(on, "PUMP"))
    {
        gbl_state.pumping = on;

        pump_activity_signal_timer.Stop();

        buzzer.Off();
        led.SetOff();

        if(on && (TOP == gbl_state.sensors_state))
        {
            buzzer.On();
            led.GetRed().On();

            return;
        }

        pump_activity_signal_timer.Start(1, SECS);
    }
}
//--------------------------------------------------------------
void setup()
{
	Logger::Initialize();

	buzzer.On();		delay(500);			buzzer.Off();
    warning_led.On();   delay(2000);        warning_led.Off();
	led.SetGreen();		delay(2000);		led.SetOff();
	led.SetRed();		delay(2000);		led.SetOff();

	pump.Off();

    memzero(gbl_state);

    gbl_state.state_id       = STARTUP;
    gbl_state.sensors_state  = NOT_SIGNALED;
    gbl_state.pumping        = false;
    gbl_state.sensor_failure = false;
    gbl_state.simulation     = false;

	BuiltinLedToggler.StartOnOff(BuiltinLed, 1000);

	LOGGER << S("Ready!") << NL;
}
//--------------------------------------------------------------
void loop()
{
    delay(100);
    
    static State state = { (StateId)-1 };

    if(!objequal(state, gbl_state))
    {
        state = gbl_state;
        ShowState();
    }

	BuiltinLedToggler.Toggle();
    WarningLedToggler.Toggle();

    if(pump_activity_signal_timer.Test())
    {
        if(pump.IsOff())
        {
            buzzer.Off();
            led.GetGreen().Toggle();
        }
        else
        {
            buzzer.Toggle();
            led.GetRed().Toggle();
        }
    }

    if(TestTimer(last_drain_timer, LAST_DRAIN))
    {
        set_state_to_DRAIN();
    }

    if(TestTimer(simulation_timer, SIMULATION))
    {
		MicroController::Restart();
		return;
    }

	if (Serial.available())
    {
		treat_serial_input();
    }

    UpdateSensorsState();
    state_funcs[gbl_state.state_id]();
}
//--------------------------------------------------------------
static void on_STARTUP()
{
    switch(gbl_state.sensors_state)
    {
        case NOT_SIGNALED:
            set_state_to_DRAIN();
            break;

        case BOTTOM:
            set_state_to_BOTTOM_LEVEL_SIGNALED();
            break;

        case TOP:
            set_state_to_TOP_LEVEL_SIGNALED();
            break;
    }
}
//--------------------------------------------------------------
static void on_DRY()
{
    set_pumping(false);

    switch(gbl_state.sensors_state)
    {
        case NOT_SIGNALED:
            break;

        case BOTTOM:
            set_state_to_BOTTOM_LEVEL_SIGNALED();
            break;

        case TOP:
            set_state_to_TOP_LEVEL_SIGNALED();
            break;
    }
}
//--------------------------------------------------------------
static void on_BOTTOM_LEVEL_SIGNALED()
{
    if(pump.IsOff())
    {
        set_pumping(true);
        return;
    }

    switch(gbl_state.sensors_state)
    {
        case NOT_SIGNALED:
            if(gbl_state.simulation)
                StartTimer(last_drain_timer, LAST_DRAIN, 2, MINS );
            else
                StartTimer(last_drain_timer, LAST_DRAIN, 2, HOURS );

            set_state_to_DRAIN();
            break;

        case BOTTOM:
            break;

        case TOP:
            set_state_to_TOP_LEVEL_SIGNALED();
            break;
    }
}
//--------------------------------------------------------------
static void on_TOP_LEVEL_SIGNALED()
{
    if(pump.IsOff())
    {
        set_pumping(true);
        return;
    }

    if(TOP == gbl_state.sensors_state)
        return;

    set_state_to_BOTTOM_LEVEL_SIGNALED();
}
//--------------------------------------------------------------
static void on_DRAIN()
{
    static Timer drain_timer;
    static const char timer_name[] = "DRAIN";

    switch(gbl_state.sensors_state)
    {
        case NOT_SIGNALED:
            break;

        case BOTTOM:
            StopTimer(drain_timer, timer_name);
            set_state_to_BOTTOM_LEVEL_SIGNALED();
            return;

        case TOP:
            StopTimer(drain_timer, timer_name);
            set_state_to_TOP_LEVEL_SIGNALED();
            return;
    }

    if(drain_timer.IsStarted())
    {
        if(TestTimer(drain_timer, timer_name))
            set_state_to_DRY();
    }
    else
    {
        StartTimer(drain_timer, timer_name, 10, SECS);
        set_pumping(true);
    }
}
//--------------------------------------------------------------
void StartBlickingWarningLed()
{
    if(WarningLedToggler.IsStarted())
    {
        return;
    }
        
	WarningLedToggler.StartOnOff(warning_led, 200);
}
//--------------------------------------------------------------
static void treat_serial_input()
{
	String s = Serial.readString();

    s.trim();

    if(s.length() == 0)
        return;

	LOGGER << S("Command '") << s << S("' got from serial") << NL;

    s.toUpperCase();
    
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

	if (s == "SIM")
	{
		gbl_state.simulation = true;
        StartTimer(simulation_timer, SIMULATION, 30, MINS);
        ResetSimulationValues();
		return;
	}
/*
	if (s == "NOSIM")
	{
		gbl_state.simulation = false;
        simulation_timer.Stop();
        gbl_state.sensor_failure = false;
		return;
	}
*/
    if(OnSensorsCommand(s))
    {
        return;
    }

	Serial.println("Unknown command");
}
//------------------------------------------------------------------------------------------
static void show_state_field(const char* name, const bool& value, bool onoff)
{
    _LOGGER << S("    ") << name << S(": ");
    
    if(onoff)   _LOGGER     << ONOFF(value).Get()   << NL;
    else        _LOGGER     << value                << NL;
}
//------------------------------------------------------------------------------------------
template <class T>
static void show_state_field(const char* name, const T& value)
{
    _LOGGER << S("    ") << name << S(": ") << value << NL;
}
//--------------------------------------------------------------
static const char* GetStateIdName()
{
    switch(gbl_state.state_id)
    {
        #define TREATE_CASE(id) case id: return #id
        TREATE_CASE(STARTUP);
        TREATE_CASE(DRY);
        TREATE_CASE(BOTTOM_LEVEL_SIGNALED);
        TREATE_CASE(TOP_LEVEL_SIGNALED);
        TREATE_CASE(DRAIN);
        #undef TREATE_CASE
    }

    return "?";
}
//--------------------------------------------------------------
static const char* GetSensorsStateName()
{
    switch(gbl_state.sensors_state)
    {
        #define TREATE_CASE(id) case id: return #id
        TREATE_CASE(NOT_SIGNALED);
        TREATE_CASE(BOTTOM);
        TREATE_CASE(TOP);
        #undef TREATE_CASE
    }

    return "?";
}
//--------------------------------------------------------------
void ShowState()
{
    _LOGGER << S("Status: ") << NL;

    #define SHOW_BOOL_FLD(fld)     show_state_field( #fld, gbl_state.fld, false )
    #define SHOW_ONOFF_FLD(fld)    show_state_field( #fld, gbl_state.fld, true )
    #define SHOW_FLD(fld)          show_state_field( #fld, gbl_state.fld )

    show_state_field("state_id",        GetStateIdName());
    show_state_field("sensors_state",   GetSensorsStateName());
    show_state_field("sensors",         GetSensorsStates());
    SHOW_ONOFF_FLD(pumping);
    SHOW_BOOL_FLD(sensor_failure);
    SHOW_ONOFF_FLD(simulation);
}
//--------------------------------------------------------------
void StartTimer(Timer& t, const char* timer_name, unsigned long delay, TimeUnit unit)
{
    if(t.IsStarted())
        return;

    t.StartOnce(delay, unit);

    LOGGER << timer_name << S(" timer started for ") << delay << S(" ") << GetTimeUnitName(unit) << NL;
}
//--------------------------------------------------------------
void StopTimer(Timer& t, const char* timer_name)
{
    if(!t.IsStarted())
        return;

    t.Stop();

    LOGGER << timer_name << S(" timer stopped") << NL;
}
//--------------------------------------------------------------
bool TestTimer(Timer& t, const char* timer_name)
{
    if(t.Test())
    {
        LOGGER << timer_name << S(" timer timed-out") << NL;
        return true;
    }

    return false;
}
//--------------------------------------------------------------
