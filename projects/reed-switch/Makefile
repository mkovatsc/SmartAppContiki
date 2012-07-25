ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.reed-switch

size:
	avr-size -C --mcu=$(MCU) reed-switch.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data reed-switch.$(TARGET) reed-switch.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom reed-switch.$(TARGET) reed-switch.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:reed-switch.$(TARGET)_flash.ihex -U eeprom:w:reed-switch.$(TARGET)_eeprom.ihex

