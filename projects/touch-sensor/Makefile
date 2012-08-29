ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.touch-button

size:
	avr-size -C --mcu=$(MCU) touch-button.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data touch-button.$(TARGET) touch-button.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom touch-button.$(TARGET) touch-button.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:touch-button.$(TARGET)_flash.ihex -U eeprom:w:touch-button.$(TARGET)_eeprom.ihex

