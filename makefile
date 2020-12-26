.PHONY: flash readfuse
mtimer.hex: mtimer.cpp
	avr-g++ -Wall -g -Os -mmcu=attiny85 -o mtimer.bin mtimer.cpp
	avr-objcopy -j .text -j .data -O ihex mtimer.bin mtimer.hex


flash: mtimer.hex
	avrdude -p attiny85 -c usbasp -U flash:w:mtimer.hex:i -F -P usb

readfuse: 
	avrdude -p attiny85 -c usbasp -U lfuse:r:-:h  -U hfuse:r:-:h -U efuse:r:-:h -P usb

defaultlfuse: 
	avrdude -p attiny85 -c usbasp -U lfuse:w:0x62:i -P usb

# https://www.engbedded.com/fusecalc/
div1: 
	avrdude -p attiny85 -c usbasp -U lfuse:w:0xe2:i  -P usb

ext10mhz: 
	avrdude -p attiny85 -c usbasp -U lfuse:w:0xd0:i  -P usb
