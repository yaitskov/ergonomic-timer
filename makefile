.PHONY: flash readfuse
mtimer.hex: mtimer.cpp
	avr-g++ -Wall -g -Os -mmcu=attiny85 -o mtimer.bin mtimer.cpp
	avr-objcopy -j .text -j .data -O ihex mtimer.bin mtimer.hex


flash: mtimer.hex
	avrdude -p attiny85 -B 128 -c usbasp -U flash:w:mtimer.hex:i -F -P usb 

readfuseii: 
	avrdude -v -p attiny85 -B 187 -c avrispmkII -U lfuse:r:-:h  -P usb

readfusetiny: 
	avrdude -v -p attiny85 -B 1000 -c usbtiny -U lfuse:r:-:h -P usb

readfuse: 
	avrdude -v -p attiny85 -B 256 -c usbasp -U lfuse:r:-:h  -U hfuse:r:-:h -U efuse:r:-:h -P usb

defaultlfuse: 
	avrdude -p attiny85 -c usbasp -U lfuse:w:0x62:i -P usb

# https://www.engbedded.com/fusecalc/
div1: 
	avrdude -p attiny85 -c usbasp -U lfuse:w:0xe2:i  -P usb

e32khz: 
	avrdude -p attiny85 -B 4 -c usbasp -U lfuse:w:0xe6:m  -P usb


e10mhz: 
	avrdude -p attiny85 -B 256 -c usbasp -U lfuse:w:0xef:m  -P usb
