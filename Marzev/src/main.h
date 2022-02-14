#pragma once

#include "NYG.h"
#include "Timer.h"

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
};

enum StateId
{
    STARTUP,
    DRY,
    BOTTOM_LEVEL_SIGNALED,
    TOP_LEVEL_SIGNALED,
    DRAIN
};

enum SensorsState   { NOT_SIGNALED, BOTTOM, TOP };

struct State
{
    StateId         state_id;
    SensorsState    sensors_state;
    bool            sensor_failure;
    bool            pumping;
    bool            simulation;
};

extern State gbl_state;

void UpdateSensorsState();
void StartBlickingWarningLed();
void ShowState();
bool OnSensorsCommand(const String& s);
String GetSensorsStates();
void ResetSimulationValues();

void StartTimer(Timer& t, const char* timer_name, unsigned long delay, TimeUnit unit);
void StopTimer(Timer& t, const char* timer_name);
bool TestTimer(Timer& t, const char* timer_name);