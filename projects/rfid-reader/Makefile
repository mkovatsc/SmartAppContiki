ifeq ($(TARGET),)
	TARGET = de-rfmega128
endif

include Makefile.rfid-reader

size:
	avr-size -C --mcu=$(MCU) rfid-reader.$(TARGET)

program:
	avr-objcopy -Oihex -j.text -j.data rfid-reader.$(TARGET) rfid-reader.$(TARGET)_flash.ihex
	avr-objcopy -Oihex -j.eeprom rfid-reader.$(TARGET) rfid-reader.$(TARGET)_eeprom.ihex
	avrdude -p $(MCU) -c jtagmkII -P usb -U flash:w:rfid-reader.$(TARGET)_flash.ihex -U eeprom:w:rfid-reader.$(TARGET)_eeprom.ihex

