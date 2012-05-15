ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.honeywell

size:
	avr-size -C --mcu=$(MCU) honeywell.$(TARGET)

program:
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:honeywell_flash.hex -U eeprom:w:honeywell_eeprom.hex

