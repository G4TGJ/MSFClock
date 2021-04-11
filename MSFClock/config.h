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

#define DEBUG_OUTPUT_PORT_REG   PORTB
#define DEBUG_OUTPUT_PIN_REG    PINB
#define DEBUG_OUTPUT_DDR_REG    DDRB
#define DEBUG_OUTPUT_PIN        PORTB4

// The clock frequency generated by the AVR which is the
// LO for the NE602 mixer
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

// The size of the LCD screen
#define LCD_WIDTH 16
#define LCD_HEIGHT 2

// RTC chip I2C address
#define RTC_ADDRESS 0x68

// Convert 8 bit quantities from 0 to 99 between
// BCD and decimal
#define BCD_TO_BIN(x) (((x)&0xF) + (((x)&0xF0)>>4)*10)
#define BIN_TO_BCD(x) (((x)%10) + (((x)/10)<<4))

// Real time clock chip registers
#define RTC_REG_SECONDS 0x00
#define RTC_REG_MINUTES 0x01
#define RTC_REG_HOURS   0x02
#define RTC_REG_DAY     0x03
#define RTC_REG_DATE    0x04
#define RTC_REG_MONTH   0x05
#define RTC_REG_YEAR    0x06
#define RTC_REG_CONTROL 0x0E
#define RTC_REG_STATUS  0x0F

// RTC control register bit
#define RTC_CONTROL_INTCN 2

#endif /* CONFIG_H_ */
