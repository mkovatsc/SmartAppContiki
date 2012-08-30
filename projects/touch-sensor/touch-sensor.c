#include "lib/sensors.h"
#include "dev/touch-sensor.h"
#include <avr/interrupt.h>
#include <avr/io.h>

const struct sensors_sensor touch_sensor;
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
				PORTE &= ~_BV(PE7);
				EICRB  |= _BV(ISC70);
				EICRB &= ~_BV(ISC71);
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
	if(timer_expired(&debouncetimer) && (PINE & _BV(PE7))) {
		timer_set(&debouncetimer, CLOCK_SECOND / 2);
		sensors_changed(&touch_sensor);
	}
}


SENSORS_SENSOR(touch_sensor, TOUCH_SENSOR,
	       value, configure, status);

