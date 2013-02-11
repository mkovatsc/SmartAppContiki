ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

ifeq ($(ADDR_LAST_BYTE),)
	ADDR_LAST_BYTE = 0x87
endif

include Makefile.honeywell

size:
	avr-size -C --mcu=$(MCU) honeywell.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data honeywell.$(TARGET) honeywell.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom honeywell.$(TARGET) honeywell.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:honeywell.$(TARGET)_flash.ihex -U eeprom:w:honeywell.$(TARGET)_eeprom.ihex

