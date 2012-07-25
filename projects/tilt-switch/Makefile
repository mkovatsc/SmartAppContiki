ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.tilt-switch

size:
	avr-size -C --mcu=$(MCU) tilt-switch.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data tilt-switch.$(TARGET) tilt-switch.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom tilt-switch.$(TARGET) tilt-switch.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:tilt-switch.$(TARGET)_flash.ihex -U eeprom:w:tilt-switch.$(TARGET)_eeprom.ihex

