/*
 * -------------------------------------------------------------------
 * BTTF Stopwatch
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Stopwatch
 */ 

#include <stddef.h>
#include <avr/io.h>
#include <Wire.h>

#include "input.h"

/*
 * SWButton class
 */

/* pin: The pin to be used
 * activeLow: Set to true when the input level is LOW when the button is pressed, Default is true.
 * pullupActive: Activate the internal pullup when available. Default is true.
 */
SWButton::SWButton(const int pin, const bool activeLow, const bool pullupActive)
{
    _pin = pin;

    _buttonPressed = activeLow ? LOW : HIGH;
  
    pinMode(pin, pullupActive ? INPUT_PULLUP : INPUT);
}


// debounce: Number of millisec that have to pass by before a click is assumed stable
// press:    Number of millisec that have to pass by before a short press is detected
// lPress:   Number of millisec that have to pass by before a long press is detected
void SWButton::setTiming(const int debounceDur, const int pressDur, const int lPressDur)
{
    _debounceDur = debounceDur;
    _pressDur = pressDur;
    _longPressDur = lPressDur;
}

// Register function for press-down event
void SWButton::attachPressDown(void (*newFunction)())
{
    _pressDownFunc = newFunction;
}

// Register function for short press event
void SWButton::attachPress(void (*newFunction)(void))
{
    _pressFunc = newFunction;
}

// Register function for long press start event
void SWButton::attachLongPressStart(void (*newFunction)(void))
{
    _longPressStartFunc = newFunction;
}

// Register function for long press stop event
void SWButton::attachLongPressStop(void (*newFunction)(void))
{
    _longPressStopFunc = newFunction;
}

// Check input of the pin and advance the state machine
void SWButton::scan(void)
{
    unsigned long now = millis();
    unsigned long waitTime = now - _startTime;
    bool active = (digitalRead(_pin) == _buttonPressed);
    
    switch(_state) {
    case SWBS_IDLE:
        if(active) {
            transitionTo(SWBS_PRESSED);
            _startTime = now;
        }
        break;

    case SWBS_PRESSED:
        if((!active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        // ----
        } else if((active) && (waitTime > _longPressDur)) {
            if(_longPressStartFunc) _longPressStartFunc();
            transitionTo(SWBS_LONGPRESS);
        } else {
            if(!_wasPressed) {
                if(_pressDownFunc) _pressDownFunc();
                _wasPressed = true;
            }
            if(!active) {
                transitionTo(SWBS_RELEASED);
                _startTime = now;
            }
        }
        // ----
        /*
        } else if(!active) {
            transitionTo(SWBS_RELEASED);
            _startTime = now;
        } else if(waitTime > _longPressDur) {
            if(_longPressStartFunc) _longPressStartFunc();
            transitionTo(SWBS_LONGPRESS);
        }
        */
        break;

    case SWBS_RELEASED:
        if((active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        } else if((!active) && (waitTime > _pressDur)) {
            if(_pressFunc) _pressFunc();
            reset();
        }
        break;
  
    case SWBS_LONGPRESS:
        if(!active) {
            transitionTo(SWBS_LONGPRESSEND);
            _startTime = now;
        }
        break;

    case SWBS_LONGPRESSEND:
        if((active) && (waitTime < _debounceDur)) { // de-bounce
            transitionTo(_lastState);
        } else if(waitTime >= _debounceDur) {
            if(_longPressStopFunc) _longPressStopFunc();
            reset();
        }
        break;

    default:
        transitionTo(SWBS_IDLE);
        break;
    }
}

/*
 * Private
 */

void SWButton::reset(void)
{
    _state = SWBS_IDLE;
    _lastState = SWBS_IDLE;
    _startTime = 0;
    _wasPressed = false;
}

// Advance to new state
void SWButton::transitionTo(ButtonState nextState)
{
    _lastState = _state;
    _state = nextState;
}
