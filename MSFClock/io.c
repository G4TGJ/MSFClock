/*
 * io.c
 *
 * Created: 28/03/2021 13:15:39
 *  Author: Richard Tomlinson G4TGJ
 */ 

 #include "config.h"

void ioInit()
{
    // Initialise LED output
    LED_OUTPUT_DDR_REG |= (1<<LED_OUTPUT_PIN);

    // Initialise RX input
    RX_INPUT_PORT_REG |= (1<<RX_INPUT_PIN);

    // Setup timer 0 to produce RX clock
    DDRD |= (1<<PD6);
    TCCR0A = ((1<<WGM01) | (0<<WGM00));
    TCCR0B = ((1<<CS00));
    OCR0A = (F_CPU / CLOCK_FREQUENCY / 2);
    TCCR0A |= (1<<COM0A0);
}

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

// Read the RX input signal
bool ioReadRXInput()
{
    return !(RX_INPUT_PIN_REG & (1<<RX_INPUT_PIN));
}

void ioToggleLEDOutput()
{
    LED_OUTPUT_PIN_REG = (1<<LED_OUTPUT_PIN);
}

