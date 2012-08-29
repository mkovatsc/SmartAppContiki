ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.barometer

size:
	avr-size -C --mcu=$(MCU) barometer.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data barometer.$(TARGET) barometer.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom barometer.$(TARGET) barometer.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:barometer.$(TARGET)_flash.ihex -U eeprom:w:barometer.$(TARGET)_eeprom.ihex

