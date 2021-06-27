# MSFClock
Decode and display UTC from MSF at Anthorn, Cumbria.

## Programming flash

The Arduino Nano is programmed via its mini USB port. The usual application for programming is [avrdude](https://www.nongnu.org/avrdude/). 
This has a GUI frontend available - [avrdudess](https://blog.zakkemble.net/avrdudess-a-gui-for-avrdude/). This is what I use and it is very good. I also have avrdude set up as
an external tool in Atmel Studio so I can program the flash directly from there.

#### Example avrdude commands

The exact command you need will depend on your computer and operating system. For example, on Linux you may need to prefix these commands with ``sudo``.

Linux:

    avrdude  -patmega328p -carduino -P/dev/ttyUSB0 -b57600 -D -U flash:w:MSFClock.hex:i
    
Windows:

    "C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avrdude.exe" -C"C:\Program Files (x86)\Arduino\hardware\tools\avr/etc/avrdude.conf" -v -patmega328p -carduino -PCOM9 -b57600 -D -U flash:w:MSFClock.hex:i

You will need to set the COM port to that used on your computer. You may also find that the baud rate needs to be changed to 115200 depending on the bootloader version of your 
Nano.
    

## Building the software

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
