/*
 * MSF Clock
 *
 * main.c
 *
 * Created: 28/03/2021 13:02:08
 *  Author: Richard Tomlinson G4TGJ
 */ 

#include <avr/io.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#include "config.h"
#include "millis.h"
#include "io.h"
#include "display.h"

#ifdef DEBUG
#include "serial.h"
#endif

// The number of bits of data potentially received from MSF
// Have to allow for a leap second and the data being stored
// starting at 1
#define NUM_BITS 62

// Store the A and B bits received from MSF
static bool bitA[NUM_BITS];
static bool bitB[NUM_BITS];

// Buffer used for debug and display
static char buf[50];

// The current time and date
static uint8_t currentYear, currentMonth, currentDate, currentDay;
static uint8_t currentHour, currentMinute, currentSecond;
static bool bDaylightSavings;

// The current data bit position we are receiving from MSF
static uint8_t currentBit;

// The AVR millisecond count for the last second
// Use to see if we have missed an MSF second and so need to
// autonomously increment the time
static uint32_t lastSecond;

// The offset between an MSF second and the AVR's internal second
static int32_t currentOffset;

// Have we ever acquired a signal? Can't display the time until we have
static bool bAcquired;

// Do we have a good signal, are we receiving second and minute pulses?
static bool bGoodSignal, bGoodSecond, bGoodMinute;

#ifdef LED_OUTPUT_DDR_REG
void flashLED(void)
{
    static bool ledOn;
    static uint32_t previous;
    uint32_t current = millis();

    if( (current - previous) >= 1000 )
    {
        previous = current;

        if( ledOn )
        {
            ledOn = false;
            ioWriteLEDOutputLow();
            displayText(0,"On ", false);
        }
        else
        {
            ledOn = true;
            ioWriteLEDOutputHigh();
            displayText(0,"Off ", false);
        }
    }
}
#endif

// Checks the parity of a set of MSF bits
static bool checkParity( uint8_t start, uint8_t finish, uint8_t parity )
{
    uint8_t count = 0;

    for( uint8_t i = start ; i <= finish ; i++ )
    {
        count += bitA[i];
    }
    count += bitB[parity];

    // Parity is OK if the count of bits is odd
    return count & 1;
}

// Converts a BCD number as received from MSF into binary
static uint8_t convertBCD( uint8_t start, uint8_t len )
{
    uint8_t val = 0;

#define NUM_BCD_DIGITS 8
    static const uint8_t bcdDigit[NUM_BCD_DIGITS] = {80, 40, 20, 10, 8, 4, 2, 1};

    uint8_t digit = NUM_BCD_DIGITS - len;

    for( uint8_t i = start ; i < (start + len) ; i++ )
    {
        val += (bitA[i] * bcdDigit[digit++]);
    }

    return val;
}

#define YEAR_START  17
#define YEAR_LEN    8
#define YEAR_PARITY 54

#define MONTH_PARITY_START  25
#define MONTH_PARITY_END    35
#define MONTH_PARITY        55

#define MONTH_START  25
#define MONTH_LEN    5

#define DATE_START          30
#define DATE_LEN             6

#define DAY_START           36
#define DAY_LEN              3
#define DAY_PARITY          56

#define HOUR_START          39
#define HOUR_LEN             6

#define MINUTE_START        45
#define MINUTE_LEN           7

#define TIME_PARITY_START   39
#define TIME_PARITY_END     51
#define TIME_PARITY         57

// Checks that the minute identifier within the MSF data is correct
static bool checkMinuteIdentifier()
{
    return  !bitA[52]
         &&  bitA[53]
         &&  bitA[54]
         &&  bitA[55]
         &&  bitA[56]
         &&  bitA[57]
         &&  bitA[58]
         && !bitA[59];
}

enum
{
    SUNDAY = 0,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    NUM_DAYS
};

#define LAST_DAY SATURDAY

static const char *dayText[NUM_DAYS] =
{
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
};

// Converts an MSF day number into text
static const char *convertDay( uint8_t day )
{
    if( day <= LAST_DAY )
    {
        return dayText[day];
    }
    else
    {
        return "Unknown";
    }
}

enum
{
    JANUARY = 1,
    FEBRUARY,
    MARCH,
    APRIL,
    MAY,
    JUNE,
    JULY,
    AUGUST,
    SEPTEMBER,
    OCTOBER,
    NOVEMBER,
    DECEMBER
};

#define NUM_MONTHS DECEMBER

// Number of days in a month
const uint8_t daysInMonth[NUM_MONTHS+1] =
{
    0,      
    31, // January
    28, // February
    31, // March
    30, // April
    31, // May
    30, // June
    31, // July
    31, // August
    30, // September
    31, // October
    30, // November
    31  // December
};

// Returns the number of days in a month allowing for leap years
// The year is from 00-99 so will never be a century year in the lifetime
// of this project (or me)
static uint8_t getDaysInMonth( uint8_t month, uint8_t year )
{
    uint8_t days = 31;

    if( month == FEBRUARY )
    {
        if( year % 4 )
        {
            // Not divisible by 4
            days = 28;
        }
        else
        {
            // Divisible by 4
            days = 29;
        }
    }
    else if( month <= NUM_MONTHS )
    {
        days = daysInMonth[month];
    }

    return days;
}

// Displays the time
static void displayTime(void)
{
    // Cannot display the time if we have never acquired it from MSF
    if( bAcquired )
    {
        //displayText(1, "Got the time", true);
        // We may need to adjust the time to take into account daylight savings
        uint8_t displayDay = currentDay;
        uint8_t displayDate = currentDate;
        uint8_t displayMonth = currentMonth;
        uint8_t displayYear = currentYear;
        uint8_t displayHour = currentHour;
        uint8_t displayMinute = currentMinute;
        uint8_t displaySecond = currentSecond;

        // If we are on daylight savings then we need to go back one hour to get UTC
        // This may also mean going back to the previous day
        if( bDaylightSavings )
        {
            // If in the first hour of the clock day then must go back to the
            // previous day
            if( displayHour == 0 )
            {
                displayHour = 23;

                // If the first of the month then go back to the previous month
                if( displayDate == 1 )
                {
                    displayMonth--;

                    // The date will be the last day of the previous month
                    displayDate = getDaysInMonth(displayMonth, displayYear);
                }
                else
                {
                    displayDate--;
                }

                // Also need to go to the previous day of the week
                if( displayDay == SUNDAY )
                {
                    displayDay = SATURDAY;
                }
                else
                {
                    displayDay--;
                }
            }
            else
            {
                displayHour--;
            }
        }

#ifdef DEBUG
        sprintf( buf, "%s %u/%u/%u %02u:%02u:%02u UTC %s %ld\r\n", convertDay(displayDay), displayDate, displayMonth, displayYear, displayHour, displayMinute, displaySecond, bGoodSignal ? "OK" : "Lost", currentOffset );
        serialTXString( buf );
#endif

        static uint32_t secondCount;
        secondCount++;
        sprintf_P( buf, PSTR("%lu"), secondCount );
//        sprintf_P( buf, PSTR("%s %02u/%02u/%02u"), convertDay(displayDay), displayDate, displayMonth, displayYear );
        displayText(0, buf, true);
        sprintf_P( buf, PSTR("%02u:%02u:%02uZ %c%c%c"), displayHour, displayMinute, displaySecond, bGoodSignal ? 'G' : 'b', bGoodMinute ?  'M' : 'm', bGoodSecond ? 'S' : 's');
        displayText(1, buf, true);
    }
    else
    {
#ifdef DEBUG
        serialTXString("Waiting to acquire signal\n\r");
#endif
    }
}

// Process the data received from MSF over the last minute
static void processRXData(void)
{
    // If the minute identifier is wrong then the data isn't valid
    if( checkMinuteIdentifier() )
    {
        // The signal is good unless we find any parity errors
        bGoodSignal = true;
        currentSecond = 0;

        // Check the parities and if they are good read in the data
        // Check the data is sensible before setting the current value
        if( checkParity(YEAR_START, YEAR_START + YEAR_LEN - 1, YEAR_PARITY) )
        {
            uint8_t year = convertBCD(YEAR_START, YEAR_LEN);
            if( year <= 99 )
            {
                currentYear = year;
            }
            else
            {
                bGoodSignal = false;
            }
        }
        else
        {
            bGoodSignal = false;
        }

        if( checkParity(MONTH_PARITY_START, MONTH_PARITY_END, MONTH_PARITY) )
        {
            uint8_t month = convertBCD(MONTH_START, MONTH_LEN);
            if( month >= JANUARY && month <= DECEMBER )
            {
                currentMonth = month;
            }
            else
            {
                bGoodSignal = false;
            }

            uint8_t date = convertBCD(DATE_START, DATE_LEN);
            if( date >= 1 && date <= getDaysInMonth(currentMonth, currentYear) )
            {
                currentDate = date;
            }
            else
            {
                bGoodSignal = false;
            }
        }
        else
        {
            bGoodSignal = false;
        }

        if( checkParity(DAY_START, DAY_START + DAY_LEN - 1, DAY_PARITY) )
        {
            uint8_t day = convertBCD(DAY_START, DAY_LEN);
            if( day <= LAST_DAY )
            {
                currentDay = day;
            }
            else
            {
                bGoodSignal = false;
            }
        }
        else
        {
            bGoodSignal = false;
        }

        if( checkParity(TIME_PARITY_START, TIME_PARITY_END, TIME_PARITY) )
        {
            uint8_t hour = convertBCD(HOUR_START, HOUR_LEN);
            if( hour <= 23 )
            {
                currentHour = hour;
            }
            else
            {
                bGoodSignal = false;
            }

            uint8_t minute = convertBCD(MINUTE_START, MINUTE_LEN);
            if( minute <= 59 )
            {
                currentMinute = minute;
            }
            else
            {
                bGoodSignal = false;
            }

            // The daylight savings flag is not protected by parity so we will
            // only read it if everything else has been read correctly to give
            // us confidence it is correct
            if( bGoodSignal )
            {
                bDaylightSavings = bitB[58];
            }
        }
        else
        {
            bGoodSignal = false;
        }

        if( bGoodSignal )
        {
            bAcquired = true;
        }
    }
    else
    {
        bGoodSignal = false;
    }

    // Start the next minute with all the bits zeroed
    for( uint8_t i = 0 ; i < NUM_BITS ; i++ )
    {
        bitA[i] = bitB[i] = 0;
    }
}

// Called every second either because we have an MSF second tick or because we
// have missed one (so that the clock keeps going without a signal)
static void newSecond( uint32_t currentTime )
{
#ifdef DEBUG
    sprintf( buf, "\r\nSecond %u MSF bit %u ", currentSecond, currentBit );
    serialTXString(buf);
#endif

    // Record the offset between the current time and the last second
    // This tells us how accurate the AVR's clock is
    currentOffset = currentTime - lastSecond;

#ifdef DEBUG
    sprintf( buf, "\r\ncurrentTime %lu  lastSecond %lu ", currentTime, lastSecond );
    serialTXString(buf);
#endif

#if 0
    sprintf_P( buf, PSTR("c=%lu l=%lu "), currentTime, lastSecond );
    displayText(0,buf,true);
#endif

    // Keep track of the AVR clock time for this second
    // If we lose the MSF signal we can still make our own second tick
    lastSecond = currentTime;

    // Move to the next second. May have to increment minutes, hours, date, day
    // Use >= instead of == just in case we end up with an odd date
    if( currentSecond >= 59 )
    {
        currentSecond = 0;
        if( currentMinute >= 59 )
        {
            currentMinute = 0;
            if( currentHour >= 23 )
            {
                if( currentDate >= getDaysInMonth(currentMonth, currentYear) )
                {
                    // Last day of the month
                    currentDate = 1;
                    if( currentMonth >= 12 )
                    {
                        currentMonth = 1;
                        currentYear++;
                    }
                    else
                    {
                        currentMonth++;
                    }
                }
                else
                {
                    currentDate++;
                }

                currentDay = (currentDay+1) % 7;
            }
            else
            {
                currentHour++;
            }
        }
        else
        {
            currentMinute++;
        }
    }
    else
    {
        currentSecond++;
    }
    displayTime();
}

// Process the data received from MSF
static void processRX( bool signal, uint32_t currentTime )
{
    static uint32_t lowTime, highTime;

    static uint32_t nextTimeout;
    static enum
    {
        IDLE,
        NEW_SECOND,
        A1,
        A0,
        B1,
        B0
    } eState = IDLE;

    // The previous signal state
    static bool bSignal;
    
    // Process the signal if it has changed
    if( bSignal != signal )
    {
        bSignal = signal;
        if( bSignal )
        {
            // Keep track of how long the signal is high
            highTime = currentTime;

            // Going high after at least 400ms low is a new second
            if( currentTime - lowTime > 400 )
            {
                // Only trigger a new second if the signal is good to
                // prevent spurious seconds if the signal is poor
                if( bGoodSignal )
                {
                    newSecond(currentTime);
                }
                nextTimeout = currentTime + 50;
                eState = NEW_SECOND;
                bGoodSecond = true;

                // Move to the next bit of MSF data to receive but don't go
                // too far
                currentBit++;
                if( currentBit >= NUM_BITS )
                {
                    currentBit = 0;
                }
            }

            switch( eState )
            {
                case A0:
                    eState = IDLE;
                    break;

                case B0:
                    eState = B1;
                    break;

                default:
                    break;
            }
        }
        else
        {
            lowTime = currentTime;

            // Going low after at least 400ms is a minute marker
            if( currentTime - highTime > 400)
            {
                bGoodMinute = true;

                // The last second must have happened 500ms ago
                // This ensures we will use the next second pulse
                // instead of forcing a second
                lastSecond = currentTime - 500;

                // Start loading received bits at the beginning again
                currentBit = 0;

                // We have a whole minute's worth of data so process it
                processRXData();
            }

            switch( eState )
            {
                case NEW_SECOND:
                    eState = IDLE;
                    break;

                case A1:
                    eState = A0;
                    break;

                case B1:
                    eState = B0;
                    break;

                default:
                    break;
            }
        }
    }
    
    // Process a timeout
    else if( currentTime > nextTimeout )
    {
        switch( eState )
        {
            case NEW_SECOND:
                eState = A1;
                nextTimeout = currentTime + 60;
                break;

            case A1:
                eState = B1;
                nextTimeout = currentTime + 100;
                bitA[currentBit] = 1;
#ifdef DEBUG
                sprintf(buf, "A(%u)=1 ", currentBit);
                serialTXString(buf);
#endif
                break;

            case A0:
                eState = B0;
                nextTimeout = currentTime + 100;
                bitA[currentBit] = 0;
#ifdef DEBUG
                sprintf(buf, "A(%u)=0 ", currentBit);
                serialTXString(buf);
#endif
                break;

            case B1:
                eState = IDLE;
                bitB[currentBit] = 1;
#ifdef DEBUG
                sprintf(buf, "B(%u)=1 ", currentBit);
                serialTXString(buf);
#endif
                break;

            case B0:
                eState = IDLE;
                bitB[currentBit] = 0;
#ifdef DEBUG
                sprintf(buf, "B(%u)=0 ", currentBit);
                serialTXString(buf);
#endif
                break;

            default:
                break;
        }
    }
}

// Handle data received from MSF. It may be noisy so need to clean it up.
static void handleRX(uint32_t currentTime)
{
    // Read the current sample
    bool state = ioReadRXInput();

    static bool glitchState;
    static uint32_t glitchTime;
    static uint32_t previousTime;
    static bool currentState;

    bool newState = currentState;

    // First eliminate any glitches
    // If the state has changed then note the time
    if( state != glitchState )
    {
        glitchTime = currentTime;
        glitchState = state;
    }
    else if( (currentTime - glitchTime) > 1 )
    {
        // If the state has been stable for long enough then set the
        // new state
        newState = glitchState;
    }

    // Now need to process the glitch free sample
    // If the state has changed then note the time
    if( newState != currentState )
    {
        previousTime = currentTime;
        currentState = newState;
    }

    // When the signal is present it can be very noisy so assume the signal is
    // there unless it has been low for long enough
    uint32_t elapsedTime = currentTime - previousTime;
    if( !currentState && (elapsedTime > 20) )
    {
        processRX( true, currentTime );
        ioWriteLEDOutputHigh();
    }
    else
    {
        processRX( false, currentTime );
        ioWriteLEDOutputLow();
    }
}

// If we lose the MSF signal the clock must carry on
void autonomousClock( uint32_t currentTime )
{
    // If it has been a lot more than a second since we last
    // received a second then we'll force one.
    if( (currentTime - lastSecond) >= 1200 )
    {
        // The signal is missing
        bGoodSignal = bGoodSecond = bGoodMinute = false;

        // Use the time when the second should have started
        newSecond(currentTime - 200);
    }
}

static void loop(void)
{
#if 1
    uint32_t currentTime = millis();
    handleRX(currentTime);
    autonomousClock(currentTime);
    //ioToggleLEDOutput();
#else
    flashLED();
#endif
}

int main(void)
{
    millisInit();
    ioInit();

#ifdef DEBUG
    serialInit(57600);
#endif

    displayInit();

    ioWriteLEDOutputLow();

#ifdef DEBUG
    serialTXString("G4TGJ MSF Clock\r\n\r\n");
#endif

    OSCCAL = 0x87;
    sprintf(buf, "0x%02x", OSCCAL);
    displayText(0, buf, true);
//    displayText(0, "G4TGJ MSF Clock", true);
    displayText(1, "Acquiring signal", true);

    while (1) 
    {
        loop();
    }
}

