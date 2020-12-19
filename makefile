.PHONY: flash
mtimer.hex: mtimer.cpp
	avr-g++ -Wall -g -Os -mmcu=attiny85 -o mtimer.bin mtimer.cpp
	avr-objcopy -j .text -j .data -O ihex mtimer.bin mtimer.hex


flash: mtimer.hex
	avrdude -p attiny85 -c usbasp -U flash:w:mtimer.hex:i -F -P usb
