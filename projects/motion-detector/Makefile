ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.motion-detector

size:
	avr-size -C --mcu=$(MCU) motion-detector.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data motion-detector.$(TARGET) motion-detector.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom motion-detector.$(TARGET) motion-detector.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:motion-detector.$(TARGET)_flash.ihex -U eeprom:w:motion-detector.$(TARGET)_eeprom.ihex

