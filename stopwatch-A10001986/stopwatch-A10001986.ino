/*
 * -------------------------------------------------------------------
 * BTTF Stopwatch
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Stopwatch
 */

 /*
  * Holding Button 1 selects the mode of operation:
  * - Movie mode: Pressing button 2 advances, holding resets
  * - Stop Watch: Like original
  * - Counter: Pressing button 2 increases counter, pressing button 1
  *   resets it.
  * - Clock mode: Holding button 2 enters, advances and exits "set" mode,
  *   pressing button 2 increases hours/minutes, pressing button 1
  *   increases minutes by 10.
  * 
  * 2024/12/10 (1.06)
  * - Add screen saver for Movie Mode. After 20 mins without user interaction,
  *   the display will be switched off in order to save power.
  * - Extend "Calibration" to "Settings"; first step in Settings is Calibration,
  *   after holding button 2 the Screen Saver can be enabled/disabled.
  *   Button 2 saves, button 1 exits without saving.
  * 2024/12/09 (1.05)
  * - Perform display test on the first couple of power-ups
  * 2024/12/06 (1.04)
  * - Ignore button 2 when stopwatch has reached end
  * 2024/12/06 (1.03)
  * - Make calib range a bit wider (980-1020 now).
  * - Make stopwatch run until 99:59.
  * 2024/12/04
  * - Add delay() in setup() to allow for power to settle
  * 2024/12/01
  * - Add calibration mode. Holding Button 1 while powering on enters this
  *   mode. It allows to adjust the number of chip-read milliseconds to be 
  *   assumed one second. The default is, naturally, 1000. If the clock runs 
  *   too fast, increase this value. If it is too slow, decrease it.
  *   Holding button 1 cancels calibration, holding button 2 saves the
  *   currently shown value and exits.
  *   Note that this is a toy, no real chronograph. Clock speed may vary
  *   due to imprecision of the built-in oscillator, voltage, temperature.
  * 2024/11/30
  * - Add counter mode
  * - Reversed buttons for most functions, in order to have a button 2
  *   "pressDown" handler for immediate stopwatch start/stop.
  * - Replace slow modulo 10 and division by 10 operations by tables
  * - Reduce press time from 200 to 100ms.
  * 2024/11/29
  * - Add stop watch and "real" clock mode. Holding Button 2 selects
  *   the mode of operation. Firmware always starts in movie mode.
  * - Stop watch: Supports start/stop and lap/reset as normal stop watch
  * - Clock mode: Holding Button 1 switches in to "set" mode, hour and
  *   minute can be set by short button presses (Button 1). Button 2
  *   increases minutes in steps of 10. Holding button 1 advances from
  *   hour setup to minute setup and afterwards exists "set" mode.
  * 2024/11/20
  * - Initial version. Only knows movie-mode.
  * 
  * 
  * Programming parameters for ArduinoIDE:
  *   Board: Minicore / ATMega328
  *   Clock: Int 8Mhz
  *   BOD: 1.8V or disabled, does not matter with 3V Lithium batteries
  *   EEPROM retained
  *   LTO enabled 
  *   Variant: 328PB
  *   No bootloader
  */

#include <avr/io.h>
#include <util/delay.h>
#include <Wire.h>
#include <EEPROM.h>

#include "input.h"

static const char *fwv = "1.06";   // do not change pattern!

//#define FONT_PROTO        // Use prototype segment routing

#define LEFT_MOST  59     // if left-most digit is fully usable
#define LEFT_MOSTH 99     // if left-most digit is fully usable
//#define LEFT_MOST 19    // if left-most digit is cut off: Can only show "1" there
//#define LEFT_MOSTH 19   // if left-most digit is cut off: Can only show "1" there

#define BTTF_HOUR       1
#define BTTF_START_MIN 18
#define BTTF_END_MIN   22

#define CLK_HOUR_START 12
#define CLK_MIN_START   0

#define MIL_MIN       980
#define MIL_MAX      1020

// 7 seg font
#ifdef FONT_PROTO // Board < 10
#define S7G_T   0b01000000    // A  top
#define S7G_TR  0b00100000    // B  top right
#define S7G_BR  0b00010000    // C  bottom right
#define S7G_B   0b00001000    // D  bottom
#define S7G_BL  0b00000100    // E  bottom left
#define S7G_TL  0b00000010    // F  top left
#define S7G_M   0b00000001    // G  middle
#define S7G_DOT 0b10000000    // DX dot
#else // Board 10+
#define S7G_T   0b00000100    // A  top
#define S7G_TR  0b00001000    // B  top right
#define S7G_BR  0b00000001    // C  bottom right
#define S7G_B   0b00000010    // D  bottom
#define S7G_BL  0b01000000    // E  bottom left
#define S7G_TL  0b10000000    // F  top left
#define S7G_M   0b00010000    // G  middle
#define S7G_DOT 0b00100000    // DX dot
#endif

static const uint8_t font7seg[38] = {
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,        // 0
    S7G_TR|S7G_BR,                                  // 1
    S7G_T|S7G_TR|S7G_B|S7G_BL|S7G_M,                // 2
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_M,                // 3
    S7G_TR|S7G_BR|S7G_TL|S7G_M,                     // 4
    S7G_T|S7G_BR|S7G_B|S7G_TL|S7G_M,                // 5
    S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,               // 6
    S7G_T|S7G_TR|S7G_BR,                            // 7
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,  // 8
    S7G_T|S7G_TR|S7G_BR|S7G_TL|S7G_M,               // 9
    S7G_T|S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,        // A
    S7G_BR|S7G_B|S7G_BL|S7G_TL|S7G_M,               // B
    S7G_T|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_M,
    S7G_T|S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL,
    S7G_T|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_B|S7G_BL|S7G_TL,
    S7G_T|S7G_BR|S7G_BL,
    S7G_T|S7G_TR|S7G_BR|S7G_BL|S7G_TL,
    S7G_T|S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_T|S7G_TR|S7G_BL|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_B|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_BL|S7G_TL,
    S7G_T|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_B|S7G_BL|S7G_TL|S7G_M,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_BR|S7G_B|S7G_BL|S7G_TL,
    S7G_TR|S7G_B|S7G_TL,
    S7G_TR|S7G_BR|S7G_BL|S7G_TL|S7G_M,
    S7G_TR|S7G_BR|S7G_B|S7G_TL|S7G_M,
    S7G_T|S7G_TR|S7G_B|S7G_BL|S7G_M,                // Z
    S7G_DOT,
    S7G_M
};

static const int div10[100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9
};

static const int mod10[100] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9
};

static unsigned long mMillis = 1000;

static unsigned long mMovieSecond = 0;
static unsigned long mMovieMinute = 0;
static unsigned long mMovieDotOnDur = 0;
static unsigned long mMovieDotOffDur = 0;
static unsigned long mMovieBlinkOffs = 0;

static unsigned long mRealSecond = 0;
static unsigned long mRealMinute = 0;
static unsigned long mRealDotOnDur = 0;
static unsigned long mRealDotOffDur = 0;

static int           address = 0;
static uint8_t       feature_reg = 0;

// Operation mode. Start in movie mode.
static int           opMode = 0;    

// Movie mode
static int           hours = BTTF_HOUR;
static int           mins  = BTTF_START_MIN;
static unsigned long mySecMillis = 0;
static unsigned long myMinMillis = 0;
static uint8_t       mdotsOn = 0;
static unsigned long dot_duration = mMovieDotOffDur;

// Stop watch
static bool          swRunning = false, lap = false, swEndReached = false;
static unsigned long SWStart = 0, elapsed = 0;
static int           oDig1 = 0, oDig2 = 0, oDig3 = 0, oDig4 = 0;
static uint8_t       oDp2 = 0, oColon = 0, oDp4 = 0;

// Counter mode
static int           oldCounter = 0, counter = 0;

// Clock mode
static int           rhours = CLK_HOUR_START;
static int           rmins  = CLK_MIN_START;
static unsigned long myRSecMillis = 0;
static unsigned long myRMinMillis = 0;
static uint8_t       rdotsOn = 0;
static unsigned long rdot_duration = mRealDotOffDur;
static bool          setMode = false;
static bool          setHours = false;
static int           tempHours, tempMins;

// Calib
static unsigned long tMillis = 0;
static int           sMode = 0;

static bool          j1 = false;
static bool          ssIsOn = false;
static bool          useSS = true;
static unsigned long ssTimer = 0;

#define BUTTON1_PIN PIN_PD1
#define BUTTON2_PIN PIN_PD2

#define BUTTON_DEBOUNCE    50    // debounce time in ms
#define BUTTON_PRESS_TIME 100    // button will register a short press
#define BUTTON_HOLD_TIME 2000    // time in ms holding the button will count as a long press

#define SS_TIMEOUT    (60*20)    // Screen-saver timeout (seconds)

static SWButton button = SWButton(BUTTON1_PIN,
    true,    // Button is active LOW
    true     // Enable internal pull-up resistor
);

static SWButton button2 = SWButton(BUTTON2_PIN,
    true,    // Button is active LOW
    true     // Enable internal pull-up resistor
);

static bool isButtonPressed = false;
static bool isButtonHeld = false;
static bool isButton2PressedDown = false;
static bool isButton2Pressed = false;
static bool isButton2Held = false;

static void resetMinuteM(unsigned long m);
static void resetMinuteR(unsigned long m);
static void showTime(bool movie);
static void setAndShowTime(bool movie, int h, int m);
static void incMovieTime();
static void incRealTime();
static void invertDotsM();
static void invertDotsR();
static void showDotsM();
static void showDotsR();
static void setDotsM(bool on);
static void setDotsR(bool on);

static void onlyShowHours(int h);
static void onlyShowMins(int m);

static void resetSW();
static void writeSWPrevious();

static void setBlink(bool blink);
static void directCmd(uint8_t cmd, uint8_t val);
static void writeDigits(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4);

static void bottonPressed();
static void buttonHeld();
static void botton2PressedDown();
static void botton2Pressed();
static void button2Held();

static void exitMode99();
static void calcTiming();

static void ssOff(unsigned long n);
static void ssOn();

static bool readEEPROM(unsigned long& ulv, bool &uss);
static void writeEEPROM(unsigned long ulv);
static void writeEEPROMSS(bool uss);
static bool checkDispTest();

void setup() 
{
    unsigned long now, temp;
    bool fontGen = true;

    // Wait a bit to let power settle
    delay(100);
   
    Wire.begin();

    Wire.beginTransmission(address);
    if(Wire.endTransmission(true)) {
        while(1); // didn't find display driver
    }

    // Init buttons
    button.setTiming(BUTTON_DEBOUNCE, BUTTON_PRESS_TIME, BUTTON_HOLD_TIME);
    button.attachPress(buttonPressed);
    button.attachLongPressStart(buttonHeld);
    button2.setTiming(BUTTON_DEBOUNCE, BUTTON_PRESS_TIME, BUTTON_HOLD_TIME);
    button2.attachPressDown(button2PressedDown);
    button2.attachPress(button2Pressed);
    button2.attachLongPressStart(button2Held);

    pinMode(PIN_PD4, INPUT);
    pinMode(PIN_PD3, OUTPUT);
    digitalWrite(PIN_PD3, LOW);
    delay(20);
    if(!digitalRead(PIN_PD4)) {
        digitalWrite(PIN_PD3, HIGH);
        delay(20);
        if(digitalRead(PIN_PD4)) {
            digitalWrite(PIN_PD3, LOW);
            delay(20);
            if(!digitalRead(PIN_PD4)) {
                j1 = true;
            }
        }
    }

    // Shutdown register: Wake up
    directCmd(0x0c, 0x01);
    
    // Trigger "Self address"
    directCmd(0x2d, 0x01);
    delay(30);

    // From this point on, the i2c address is 0x03
    address = 3;

    // Feature register
    feature_reg = 0x00;
    directCmd(0x0e, feature_reg);

    // Decode off
    directCmd(0x09, 0x00);

    // Scan limit: Display 0:3
    directCmd(0x0b, 0x03);

    // Clear digits
    writeDigits(0x00, 0x00, 0x00, 0x00);

    // Global brightness
    directCmd(0x0a, 0x0f);

    // Display Test: Light up all segments
    if(checkDispTest()) {
        writeDigits(0xff, 0xff, 0xff, 0xff);
        delay(3000);
    }

    // Read calibration data from EEPROM
    if(readEEPROM(temp, useSS)) {
        if(temp >= MIL_MIN && temp <= MIL_MAX) {
            mMillis = temp;
        }
    }
    
    calcTiming();
    
    // See if user wants to enter calibration mode
    if(!digitalRead(BUTTON1_PIN)) {
       delay(50);
       if(!digitalRead(BUTTON1_PIN)) {
          opMode = 99;
       }
    // See if user wants to see firmware version
    } else if(!digitalRead(BUTTON2_PIN)) {
       delay(50);
       if(!digitalRead(BUTTON2_PIN)) {
          writeDigits(0 | S7G_DOT, 
                      font7seg[fwv[0] - '0'],
                      font7seg[fwv[2] - '0'],
                      font7seg[fwv[3] - '0']);
          while(!digitalRead(BUTTON2_PIN)) {}
       }
    }

    // Init display for movie mode
    setDotsM(false);
    setDotsR(true);

    switch(opMode) {
    case 99:
        tMillis = mMillis;
        showNumber(tMillis);
        while(!digitalRead(BUTTON1_PIN)) {}
        break;
    default:
        setAndShowTime(true, hours, mins);
    }

    now = millis();
    ssOff(now);
    resetMinuteM(now);
    resetMinuteR(now);
}

void loop() 
{
    unsigned long now = millis();
    bool movieMod = false;
    bool mTimeChg = false;
    bool rTimeChg = false;
    bool mDotsChg = false;
    bool rDotsChg = false;
    bool forceDisplay = false;
    bool forceDisplaySWcur = false;

    // Clock
    if(now - myMinMillis > mMovieMinute) {
        myMinMillis += mMovieMinute;
        incMovieTime();
        mTimeChg = true;
    }

    if(now - myRMinMillis > mRealMinute) {
        myRMinMillis += mRealMinute;
        incRealTime();
        rTimeChg = true;
    }

    if(now - mySecMillis > dot_duration) {
        mySecMillis += dot_duration;
        invertDotsM();
        mDotsChg = true;
    }

    if(now - myRSecMillis > rdot_duration) {
        myRSecMillis += rdot_duration;
        invertDotsR();
        rDotsChg = true;
    }

    // Update button state
    button.scan();
    button2.scan();

    // Operation Mode selection
    if(isButtonHeld) {
        isButtonHeld = false;
        if(opMode == 99) {
            exitMode99();
        } else if(!setMode && !swRunning) {
            opMode++;
            opMode &= 3;
            isButtonPressed = false;
            isButton2PressedDown = false;
            isButton2Pressed = false;
            isButton2Held = false;
            ssOff(now);
            switch(opMode) {
            case 0:
                mTimeChg = mDotsChg = true;
                break;
            case 1:
                resetSW();
                lap = false;
                forceDisplay = true;
                break;
            case 2:
                forceDisplay = true;
                break;
            case 3:
                rTimeChg = rDotsChg = true;
                break;
            }
        }
    }

    switch(opMode) {

    case 0:   // Movie mode        

        if(isButtonPressed) {
            isButtonPressed = false;
            ssOff(now);
        }
        
        if(isButton2Pressed) {
            isButton2Pressed = false;
            movieMod = true;
            incMovieTime();
        } else if(isButton2Held) {
            isButton2Held = false;
            movieMod = true;
            mins = 18;
        }
    
        if(movieMod) {
            setDotsM(false); // They are off on min changes
            mTimeChg = true;
            resetMinuteM(now);
            ssOff(now);
        }
        
        if(mTimeChg) {
            setAndShowTime(true, hours, mins);
        } else if(mDotsChg) {
            showDotsM();
        }

        if(useSS && (now - ssTimer > SS_TIMEOUT*1000UL)) {
            ssOn();
        }
        break;

    case 1:   // Stop watch mode

        isButton2Pressed = false;   // unused
        isButton2Held = false;      // unused

        if(isButtonPressed) {           // lap/reset
            isButtonPressed = false;
            if(swRunning) {
                if(!(lap = !lap)) {
                    forceDisplay = true;
                    setBlink(false);
                } else {
                    setBlink(true);
                }
            } else {
                resetSW();
                forceDisplay = true;
            }
        }

        if(isButton2PressedDown) {      // start/stop
            isButton2PressedDown = false;
            if(!swEndReached) {
                if((swRunning = !swRunning)) {
                    SWStart = now;
                } else {
                    elapsed += (now - SWStart);
                    SWStart = now;
                    lap = false;
                    setBlink(false);
                    forceDisplaySWcur = true;
                }
                forceDisplay = true;
            }
        }
        
        if((swRunning && !lap) || forceDisplaySWcur) {
            unsigned long temp = elapsed + (now - SWStart);
            unsigned long swt_secs = temp / 1000;
            int     dig1 = -1, dig2 = -1, dig3 = -1, dig4 = -1;
            uint8_t dp2 = 0, colon = 0, dp4 = 0;
            if(swt_secs <= LEFT_MOST) {                     // <= x9.99: seconds.fractions
                unsigned long swt_f = (temp / 10) % 100;
                dig4 = mod10[swt_f];          //   . 1
                dig3 = div10[swt_f];          //   .1
                dig2 = mod10[swt_secs];       //  1.
                dig1 = div10[swt_secs];       // 1 .
                dp2 = S7G_DOT;
            } else if(swt_secs <= ((LEFT_MOSTH*60UL)+59)) { // <= x9:59: mins:seconds
                unsigned long swt_s = swt_secs % 60;
                swt_secs /= 60;
                dig4 = mod10[swt_s];          //   : 1
                dig3 = div10[swt_s];          //   :1
                dig2 = mod10[swt_secs];       //  1:
                dig1 = div10[swt_secs];       // 1 :
                colon = S7G_DOT;
            /*
            } else if(swt_secs <= LEFT_MOSTH*60*60UL) {     // <= x9:59 hrs:mins
                swt_secs /= 60;
                temp = swt_secs % 60;
                swt_secs /= 60;
                dig4 = mod10[temp];           //   : 1.
                dig3 = div10[temp]; //temp / 10;           //   :1 .
                dig2 = mod10[swt_secs]; //swt_secs % 10;       //  1:  .
                dig1 = div10[swt_secs]; //swt_secs / 10;       // 1 :  .
                colon = dp4 = S7G_DOT;
            */
            } else {
                swRunning = false;
                swEndReached = true;
            }

            if(forceDisplay || dig4 != oDig4 || dig3 != oDig3 || dig2 != oDig2 || dig1 != oDig1) {
                if(dig1 >= 0) {
                    oDig1 = dig1; oDig2 = dig2; oDig3 = dig3; oDig4 = dig4;
                    oDp2 = dp2; oColon = colon; oDp4 = dp4;
                }
                writeSWPrevious();
            }
                
        } else if(forceDisplay) {
          
            writeSWPrevious();
            
        }
        break;
        
    case 2:   // Counter mode

        isButton2Held = false;      // unused
        
        if(isButtonPressed) {           // reset
            isButtonPressed = false;
            counter = 0;
        } else if(isButton2Pressed) {   // increase
            isButton2Pressed = false;
            counter++;
            if(counter > 9999) counter = 0;
        }

        if(forceDisplay || counter != oldCounter) {
            showNumber(counter);
            oldCounter = counter;
        }

        break;
            
    case 3:   // Clock mode
        
        if(setMode) {

            if(isButton2Held) {
                isButton2Held = false;
                if(!setHours) {
                    setMode = false;
                    rhours = tempHours;
                    rmins = tempMins;
                    setAndShowTime(false, rhours, rmins);
                    setBlink(false);
                    resetMinuteR(now);
                } else {
                    setHours = false;
                    onlyShowMins(tempMins);
                }
            } else if(isButton2Pressed) {
                isButton2Pressed = false;
                if(setHours) {
                    tempHours++;
                    if(tempHours > 12) tempHours = 1;
                    onlyShowHours(tempHours);
                } else {
                    tempMins++;
                    if(tempMins > 59) tempMins = 0;
                    onlyShowMins(tempMins);
                }
            } else if(isButtonPressed) {
                isButtonPressed = false;
                if(!setHours) {
                    tempMins += 10;
                    if(tempMins > 59) tempMins -= 60;
                    onlyShowMins(tempMins);
                }
            }
            
        } else {

            isButtonPressed = false;  // unused
            isButton2Pressed = false; // unused

            if(isButton2Held) {
                isButton2Held = false;
                setMode = setHours = true;
                tempHours = rhours;
                tempMins = rmins;
                onlyShowHours(tempHours);
                setBlink(true);
            }
            
            if(!setMode) {
                if(rTimeChg) {
                    setAndShowTime(false, rhours, rmins);
                } else if(rDotsChg) {
                    showDotsR();
                }
            }
        }
        break;

    case 99:

        if(isButtonPressed) {
            isButtonPressed = false;
            if(!sMode) {
                tMillis--;
                if(tMillis < MIL_MIN) tMillis = MIL_MIN;
                showNumber(tMillis);
            } else {
                useSS = !useSS;
                showSS(useSS);
            }
        }
        if(isButton2Pressed) {
            isButton2Pressed = false;
            if(!sMode) {
                tMillis++;
                if(tMillis > MIL_MAX) tMillis = MIL_MAX;
                showNumber(tMillis);
            } else {
                useSS = !useSS;
                showSS(useSS);
            }
        }
        if(isButton2Held) {
            isButton2Held = false;
            if(!sMode) {
                writeEEPROM(tMillis);
                mMillis = tMillis;
                calcTiming();
                sMode++;
                showSS(useSS);
            } else {
                writeEEPROMSS(useSS);
                exitMode99();
            }
        }

        break;
    }
}

static void resetMinuteM(unsigned long m)
{
    myMinMillis = m;
    mySecMillis = myMinMillis - mMovieBlinkOffs;
}

static void resetMinuteR(unsigned long m)
{
    myRMinMillis = m;
    myRSecMillis = myRMinMillis;
}

static void showTime(bool movie)
{
    int h = movie ? hours : rhours;
    int m = movie ? mins : rmins;

    uint8_t hh = (h < 10) ? 0 : font7seg[1];
    uint8_t hd = font7seg[mod10[h]] | (movie ? mdotsOn : rdotsOn);

    writeDigits(hh, 
                hd,
                font7seg[div10[m]],
                font7seg[mod10[m]]);
}

static void setAndShowTime(bool movie, int h, int m)
{
    if(movie) {
        hours = h;
        mins = m;
    } else {
        rhours = h;
        rmins = m;
    }
    showTime(movie);
}

static void incMovieTime()
{
    mins++;
    if(mins > BTTF_END_MIN) mins = BTTF_START_MIN;
}

static void incRealTime()
{
    rmins++;
    if(rmins > 59) {
        rmins = 0;
        rhours++;
        if(rhours > 12) rhours = 1;
    }
}

static void invertDotsM()
{
    mdotsOn ^= S7G_DOT;
    dot_duration = mdotsOn ? mMovieDotOnDur : mMovieDotOffDur;
}

static void invertDotsR()
{
    rdotsOn ^= S7G_DOT;
    rdot_duration = rdotsOn ? mRealDotOnDur : mRealDotOffDur;
}

static void showDotsM()
{
    directCmd(0x02, font7seg[mod10[hours]] | mdotsOn);
}

static void showDotsR()
{
    directCmd(0x02, font7seg[mod10[rhours]] | rdotsOn);
}

static void setDotsM(bool on)
{
    if(on) {
        mdotsOn = S7G_DOT;
        dot_duration = mMovieDotOnDur;
    } else {
        mdotsOn = 0;
        dot_duration = mMovieDotOffDur;
    }
}

static void setDotsR(bool on)
{
    if(on) {
        rdotsOn = S7G_DOT;
        rdot_duration = mRealDotOnDur;
    } else {
        rdotsOn = 0;
        rdot_duration = mRealDotOffDur;
    }
}

static void onlyShowHours(int h)
{
    uint8_t hh = (h < 10) ? 0 : font7seg[div10[h]];
    writeDigits(hh, font7seg[mod10[h]], 0, 0);
}

static void onlyShowMins(int m)
{
    writeDigits(0, 0, font7seg[div10[m]], font7seg[mod10[m]]);
}

static void resetSW()
{
    oDig1 = oDig2 = oDig3 = oDig4 = 0;
    oColon = oDp4 = 0;
    oDp2 = S7G_DOT; 
    elapsed = 0;
    swEndReached = false;
}

static void writeSWPrevious()
{
    writeDigits(font7seg[oDig1] | oDp2, 
                font7seg[oDig2] | oColon, 
                font7seg[oDig3],
                font7seg[oDig4] | oDp4);
}

static void showNumber(int n)
{
    int c1 = n / 100;
    int c2 = n % 100;
    uint8_t dig1 = div10[c1] ? font7seg[div10[c1]] : 0;
    uint8_t dig2 = c1 ? font7seg[mod10[c1]] : 0;
    uint8_t dig3 = (c1 || div10[c2]) ? font7seg[div10[c2]] : 0;
    uint8_t dig4 = font7seg[mod10[c2]];
    writeDigits(dig1, dig2, dig3, dig4);
}    

static void setBlink(bool blink)
{
    directCmd(0x0e, blink ? feature_reg | 0x10 : feature_reg);
}

static void showSS(bool uss)
{
    uint8_t myS = font7seg['S'-'A'+10];
    writeDigits(myS, myS, 0x00, font7seg[uss ? 1 : 0]);
}

static void directCmd(uint8_t cmd, uint8_t val)
{
    Wire.beginTransmission(address);
    Wire.write(cmd);
    Wire.write(val);
    Wire.endTransmission();
}

static void writeDigits(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4)
{
    Wire.beginTransmission(address);
    Wire.write(0x01);
    Wire.write(dig1);
    Wire.write(dig2);
    Wire.write(dig3);
    Wire.write(dig4);
    Wire.endTransmission();
}

static void buttonPressed()
{
    isButtonPressed = true;
}

static void buttonHeld()
{
    isButtonHeld = true;
}

static void button2PressedDown()
{
    isButton2PressedDown = true;
}

static void button2Pressed()
{
    isButton2Pressed = true;
}

static void button2Held()
{
    isButton2Held = true;
}

static void exitMode99()
{
    unsigned long now;
    
    setDotsM(false);
    setDotsR(true);

    hours = BTTF_HOUR;
    mins  = BTTF_START_MIN;

    rhours = CLK_HOUR_START;
    rmins  = CLK_MIN_START;

    opMode = 0;
    
    setAndShowTime(true, hours, mins);
    
    now = millis();
    resetMinuteM(now);
    resetMinuteR(now);
}

static void calcTiming()
{
    /* 
     * Movie mode:
     * A second is in fact not a second on those watches in the movie.
     * The colon is visible for 12/13 frames, invisible for 9 frames.
     * That makes 21/22 frames all together, instead of 24.
     * Moreover, the colon is not in sync with the minutes.
     * Nothing is right here.
     */
    mMovieSecond    = (mMillis * 22UL) / 24UL;
    mMovieMinute    = mMovieSecond * 60UL;
    mMovieDotOnDur  = (mMovieSecond * 13UL) / 22UL;
    mMovieDotOffDur = mMovieSecond - mMovieDotOnDur;
    mMovieBlinkOffs = mMovieDotOffDur / 2UL;
    
    mRealSecond     = mMillis;
    mRealMinute     = mRealSecond * 60UL;
    mRealDotOnDur   = (mRealSecond * 13UL) / 24UL;
    mRealDotOffDur  = mRealSecond - mRealDotOnDur;
}

static void ssOff(unsigned long n)
{
    ssTimer = n;

    if(!ssIsOn)
        return;

    directCmd(0x0c, 0x81);
    ssIsOn = false;
}

static void ssOn()
{
    if(ssIsOn)
        return;

    directCmd(0x0c, 0x80);
    ssIsOn = true;
}

static bool readEEPROM(unsigned long& ulv, bool &uss)
{
    byte ee1, ee2, ee3;
    
    ee1 = EEPROM.read(1);
    ee2 = EEPROM.read(2);
    ee3 = EEPROM.read(3);

    if(ee1 == (((ee2 + 0x55) ^ (ee3 + 0xaa)) & 0xff)) {
        ulv = (ee2 << 8) | ee3;
        ee1 = EEPROM.read(6);
        ee2 = EEPROM.read(7);
        if((ee1 ^ 0xaa) == ee2) {
            uss = !!ee1;
        }
        return true;
    }

    ulv = 0;
    uss = true;
    return false;
}

static void writeEEPROM(unsigned long ulv)
{
    byte ee1, ee2, ee3;

    ee2 = ulv >> 8;
    ee3 = ulv & 0xff;
    ee1 = (ee2 + 0x55) ^ (ee3 + 0xaa);
    
    EEPROM.update(1, ee1);
    EEPROM.update(2, ee2);
    EEPROM.update(3, ee3);
}

static void writeEEPROMSS(bool uss)
{
    byte ee1, ee2;

    ee1 = uss ? 1 : 0;
    ee2 = ee1 ^ 0xaa;
    
    EEPROM.update(6, ee1);
    EEPROM.update(7, ee2);
}

static bool checkDispTest()
{
    byte ee1, ee2;
    
    ee1 = EEPROM.read(4);
    ee2 = EEPROM.read(5);
    if((ee1 ^ 0x5a) == ee2) {
        if(ee1 > 3) return false;
        ee1++;
    } else {
        ee1 = 0;
    }
    ee2 = ee1 ^ 0x5a;

    EEPROM.update(4, ee1);
    EEPROM.update(5, ee2);

    return true;
}
