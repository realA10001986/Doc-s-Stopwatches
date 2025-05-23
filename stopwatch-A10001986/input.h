/*
 * -------------------------------------------------------------------
 * BTTF Stopwatch
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Stopwatch
 */

#ifndef _SWINPUT_H
#define _SWINPUT_H

/*
 * SWButton class
 */


typedef enum {
    SWBS_IDLE,
    SWBS_PRESSED,
    SWBS_RELEASED,
    SWBS_LONGPRESS,
    SWBS_LONGPRESSEND
} ButtonState;

class SWButton {
  
    public:
        SWButton(const int pin, const bool activeLow = true, const bool pullupActive = true);
      
        void setTiming(const int debounceDur, const int pressDur, const int lPressDur);

        void attachPressDown(void (*newFunction)());
        void attachPress(void (*newFunction)(void));
        void attachLongPressStart(void (*newFunction)(void));
        void attachLongPressStop(void (*newFunction)(void));

        void scan(void);

    private:

        void reset(void);
        void transitionTo(ButtonState nextState);

        void (*_pressDownFunc)(void) = NULL;
        void (*_pressFunc)(void) = NULL;
        void (*_longPressStartFunc)(void) = NULL;
        void (*_longPressStopFunc)(void) = NULL;

        int _pin;
        
        unsigned int _debounceDur = 50;
        unsigned int _pressDur = 400;
        unsigned int _longPressDur = 800;
      
        int _buttonPressed;

        bool _wasPressed = false;
      
        ButtonState _state     = SWBS_IDLE;
        ButtonState _lastState = SWBS_IDLE;
      
        unsigned long _startTime;
};

#endif
