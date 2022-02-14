#include "main.h"

#include "IInput.h"
#include "StableInput.h"
#include "Observer.h"
#include "Timer.h"

#define DRY_TEST	0

#if DRY_TEST	
	#define DELAY_MINUTES	1
	#define WATER_SENSOR_REVERSED	true
#else
	#define DELAY_MINUTES	5
	#define WATER_SENSOR_REVERSED	false
#endif

//-----------------------------------------------------
//	WaterLevelSensor class
//-----------------------------------------------------

class WaterLevelSensor
{
public:

	#define WSR						WATER_SENSOR_REVERSED

	WaterLevelSensor(Pin pin) : sensor(pin, WSR), stable(sensor), observer(stable)	
    {
        simulation_value = simulation_value_changed = false;
    }

	bool TestChanged(bool& value)
	{
        if(gbl_state.simulation)
        {
            value = simulation_value;

            if(simulation_value_changed)
            {
                simulation_value_changed = false;
                return true;
            }
            
            return false;
        }

		return observer.TestChanged(value);
	}

    bool Get()  const
    {
        return (gbl_state.simulation) ? simulation_value : observer.GetValue();
    }

    void SetSimulationValue(bool val)
    {
        if(simulation_value == val)
            return;

        simulation_value = val;
        simulation_value_changed = true;
    }

    void ResetSimulationValues()
    {
        simulation_value = simulation_value_changed = false;
    }

private:

	DigitalPullupInputPin	sensor;
	typedef StableDigitalInput<500, 500, millis>	BouncingSensor;
	BouncingSensor			stable;
	DigitalObserver			observer;
    bool                    simulation_value;
    bool                    simulation_value_changed;
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
			if (s##n.TestChanged(v##n))	LOGGER << name << S("#" #n " is ") << v##n << NL

		CHECK_SENSOR_CHANGED(1);
		CHECK_SENSOR_CHANGED(2);
		CHECK_SENSOR_CHANGED(3);

		#undef CHECK_SENSOR_CHANGED

		count = (int)v1 + (int)v2 + (int)v3;
		return count >= 2;
	}

	uint8_t GetCount()	const 
    { 
        return count; 
    }

    const char* GetStatus() const
    {
        static char retval[4] = { 0 };

        retval[0] = s1.Get() + '0';
        retval[1] = s2.Get() + '0';
        retval[2] = s3.Get() + '0';

        return retval;
    }

    void OnCommand(const char* abc)
    {
        s1.SetSimulationValue(abc[1] - '0');
        s2.SetSimulationValue(abc[2] - '0');
        s3.SetSimulationValue(abc[3] - '0');
        LOGGER << name << S(" set to ") << GetStatus() << NL;
    }

    void ResetSimulationValues()
    {
        s1.ResetSimulationValues();
        s2.ResetSimulationValues();
        s3.ResetSimulationValues();
    }

    String GetSensorsStates()   const
    {
        return String(name) + '=' + GetStatus();
    }

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

static LevelInput				bottom_level(_bottom_level_sensor),
								top_level	 (_top_level_sensor);

//--------------------------------------------------------------
static void log_error(const char* reason)
{
    LOGGER << S("*** SENSORS ERROR: ") << reason << S(". States: ") << GetSensorsStates() << S(" ***") << NL;
    gbl_state.sensor_failure = true;
    StartBlickingWarningLed();
}
//--------------------------------------------------------------
static bool check_sensors_conflicting_status()
{
    uint8_t top_count = _top_level_sensor.GetCount();

    if(top_count)
    {
        uint8_t bottom_count = _bottom_level_sensor.GetCount();

        if(bottom_count != 3)
        {
            log_error("Conflicting bottom-top sensors state");
            return true;
        }
    }

    return false;
}
//--------------------------------------------------------------
static bool check_bottom_sensors_stucked()
{
    static Timer timer;
    static const char timer_name[] = "Sensors sanity test";

    bool stop_timer = false;

    if(DRAIN == gbl_state.state_id)
    {
        switch(_bottom_level_sensor.GetCount())
        {
            case 0 :
            case 2 : 
                stop_timer = true;
                break;

            case 1 : 
                if(!timer.IsStarted()) 
                {
                    if(gbl_state.simulation)
                        StartTimer(timer, timer_name, 20, SECS);
                    else
                        StartTimer(timer, timer_name, 5, MINS);
                }

                break;
        }

        return false;
    }
    
    if(DRY == gbl_state.state_id)
    {
        switch(_bottom_level_sensor.GetCount())
        {
            case 0 :
            case 2 : 
                stop_timer = true;
                break;

            case 1 : 
                if(timer.Test())
                {
                    log_error("Bottom sensor looks to be stuck");
                    return true;
                }

                break;
        }
    }

    if(stop_timer) 
        StopTimer(timer, timer_name);

    return false;
}
//--------------------------------------------------------------
static void do_sensors_sanity_check()
{
    if(gbl_state.sensor_failure)
        return;

    if(check_sensors_conflicting_status())
        return;

    if(check_bottom_sensors_stucked())
        return;
}
//--------------------------------------------------------------
void UpdateSensorsState()
{
    bool top    = top_level.Get(), 
         bottom = bottom_level.Get();

    SensorsState state = top        ? TOP    :
                         bottom     ? BOTTOM :
                                      NOT_SIGNALED;

    gbl_state.sensors_state = state;

    do_sensors_sanity_check();
}
//--------------------------------------------------------------
bool OnSensorsCommand(const String& s)
{
    if(!gbl_state.simulation)
        return false;

    const char* abc = s.c_str();

    if(strlen(abc) != 4)
        return false;

    for(int idx = 1; abc[idx]; idx++)
    {
        if(abc[idx] < '0' || abc[idx] > '1')
            return false;
    }

    switch(s[0])
    {
        case 'B' : _bottom_level_sensor.OnCommand(abc);
                   break;
        case 'T' : _top_level_sensor.OnCommand(abc);
                   break;
        default  : return false;
    }

    return true;
}
//--------------------------------------------------------------
void ResetSimulationValues()
{
    _bottom_level_sensor.ResetSimulationValues();
    _top_level_sensor.ResetSimulationValues();
}
//--------------------------------------------------------------
String GetSensorsStates()
{
    return _bottom_level_sensor.GetSensorsStates() + ' ' + _top_level_sensor.GetSensorsStates();
}
//--------------------------------------------------------------
