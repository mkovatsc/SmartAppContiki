ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.temp-sensor

size:
	avr-size -C --mcu=$(MCU) temp-sensor.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data temp-sensor.$(TARGET) temp-sensor.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom temp-sensor.$(TARGET) temp-sensor.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:temp-sensor.$(TARGET)_flash.ihex -U eeprom:w:temp-sensor.$(TARGET)_eeprom.ihex

