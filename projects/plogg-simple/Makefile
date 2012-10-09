

all:
	${MAKE} TARGET=de-rfmega128 NOAVRSIZE=1 -f Makefile.plogg plogg.elf
	avr-objcopy -O ihex -j .text -j .data plogg.elf plogg_flash.hex
	avr-objcopy -O ihex -j .eeprom plogg.elf plogg_eeprom.hex
	avr-size -C --mcu=atmega128rfa1 plogg.elf

program:
	avrdude -p atmega128rfa1 -c jtagmkII -P usb -U flash:w:plogg_flash.hex -U eeprom:w:plogg_eeprom.hex

clean:
	${MAKE} TARGET=de-rfmega128 -f Makefile.plogg clean
	rm -rf obj_de-rfmega128
	rm symbols.c symbols.h plogg.elf *.hex
	rm /mnt/hgfs/vm-share/plogg.elf
