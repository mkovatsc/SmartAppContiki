ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.light-sensor

size:
	avr-size -C --mcu=$(MCU) light-sensor.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data light-sensor.$(TARGET) light-sensor.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom light-sensor.$(TARGET) light-sensor.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:light-sensor.$(TARGET)_flash.ihex -U eeprom:w:light-sensor.$(TARGET)_eeprom.ihex

