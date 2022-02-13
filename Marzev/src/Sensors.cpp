#include "main.h"

#include "IInput.h"
#include "StableInput.h"
#include "Observer.h"

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

void UpdateSensorsState()
{
    
}
