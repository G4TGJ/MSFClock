/*
 * io.h
 *
 * Created: 28/03/2021 13:15:56
 *  Author: Richard Tomlinson G4TGJ
 */ 


#ifndef IO_H_
#define IO_H_

// Initialise all IO ports
void ioInit();

// Set the LED output high or low
void ioWriteLEDOutputHigh();
void ioWriteLEDOutputLow();

// Toggle the LED output
void ioToggleLEDOutput();

// Read the RX input signal
bool ioReadRXInput();

uint32_t getMagnitude(void);
bool getSignal(void);
uint32_t getAverage(void);

#endif /* IO_H_ */