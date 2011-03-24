all:
	${MAKE} TARGET=avr-ravenmote NOAVRSIZE=1 -f Makefile.ravenusbserver ravenusbserver.elf
	avr-objcopy -O ihex -R .signature -R .fuse -R .eeprom ravenusbserver.elf ravenusbserver.hex
	avr-size -C --mcu=at90usb1287 ravenusbserver.elf

clean:
	${MAKE} -f Makefile.ravenusbserver clean
	rm -rf obj_avr-ravenmote
	rm symbols.c symbols.h ravenusbserver.elf ravenusbserver.hex
