#include "lib/sensors.h"
#include "radio-sensor.h"
#include <avr/io.h>

/*
*
* Values & Formula from the ATmega128rfa1 Datasheet
*
*/

#define RSSI_BASE_VALUE -90


const struct sensors_sensor radio_sensor;

static int enabled = 0;

static int
status(int type)
{
	switch (type) {
	case SENSORS_ACTIVE:
	case SENSORS_READY:
		return enabled;
	}
	return 0;
}





static int
value(int type)
{
	int read;
	switch (type){
		case RADIO_SENSOR_RSSI:
		default:
			read = PHY_RSSI & 0x1f;
			return RSSI_BASE_VALUE + 3 * (read - 1);
	}	
}

static int
configure(int type, int c)
{
  if(type == SENSORS_ACTIVE) {
    enabled = c;
    return 1;
  }
  return 0;
}

SENSORS_SENSOR(radio_sensor, RADIO_SENSOR,
	       value, configure, status);

