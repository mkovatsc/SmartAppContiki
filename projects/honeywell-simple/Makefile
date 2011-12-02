all:
	${MAKE} TARGET=de-rfmega128 NOAVRSIZE=1 -f Makefile.honeywell honeywell.elf
	avr-objcopy -O ihex -j .text -j .data honeywell.elf honeywell_flash.hex
	avr-objcopy -O ihex -j .eeprom honeywell.elf honeywell_eeprom.hex
	avr-size -C --mcu=atmega128rfa1 honeywell.elf

program:
	avrdude -p atmega128rfa1 -c jtagmkII -P usb -U flash:w:honeywell_flash.hex -U eeprom:w:honeywell_eeprom.hex

clean:
	${MAKE} TARGET=de-rfmega128 -f Makefile.honeywell clean
	rm -rf obj_de-rfmega128
	rm symbols.c symbols.h honeywell.elf *.hex
