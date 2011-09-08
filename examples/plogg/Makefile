all:
	${MAKE} TARGET=avr-ravenmote NOAVRSIZE=1 -f Makefile.plogg plogg.elf
	avr-objcopy -O ihex -j .text -j .data plogg.elf plogg_flash.hex
	avr-objcopy -O ihex -j .eeprom plogg.elf plogg_eeprom.hex
	avr-size -C --mcu=at90usb1287 plogg.elf

program:
	avrdude -p usb1287 -c jtagmkII -P usb -U flash:w:plogg_flash.hex -U eeprom:w:plogg_eeprom.hex

clean:
	${MAKE} TARGET=avr-ravenmote -f Makefile.plogg clean
	rm -rf obj_avr-ravenmote
	rm symbols.c symbols.h plogg.elf *.hex
	rm /mnt/hgfs/vm-share/plogg.elf
