all:
	${MAKE} TARGET=avr-ravenmote NOAVRSIZE=1 -f Makefile.honeywell honeywell.elf
	avr-objcopy -O ihex -j .text -j .data honeywell.elf honeywell_flash.hex
	avr-objcopy -O ihex -j .eeprom honeywell.elf honeywell_eeprom.hex
	avr-size -C --mcu=at90usb1287 honeywell.elf

program:
	avrdude -p usb1287 -c jtagmkII -P usb -U flash:w:honeywell_flash.hex -U eeprom:w:honeywell_eeprom.hex

clean:
	${MAKE} TARGET=avr-ravenmote -f Makefile.honeywell clean
	rm -rf obj_avr-ravenmote
	rm symbols.c symbols.h honeywell.elf *.hex
