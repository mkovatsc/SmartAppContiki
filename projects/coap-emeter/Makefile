all:	
	${MAKE} TARGET=de-rfmega128 NOAVRSIZE=1 -f Makefile.coapemeter coapemeter.elf >/dev/null
	avr-objcopy -O ihex -j .text -j .data -j .bss coapemeter.elf coapemeter_flash.hex
	avr-objcopy -O ihex -j .eeprom coapemeter.elf coapemeter_eeprom.hex
	avr-size coapemeter.elf
	
program:
	avrdude -e -p m128rfa1 -c jtagmkII -P usb -U flash:w:coapemeter_flash.hex -U eeprom:w:coapemeter_eeprom.hex
 
clean:
	${MAKE} TARGET=de-rfmega128 -f Makefile.coapemeter clean
	rm -rf obj_de-rfmega128
	rm symbols.c symbols.h coapemeter.elf *.hex
	rm -rf doxygen
