ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.honeywell

size:
	avr-size -C --mcu=$(MCU) honeywell.$(TARGET)

program:
	avr-objcopy -O ihex -j .text -j .data honeywell.$(TARGET) honeywell_flash.hex
	avr-objcopy -O ihex -j .eeprom honeywell.$(TARGET) honeywell_eeprom.hex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:honeywell_flash.hex -U eeprom:w:honeywell_eeprom.hex

