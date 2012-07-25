#include "lib/sensors.h"
#include "dev/reed-sensor.h"
#include <avr/interrupt.h>

const struct sensors_sensor reed_sensor;
static int status(int type);

static struct timer debouncetimer;

static int enabled = 0;
uint8_t counter = 0;

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
	char t;
	t = PINE & _BV(PE7);
	return t;
}

static int
configure(int type, int c)
{
	switch (type) {
	case SENSORS_ACTIVE:
		if (c) {
			if(!status(SENSORS_ACTIVE)) {
				
				DDRE &= ~_BV(PE7);
				PORTE |= _BV(PE7);
				EICRB &= ~_BV(ISC71);
				EICRB  |= _BV(ISC70);
				EIMSK |= _BV(INT7); 
				enabled = 1;
				timer_set(&debouncetimer, 0);

			}
		} else {
			EIMSK &= ~_BV(INT7); 
			enabled = 0;
		}
		return 1;
	}
	return 0;
}


ISR(INT7_vect)
{
	if(timer_expired(&debouncetimer)) {
		timer_set(&debouncetimer, CLOCK_SECOND / 4);
		sensors_changed(&reed_sensor);
	}
}


SENSORS_SENSOR(reed_sensor, REED_SENSOR,
	       value, configure, status);

