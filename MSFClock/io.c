/*
 * io.c
 *
 * Implements the Goertzel algorithm on the ADC input
 * to detect the MSF clock signal after the NE602 mixer.
 *
 * By sampling at 4 times the frequency the algorithm
 * is very simple.
 *
 * Created: 28/03/2021 13:15:39
 *  Author: Richard Tomlinson G4TGJ
 */ 

 #include <avr/interrupt.h>

 #include "config.h"

// The number of samples for calculating the signal magnitude
#define NUM_SAMPLES 28

// The number of samples over which we will average the signal
// to decide the threshold for deciding the carrier is present
// Needs to be a lot more that the number of goertzel samples
#define NUM_AVERAGE_SAMPLES 1024

// To get the correct sample rate we only process every so
// many samples
#define SAMPLE_COUNT 14

// The square of the magnitude of the clock signal as
// calculated by the goertzel algorithm
static volatile uint32_t magsq;

// The average signal - used to scale the threshold
// for the clock signal
static volatile uint32_t average;

// True when the signal is present
static volatile bool bSignal;

// The threshold for deciding the clock signal is present
// Also keep the previous threshold so we can apply hysteresis
static uint32_t threshold, prevThreshold;

// A to D interrupt complete vector
 ISR (ADC_vect)
{
    // To get the correct sample rate only process every n samples
    static uint8_t count;
    count++;
    if( count >= SAMPLE_COUNT )
    {
        count = 0;

#ifdef DEBUG_OUTPUT_PIN_REG
DEBUG_OUTPUT_PIN_REG = (1<<DEBUG_OUTPUT_PIN);
#endif

        // The values for the goertzel algorithm
        static int32_t q0, q1, q2;

        // The number of samples we have processed for goertzel
        static uint8_t gCount;

        // Scale the sample from 8 bit unsigned to a signed number
        int32_t sample = ((int16_t) ADCH) - 128;

        // Process the Goertzel algorithm
        q0 = sample - q2;
        q2 = q1;
        q1 = q0;

        // Keep a moving average of the signal magnitude squared
        // We use this to determine the threshold
        average = (average * (NUM_AVERAGE_SAMPLES-1)  + sample*sample) / NUM_AVERAGE_SAMPLES;

        // Check if we have processed enough samples to calculate the magnitude
        gCount++;
        if( gCount == NUM_SAMPLES )
        {
            // Calculate the magnitude squared
            magsq =  q1*q1 +  q2*q2;

            // Is the signal strong enough?
            if( magsq > threshold )
            {
                // Once the signal is detected it only needs to remain above a low threshold
                prevThreshold = threshold = NUM_SAMPLES * average;
                LED_OUTPUT_PORT_REG |= (1<<LED_OUTPUT_PIN);
                bSignal = true;
            }
            else
            {
                // To help eliminate false signals set the threshold
                // to be much higher than previously
                threshold = prevThreshold * 4;
                LED_OUTPUT_PORT_REG &= ~(1<<LED_OUTPUT_PIN);
                bSignal = false;
            }

            // Restart the Goertzel algorithm
            gCount = 0;
            q1 = q2 = 0;
        }
    }
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

    // Setup timer 0 to produce RX clock
    // This is the LO for the NE602
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
    // Use AVCC as voltage reference and left adjust result (for 8 bit samples)
    ADMUX = (1<<REFS0) | (1<<ADLAR);

    // Disable the digital circuitry on the pin
    DIDR0 = (1<<ADC0D);

    // Enable the ADC, start conversion, enable auto triggering, enable completion interrupt,
    // prescale by 32
    ADCSRA = (1<<ADEN) | (1<<ADSC) | (1<<ADATE) | (1<<ADIE) | (1<<ADPS2) | (0<<ADPS1) | (1<<ADPS0);

    // Turn on the pull-ups on unused pins
    PORTC = (1<<PORTC1) | (1<<PORTC2) | (1<<PORTC3) | (1<<PORTC4) | (1<<PORTC5);
    PORTD = (1<<PORTD0) | (1<<PORTD1) | (1<<PORTD2) | (1<<PORTD3) | (1<<PORTD7);
}

// Read the RX input signal
// true means the carrier is present
bool ioReadRXInput()
{
    return bSignal;
}
