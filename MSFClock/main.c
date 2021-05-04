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

#include "config.h"
#include "millis.h"
#include "io.h"
#include "display.h"
#include "i2c.h"

#ifdef DEBUG
#include "serial.h"
#endif

// Positions in the MSF data for the date and time data
// plus the areas covered by parity checks
#define YEAR_START          17
#define YEAR_LEN             8
#define YEAR_PARITY         54

#define MONTH_PARITY_START  25
#define MONTH_PARITY_END    35
#define MONTH_PARITY        55

#define MONTH_START         25
#define MONTH_LEN            5

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

// The number of bits of data potentially received from MSF
// Have to allow for a leap second and the data being stored
// starting at 1
#define NUM_BITS 62

// Store the A and B bits received from MSF
static bool bitA[NUM_BITS];
static bool bitB[NUM_BITS];

// Buffer used for debug and display
static char buf[50];

// The current time and date as received from MSF - these are local time
// so may be one hour ahead of UTC
// Initialise to something to display while acquiring the time from MSF
static uint8_t currentYear = 1, currentMonth = 1, currentDate = 1, currentDay = MONDAY;
static uint8_t currentHour, currentMinute, currentSecond;
static bool bDaylightSavings;

// UTC versions of the time and date
// Do not need UTC minute and second as these are always the same as local time
static uint8_t utcDay;
static uint8_t utcDate;
static uint8_t utcMonth;
static uint8_t utcYear;
static uint8_t utcHour;

// The current data bit position we are receiving from MSF
static uint8_t currentBit;

// The AVR millisecond count for the last second
// Use to see if we have missed an MSF second and so need to
// autonomously increment the time
static uint32_t lastSecond;

// The millisecond count for the last second pulse receive from MSF
// Used to display if second pulses are being received
static uint32_t lastSecondPulse;

// The millisecond count for the last time we got a minute pulse
// Used to display if we are getting minute pulses
static uint32_t lastMinute;

// Do we have a good signal, are we receiving second and minute pulses?
static bool bGoodSignal, bGoodSecond, bGoodMinute;

// The current DUT1
static int8_t dut1;

static void convertTimeUTC(void);

// Initialise the RTC chip
static void initRTC(void)
{
    // Disable the 32kHz and 1Hz outputs
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_STATUS, 0);
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_CONTROL, 1<<RTC_CONTROL_INTCN);
}

// Read the time from the RTC chip
static void readRTCTime(void)
{
    i2cReadRegister(RTC_ADDRESS, RTC_REG_SECONDS, &currentSecond);
    currentSecond = BCD_TO_BIN( currentSecond );
    i2cReadRegister(RTC_ADDRESS, RTC_REG_MINUTES, &currentMinute);
    currentMinute = BCD_TO_BIN( currentMinute );
    i2cReadRegister(RTC_ADDRESS, RTC_REG_HOURS, &currentHour);
    currentHour = BCD_TO_BIN( currentHour );
    i2cReadRegister(RTC_ADDRESS, RTC_REG_DAY, &currentDay);
    currentDay--; // MSF has days as 0-6 but RTC is 1-7
    i2cReadRegister(RTC_ADDRESS, RTC_REG_DATE, &currentDate);
    currentDate = BCD_TO_BIN( currentDate );
    i2cReadRegister(RTC_ADDRESS, RTC_REG_MONTH, &currentMonth);
    currentMonth = BCD_TO_BIN( currentMonth );
    i2cReadRegister(RTC_ADDRESS, RTC_REG_YEAR, &currentYear);
    currentYear = BCD_TO_BIN( currentYear );

    // Nowhere to store the daylight saving setting in the RTC so
    // always store the time in UTC.
    bDaylightSavings = false;

    // Convert it to UTC
    convertTimeUTC();
}

// Write the UTC time to the RTC chip
// We do this every minute
static void writeRTCTime(void)
{
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_SECONDS, BIN_TO_BCD(currentSecond));
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_MINUTES, BIN_TO_BCD(currentMinute));
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_HOURS, BIN_TO_BCD(utcHour));
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_DAY, utcDay+1);  // MSF has days as 0-6 but RTC is 1-7
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_DATE, BIN_TO_BCD(utcDate));
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_MONTH, BIN_TO_BCD(utcMonth));
    i2cWriteRegister(RTC_ADDRESS, RTC_REG_YEAR, BIN_TO_BCD(utcYear));
}

#ifdef DEBUG
static void displayBits( uint8_t *bits )
{
    uint8_t i;
    for( i = 0 ; i < NUM_BITS ; i++ )
    {
        switch(i)
        {
            case 17:
            case 25:
            case 30:
            case 36:
            case 39:
            case 45:
            case 52:
                sprintf( buf, " %d: ", i );
                serialTXString(buf);
                break;

            default:
                break;
        }
        sprintf( buf, "%d", bits[i] );
        serialTXString(buf);
    }
    serialTXString( "\r\n" );
}

static void badData( char *text )
{
    serialTXString(text);
    serialTXString("A: ");
    displayBits( bitA );
    serialTXString("B: ");
    displayBits( bitB );
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

// Checks that the minute identifier within the MSF data is correct
static bool checkMinuteIdentifier()
{
    bool bId =  !bitA[52]
             &&  bitA[53]
             &&  bitA[54]
             &&  bitA[55]
             &&  bitA[56]
             &&  bitA[57]
             &&  bitA[58]
             && !bitA[59];

#ifdef DEBUG
    if( !bId )
    {
        sprintf(buf, "ID: %d%d%d%d%d%d%d%d\n\r", 
            bitA[52],
            bitA[53],
            bitA[54],
            bitA[55],
            bitA[56],
            bitA[57],
            bitA[58],
            bitA[59]
        );
        serialTXString(buf);
    }
#endif

    return bId;
}

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

// Converts the time to UTC
static void convertTimeUTC(void)
{
    // We may need to adjust the time to take into account daylight savings
    utcDay = currentDay;
    utcDate = currentDate;
    utcMonth = currentMonth;
    utcYear = currentYear;
    utcHour = currentHour;

    // If we are on daylight savings then we need to go back one hour to get UTC
    // This may also mean going back to the previous day
    if( bDaylightSavings )
    {
        // If in the first hour of the clock day then must go back to the
        // previous day
        if( utcHour == 0 )
        {
            utcHour = 23;

            // If the first of the month then go back to the previous month
            if( utcDate == 1 )
            {
                if( utcMonth == 1 )
                {
                    utcMonth = 12;
                    utcYear--;
                }
                else
                {
                    utcMonth--;
                }

                // The date will be the last day of the previous month
                utcDate = getDaysInMonth(utcMonth, utcYear);
            }
            else
            {
                utcDate--;
            }

            // Also need to go to the previous day of the week
            if( utcDay == SUNDAY )
            {
                utcDay = SATURDAY;
            }
            else
            {
                utcDay--;
            }
        }
        else
        {
            utcHour--;
        }
    }
}

// Displays the time
static void displayTime(void)
{
#ifdef DEBUG
    sprintf( buf, "%s %u/%u/%u %02u:%02u:%02u UTC %s\r\n", convertDay(utcDay), utcDate, utcMonth, utcYear, utcHour, currentMinute, currentSecond, bGoodSignal ? "OK" : "Lost" );
    //serialTXString( buf );
#endif

#if 0
    static uint32_t secondCount;
    secondCount++;
    sprintf( buf, "%lu", secondCount );
#else
    sprintf( buf, "%s %02u/%02u/%02u   %c", convertDay(utcDay), utcDate, utcMonth, utcYear, bGoodSignal ? '*' : ' ' );
#endif
    displayText(0, buf, true);
    sprintf( buf, "%02u:%02u:%02u UTC  %c%c", utcHour, currentMinute, currentSecond, bGoodSignal ? (dut1 >= 0 ? '+' : '-') : bGoodMinute ?  'M' : 'm', bGoodSignal ? (dut1 >= 0 ? dut1 + '0' : '0' - dut1) : bGoodSecond ? 'S' : 's');
    displayText(1, buf, true);
}

// Process the data received from MSF over the last minute
static void processRXData(void)
{
    // We will read the data in but only set the values if all the
    // parity checks are OK and the data is sensible. Needed because
    // the checks are not very sophisticated and won't detect all
    // errors.
    uint8_t year = 0, month = 1, date = 1, day = 1, hour = 0, minute = 0, second = 0;

    // For processing the DUT1 value
    uint8_t posDutCount;
    int8_t posDut1 = 0;
    uint8_t negDutCount;
    int8_t negDut1 = 0;

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
            year = convertBCD(YEAR_START, YEAR_LEN);
            if( year > 99 )
            {
                bGoodSignal = false;
#ifdef DEBUG
                badData("Bad year\r\n");
#endif
            }
        }
        else
        {
            bGoodSignal = false;
#ifdef DEBUG
            badData("Bad year parity\r\n");
#endif
        }

        if( checkParity(MONTH_PARITY_START, MONTH_PARITY_END, MONTH_PARITY) )
        {
            month = convertBCD(MONTH_START, MONTH_LEN);
            if( month < JANUARY || month > DECEMBER )
            {
                bGoodSignal = false;
#ifdef DEBUG
                badData("Bad month\r\n");
#endif
            }

            date = convertBCD(DATE_START, DATE_LEN);
            if( date < 1 || date > getDaysInMonth(currentMonth, currentYear) )
            {
                bGoodSignal = false;
#ifdef DEBUG
                badData("Bad date\r\n");
#endif
            }
        }
        else
        {
            bGoodSignal = false;
#ifdef DEBUG
            badData("Bad date parity\r\n");
#endif
        }

        if( checkParity(DAY_START, DAY_START + DAY_LEN - 1, DAY_PARITY) )
        {
            day = convertBCD(DAY_START, DAY_LEN);
            if( day > LAST_DAY )
            {
                bGoodSignal = false;
#ifdef DEBUG
                badData("Bad day\r\n");
#endif
            }
        }
        else
        {
            bGoodSignal = false;
#ifdef DEBUG
            badData("Bad day parity\r\n");
#endif
        }

        if( checkParity(TIME_PARITY_START, TIME_PARITY_END, TIME_PARITY) )
        {
            hour = convertBCD(HOUR_START, HOUR_LEN);
            if( hour > 23 )
            {
                bGoodSignal = false;
#ifdef DEBUG
                badData("Bad hour\r\n");
#endif
            }

            minute = convertBCD(MINUTE_START, MINUTE_LEN);
            if( minute > 59 )
            {
                bGoodSignal = false;
#ifdef DEBUG
                badData("Bad minute\r\n");
#endif
            }

            // Process the DUT1 code
            dut1 = 0;

            // Start by counting the positive bits. Then check that if bit n is set there are n bits set.
            posDutCount = bitB[1] + bitB[2] + bitB[3] + bitB[4] + bitB[5] + bitB[6] + bitB[7] + bitB[8];
            posDut1 = 0;
            if( bitB[8] )
            {
                if( posDutCount == 8 )
                {
                    posDut1 = 8;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 8\r\n");
#endif
                }
            }
            else if( bitB[7] )
            {
                if( posDutCount == 7 )
                {
                    posDut1 = 7;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 7\r\n");
#endif
                }
            }
            else if( bitB[6] )
            {
                if( posDutCount == 6 )
                {
                    posDut1 = 6;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 6\r\n");
#endif
                }
            }
            else if( bitB[5] )
            {
                if( posDutCount == 5 )
                {
                    posDut1 = 5;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 5\r\n");
#endif
                }
            }
            else if( bitB[4] )
            {
                if( posDutCount == 4 )
                {
                    posDut1 = 4;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 4\r\n");
#endif
                }
            }
            else if( bitB[3] )
            {
                if( posDutCount == 3 )
                {
                    posDut1 = 3;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 3\r\n");
#endif
                }
            }
            else if( bitB[2] )
            {
                if( posDutCount == 2 )
                {
                    posDut1 = 2;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 2\r\n");
#endif
                }
            }
            else if( bitB[1] )
            {
                if( posDutCount == 1 )
                {
                    posDut1 = 1;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut 1\r\n");
#endif
                }
            }

            // Now count the negative bits. Then check that if bit n is set there are n bits set.
            negDutCount = bitB[9] + bitB[10] + bitB[11] + bitB[12] + bitB[13] + bitB[14] + bitB[15] + bitB[16];
            negDut1 = 0;
            if( bitB[16] )
            {
                if( negDutCount == 8 )
                {
                    negDut1 = -8;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -8\r\n");
#endif
                }
            }
            else if( bitB[15] )
            {
                if( negDutCount == 7 )
                {
                    negDut1 = -7;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -7\r\n");
#endif
                }
            }
            else if( bitB[14] )
            {
                if( negDutCount == 6 )
                {
                    negDut1 = -6;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -6\r\n");
#endif
                }
            }
            else if( bitB[13] )
            {
                if( negDutCount == 5 )
                {
                    negDut1 = -5;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -5\r\n");
#endif
                }
            }
            else if( bitB[12] )
            {
                if( negDutCount == 4 )
                {
                    negDut1 = -4;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -4\r\n");
#endif
                }
            }
            else if( bitB[11] )
            {
                if( negDutCount == 3 )
                {
                    negDut1 = -3;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -3\r\n");
#endif
                }
            }
            else if( bitB[10] )
            {
                if( negDutCount == 2 )
                {
                    negDut1 = -2;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -2\r\n");
#endif
                }
            }
            else if( bitB[9] )
            {
                if( negDutCount == 1 )
                {
                    negDut1 = -1;
                }
                else
                {
                    bGoodSignal = false;
#ifdef DEBUG
                    badData("Bad dut -1\r\n");
#endif
                }
            }

        }
        else
        {
            bGoodSignal = false;
#ifdef DEBUG
            badData("Bad time parity\r\n");
#endif
        }
    }
    else
    {
        bGoodSignal = false;
#ifdef DEBUG
        badData("Bad minute marker\r\n");
#endif
    }

    // If everything received OK then can update the DUT
    if( bGoodSignal )
    {
        // Cannot have both positive and negative DUT1 signals
        if( posDut1)
        {
            if( negDut1 )
            {
                bGoodSignal = false;
#ifdef DEBUG
                badData("Bad dut pos/neg\r\n");
#endif
            }
            else
            {
                dut1 = posDut1;
            }
        }
        else
        {
            dut1 = negDut1;
        }
    }

    // If everything received OK then can update the time
    if( bGoodSignal )
    {
        currentYear = year;
        currentMonth = month;
        currentDate = date;
        currentDay = day;
        currentHour = hour;
        currentMinute = minute;
        currentSecond = second;

        // The daylight savings bit is not protected in any way
        bDaylightSavings = bitB[58];

        // Convert the received time to UTC if necessary
        convertTimeUTC();

        // Write to the RTC chip
        writeRTCTime();
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
    //serialTXString(buf);
#endif

#ifdef DEBUG
    sprintf( buf, "\r\ncurrentTime %lu  lastSecond %lu ", currentTime, lastSecond );
    //serialTXString(buf);
#endif

    // Keep track of the AVR clock time for this second
    // If we lose the MSF signal we can still make our own second tick
    lastSecond = currentTime;

    // Move to the next second. May have to increment minutes, hours, date, day
    // Use >= instead of == just in case we end up with an odd time or date
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
    // Note the time at which the signal has gone high or low
    static uint32_t highTime, lowTime;

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
    if( signal != bSignal )
    {
        bSignal = signal;
        if( bSignal )
        {
            highTime = currentTime;

            // Going high after at least 400ms is a minute marker
            // Only accept this if we are getting good second pulses
            // Otherwise regaining the signal after loss looks like a minute
            // pulse.
            if( bGoodSecond && (currentTime - lowTime > 400) )
            {
                bGoodMinute = true;

                // Note the time we got the minute pulse
                lastMinute = currentTime;

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
        else
        {
            // Keep track of how long the signal is low
            lowTime = currentTime;

            // Going low after at least 400ms high is a new second
            if( currentTime - highTime > 400 )
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

                // Note the time we got the pulse
                // Used to display if second pulses are being received
                lastSecondPulse = currentTime;

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
                //serialTXString(buf);
#endif
                break;

            case A0:
                eState = B0;
                nextTimeout = currentTime + 100;
                bitA[currentBit] = 0;
#ifdef DEBUG
                sprintf(buf, "A(%u)=0 ", currentBit);
                //serialTXString(buf);
#endif
                break;

            case B1:
                eState = IDLE;
                bitB[currentBit] = 1;
#ifdef DEBUG
                sprintf(buf, "B(%u)=1 ", currentBit);
                //serialTXString(buf);
#endif
                break;

            case B0:
                eState = IDLE;
                bitB[currentBit] = 0;
#ifdef DEBUG
                sprintf(buf, "B(%u)=0 ", currentBit);
                //serialTXString(buf);
#endif
                break;

            default:
                break;
        }
    }
}

// Handle data received from MSF.
static void handleRX(uint32_t currentTime)
{
    // Read the current carrier state
    bool carrier = ioReadRXInput();

    static uint32_t previousTime;

    // The current state of the carrier
    static bool currentCarrier;

    // If the state has changed then note the time
    if( carrier != currentCarrier )
    {
        previousTime = currentTime;
        currentCarrier = carrier;
    }

    // Only process the carrier state if it has been stable for long enough
    if( currentTime - previousTime > 20 )
    {
        if( currentCarrier )
        {
            processRX( true, currentTime );
        }
        else
        {
            processRX( false, currentTime );
        }
    }
}

// If we lose the MSF signal the clock must carry on
void autonomousClock( uint32_t currentTime )
{
    // If it has been a lot more than a second since we last
    // received a second pulse then we'll force one.
    if( (currentTime - lastSecond) >= 1200 )
    {
        // The signal is missing
        bGoodSignal = false;

        // Use the time when the second should have started
        newSecond(currentTime - 200);
    }

    // If it has been a lot more than a second since we last
    // received a second pulse then will display that fact
    if( (currentTime - lastSecondPulse) >= 1200 )
    {
        bGoodSecond = false;
    }

    // If it has been more than a minute since we last
    // received a minute pulse then we'll display that fact
    if( (currentTime - lastMinute) >= 61000 )
    {
        bGoodMinute = false;
    }
}

static void loop(void)
{
    uint32_t currentTime = millis();
    handleRX(currentTime);
    autonomousClock(currentTime);
}

int main(void)
{
    millisInit();
    ioInit();

#ifdef DEBUG
    serialInit(57600);
#endif

    displayInit();

    i2cInit();

    // Read the time from the RTC chip
    initRTC();
    readRTCTime();

#ifdef DEBUG
    serialTXString("G4TGJ MSF Clock\r\n\r\n");
#endif

    while (1) 
    {
        loop();
    }
}

