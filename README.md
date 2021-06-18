# MSFClock
Decode and display UTC from MSF at Anthorn, Cumbria.

## Building the sofware

To compile from source you will need this repo and [TARL](https://github.com/G4TGJ/TARL).

### Windows Build

You can download the source as zip files or clone the repo using git. To do this install [Git for Windows](https://git-scm.com/download/win) or 
[GitHub Desktop](https://desktop.github.com/).

To build with [Atmel Studio 7](https://www.microchip.com/mplab/avr-support/atmel-studio-7) open MSFClock/MSFClock.atsln.

### Linux Build

To build with Linux you will need to install git, the compiler and library. For Ubuntu:

    sudo apt install gcc-avr avr-libc git
    
Clone this repo plus TARL:

    git clone https://github.com/G4TGJ/MSFClock.git
    git clone https://github.com/G4TGJ/TARL.git
    
Build:

    cd MSFClock/MSFClock
    ./build.sh

This creates Release/MSFClock.hex.
