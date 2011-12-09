ifeq ($(TARGET), )
	TARGET = de-rfmega128
endif

all:
	${MAKE} NOAVRSIZE=1 -f Makefile.honeywell honeywell.elf
	avr-objcopy -O ihex -j .text -j .data honeywell.elf honeywell_flash.hex
	avr-objcopy -O ihex -j .eeprom honeywell.elf honeywell_eeprom.hex
	avr-size -C --mcu=$(MCU) honeywell.elf

program:
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:honeywell_flash.hex -U eeprom:w:honeywell_eeprom.hex

clean:
	${MAKE} -f Makefile.honeywell clean
	rm -rf obj_$(TARGET)
	rm symbols.c symbols.h honeywell.elf *.hex
