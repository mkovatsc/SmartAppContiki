all:
	${MAKE} TARGET=avr-ravenmote NOAVRSIZE=1 -f Makefile.plogg-raven plogg-raven.elf
	avr-objcopy -O ihex -R .signature -R .fuse -R .eeprom plogg-raven.elf plogg-raven.hex
	avr-size -C --mcu=at90usb1287 plogg-raven.elf
	cp plogg-raven.elf /mnt/hgfs/vm-share/plogg-raven.elf

clean:
	${MAKE} -f Makefile.plogg-raven clean
	rm -rf obj_avr-ravenmote
	rm symbols.c symbols.h plogg-raven.elf plogg-raven.hex
	rm /mnt/hgfs/vm-share/plogg-raven.elf
