/*
 * config.h
 *
 * Created: 28/03/2021 13:07:38
 *  Author: Richard Tomlinson G4TGJ
 */ 


#ifndef CONFIG_H_
#define CONFIG_H_

#include <avr/io.h>

// General definitions
typedef uint8_t bool;
#define true 1
#define false 0

#define F_CPU 16000000UL

#define LED_OUTPUT_PORT_REG   PORTB
#define LED_OUTPUT_PIN_REG    PINB
#define LED_OUTPUT_DDR_REG    DDRB
#define LED_OUTPUT_PIN        PORTB5

#define RX_INPUT_PORT_REG   PORTD
#define RX_INPUT_PIN_REG    PIND
#define RX_INPUT_DDR_REG    DDRD
#define RX_INPUT_PIN        PORTD2

#define CLOCK_FREQUENCY 59275

// Serial port buffer lengths
// Lengths should be a power of 2 for efficiency
#define SERIAL_RX_BUF_LEN 32
#define SERIAL_TX_BUF_LEN 256

// LCD Port definitions
#define LCD_ENABLE_PORT PORTD
#define LCD_ENABLE_DDR  DDRD
#define LCD_ENABLE_PIN  PD4
#define LCD_RS_PORT     PORTD
#define LCD_RS_DDR      DDRD
#define LCD_RS_PIN      PD5
#define LCD_DATA_PORT_0 PORTB
#define LCD_DATA_DDR_0  DDRB
#define LCD_DATA_PIN_0  PB0
#define LCD_DATA_PORT_1 PORTB
#define LCD_DATA_DDR_1  DDRB
#define LCD_DATA_PIN_1  PB1
#define LCD_DATA_PORT_2 PORTB
#define LCD_DATA_DDR_2  DDRB
#define LCD_DATA_PIN_2  PB2
#define LCD_DATA_PORT_3 PORTB
#define LCD_DATA_DDR_3  DDRB
#define LCD_DATA_PIN_3  PB3

#define LCD_WIDTH 16
#define LCD_HEIGHT 2

#endif /* CONFIG_H_ */