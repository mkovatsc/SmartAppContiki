/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#ifndef __HONEYWELL_SMART_H__
#define __HONEYWELL_SMART_H__

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "dev/radio-sensor.h"

#include "rest-engine.h"
#include "er-coap-separate.h"
#include "er-coap-transactions.h"

//sets the size of the request queue
#define REQUEST_QUEUE_SIZE 3

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0

#define BATT_LOW 	(1<<7)
#define BATT_WARNING 	(1<<6)
#define ERROR_MOTOR	(1<<3)
#define ERROR_MONTAGE	(1<<2)

#define REMOTE_PORT UIP_HTONS(COAP_DEFAULT_PORT)

#define EPTYPE "Honeywell"
#define VERSION "0.18.0"

#define RES_SEPARATE_CLEAN_UP_TIMEOUT   5

enum mode {manual_target=0, manual_timer=1, radio_target=2, radio_valve=3};

typedef struct {
  coap_separate_t request_metadata;
  clock_time_t last_call;
  uint8_t action;
  uint8_t error;
} application_separate_store_t;

typedef struct {
	uint8_t mode;
	uint16_t time;
} hw_timer_slot_t;

//struct for the cache
typedef struct {
	uint8_t day;
	uint8_t month;
	uint8_t year;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;

	uint16_t is_temperature;
	uint16_t target_temperature;
	uint16_t battery;
	uint16_t last_battery;
	uint8_t valve_wanted;
	uint8_t valve_is;
	uint8_t mode;

	// values used in the auto mode
	uint16_t threshold_temperature;
	uint16_t threshold_battery;

	/* 0 : justOne
	 *  1 : weekdays */
	uint8_t automode;

	int32_t wheel_event_value;

	clock_time_t last_battery_reading;
	clock_time_t last_predefined_reading;

	clock_time_t last_target_reading;
	clock_time_t last_threshold_temp_reading;
	clock_time_t last_threshold_bat_reading;
	clock_time_t last_valve_wanted_reading;
	clock_time_t last_slots_reading[8];

} poll_data_t;

extern poll_data_t poll_data;

#endif /* __HONEYWELL_SMART_H__ */
