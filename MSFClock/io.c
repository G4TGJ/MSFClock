/*
 * io.c
 *
 * Created: 28/03/2021 13:15:39
 *  Author: Richard Tomlinson G4TGJ
 */ 

 #include <avr/interrupt.h>
 #include <util/atomic.h>

 #include "config.h"

 static volatile uint32_t magsq;
 static volatile uint32_t maxmag;
 static volatile uint32_t average;
 static volatile bool bSignal;

 #define NUM_SAMPLES           28
 #define NUM_AVERAGE_SAMPLES 1024

 #define THRESHOLD (NUM_SAMPLES*average)

static uint32_t threshold, prevThreshold;

 ISR (ADC_vect)
{
    static uint8_t count;
    count++;
    if( count >= 13)
    {
        DEBUG_OUTPUT_PIN_REG = (1<<DEBUG_OUTPUT_PIN);
        count = 0;

        static int32_t q0, q1, q2;
        static uint8_t gCount;
        int32_t sample = ((int16_t) ADCH) - 128;
        q0 = sample - q2;
        q2 = q1;
        q1 = q0;

        average = (average * (NUM_AVERAGE_SAMPLES-1)  + sample*sample) / NUM_AVERAGE_SAMPLES;

        gCount++;
        if( gCount == NUM_SAMPLES )
        {
            magsq =  q1*q1 +  q2*q2;

            if( magsq > threshold )
            {
                prevThreshold = threshold = NUM_SAMPLES * average;
                LED_OUTPUT_PORT_REG |= (1<<LED_OUTPUT_PIN);
                bSignal = true;
            }
            else
            {
                threshold = prevThreshold * 8;
                LED_OUTPUT_PORT_REG &= ~(1<<LED_OUTPUT_PIN);
                bSignal = false;
            }

            gCount = 0;
            q1 = q2 = 0;
        }
    }
}

bool getSignal(void)
{
    return bSignal;
}

uint32_t getMagnitude(void)
{
    uint32_t mag;
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        mag = magsq;
    }
    return mag;
}

uint32_t getAverage(void)
{
    uint32_t ave;
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
        ave = threshold;
    }
    return ave;
}

void ioInit()
{
#ifdef LED_OUTPUT_DDR_REG
    // Initialise LED output
    LED_OUTPUT_DDR_REG |= (1<<LED_OUTPUT_PIN);
#endif
#ifdef DEBUG_OUTPUT_DDR_REG
// Initialise debug output
DEBUG_OUTPUT_DDR_REG |= (1<<DEBUG_OUTPUT_PIN);
#endif
    // Initialise RX input
    RX_INPUT_PORT_REG |= (1<<RX_INPUT_PIN);

    // Setup timer 0 to produce RX clock
#ifdef DDRD
    DDRD |= (1<<PD6);
#else
    DDRB |= (1<<PB2);
#endif
    TCCR0A = ((1<<WGM01) | (0<<WGM00));
    TCCR0B = ((1<<CS00));
    OCR0A = (F_CPU / CLOCK_FREQUENCY / 2);
    TCCR0A |= (1<<COM0A0);

    // Set up the ADC
    ADMUX = (1<<REFS0) | (1<<ADLAR);
    DIDR0 = (1<<ADC0D);
    ADCSRA = (1<<ADEN) | (1<<ADSC) | (1<<ADATE) | (1<<ADIE) | (1<<ADPS2) | (0<<ADPS1) | (1<<ADPS0);

    // Turn on the pull-ups on unused pins
    PORTC = (1<<PORTC1) | (1<<PORTC2) | (1<<PORTC3) | (1<<PORTC4) | (1<<PORTC5);
    PORTD = (1<<PORTD0) | (1<<PORTD1) | (1<<PORTD2) | (1<<PORTD3) | (1<<PORTD7);
}

#if 0 //def LED_OUTPUT_DDR_REG
// Set the LED output high
void ioWriteLEDOutputHigh()
{
    LED_OUTPUT_PORT_REG |= (1<<LED_OUTPUT_PIN);
}

// Set the LED output low
void ioWriteLEDOutputLow()
{
    LED_OUTPUT_PORT_REG &= ~(1<<LED_OUTPUT_PIN);
}

void ioToggleLEDOutput()
{
    LED_OUTPUT_PIN_REG = (1<<LED_OUTPUT_PIN);
}
#else
void ioWriteLEDOutputHigh()
{
}

// Set the LED output low
void ioWriteLEDOutputLow()
{
}

void ioToggleLEDOutput()
{
}
#endif

// Read the RX input signal
// true means the carrier is present
bool ioReadRXInput()
{
    return !(RX_INPUT_PIN_REG & (1<<RX_INPUT_PIN));
}
