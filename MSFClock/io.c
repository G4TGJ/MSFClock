/*
 * io.c
 *
 * Created: 28/03/2021 13:15:39
 *  Author: Richard Tomlinson G4TGJ
 */ 

 #include "config.h"

void ioInit()
{
#ifdef LED_OUTPUT_DDR_REG
    // Initialise LED output
    LED_OUTPUT_DDR_REG |= (1<<LED_OUTPUT_PIN);
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
}

#ifdef LED_OUTPUT_DDR_REG
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
