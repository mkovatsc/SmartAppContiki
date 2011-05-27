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

/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "contiki-raven.h"
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"

#if WITH_COAP == 3
#include "coap-03.h"
#include "coap-03-transactions.h"
#elif WITH_COAP == 6
#include "coap-06.h"
#include "coap-06-transactions.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

#define MAX(a,b) ((a)<(b)?(b):(a))

#define DEBUG 0

/*#include "UsefulMicropendousDefines.h"
// set up external SRAM prior to anything else to make sure malloc() has access to it
void EnableExternalSRAM (void) __attribute__ ((naked)) __attribute__ ((section (".init3")));
void EnableExternalSRAM(void)
{
  PORTE_EXT_SRAM_SETUP;  // set up control port
  ENABLE_EXT_SRAM;       // enable the external SRAM
  XMCRA = ((1 << SRE));  // enable XMEM interface with 0 wait states
  XMCRB = 0;
  SELECT_EXT_SRAM_BANK0; // select Bank 0
}*/


#define MAX(a,b) ((a)<(b)?(b):(a))


/*---------------------------------------------------------------------------*/
PROCESS(honeywell_process, "Honeywell comm");
PROCESS(coap_process, "coap");

/*---------------------------------------------------------------------------*/
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};
#if DEBUG
static char debug_buffer[128];
#endif
static int poll_time = 10;

enum mode {manual=0, timers=1, valve=2};
enum request_type {debug, idle, poll, auto_temperatures, set_auto_temperatures, auto_mode, get_timer};

static enum request_type request_state = idle;


typedef struct {
	uint8_t mode;
	uint16_t time;
} hw_timer_slot_t;

static struct {
	uint8_t day;
	uint8_t month;
	uint8_t year;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;

	uint16_t is_temperature;
	uint16_t target_temperature;
	uint16_t battery;
	uint8_t valve;
	uint8_t mode;

	// values used in the auto mode
	uint16_t frost_temperature;
	uint16_t energy_temperature;
	uint16_t comfort_temperature;
	uint16_t supercomfort_temperature;

	hw_timer_slot_t timers[8][8];

	/* 0 : justOne
	*  1 : weekdays */
	bool automode;

	clock_time_t last_poll;
} poll_data;

/*---------------------------------------------------------------------------*/
static int uart_get_char(unsigned char c)
{
	ringbuf_put(&uart_buf, c);
	if (c=='\n' || ringbuf_size(&uart_buf)==127) {
		ringbuf_put(&uart_buf, '\0');
		process_post(&honeywell_process, PROCESS_EVENT_MSG, NULL);
	}
	return 1;
}

static void parseD(char * data){
	//D: d5 01.01.10 14:20:07 A V: 30 I: 2287 S: 1700 B: 2707 Is: 00000000 Ib: 00 Ic: 28 Ie: 17 X
	if(data[0]=='D'){
		poll_data.valve = atoi(&data[29]);
		uint16_t is_temperature = atoi(&data[35]);
		if(poll_data.is_temperature != is_temperature){
			process_post(&coap_process, PROCESS_EVENT_MSG, NULL);
		}
		poll_data.is_temperature = is_temperature;
		poll_data.target_temperature = atoi(&data[43]);
		poll_data.battery = atoi(&data[51]);

		switch(data[24]){
			case 'M':
				poll_data.mode=manual;
				break;
			case 'A':
				poll_data.mode=timers;
				break;
			case 'V':
				poll_data.mode=valve;
				break;
		}

		poll_data.day=atoi(&data[6]);
		poll_data.month=atoi(&data[9]);
		poll_data.year=atoi(&data[12]);

		poll_data.hour=atoi(&data[15]);
		poll_data.minute=atoi(&data[18]);
		poll_data.second=atoi(&data[21]);

		poll_data.last_poll = clock_time();

	}
}

PROCESS_THREAD(honeywell_process, ev, data)
{
	PROCESS_BEGIN();
	
	static struct etimer etimer;
	int rx;
	int buf_pos;
	char buf[128];


	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);
	Led1_on(); // red

	//etimer_set(&etimer, CLOCK_SECOND * poll_time);
	
	request_state = idle;

	while (1) {
		PROCESS_WAIT_EVENT();
		if(ev == PROCESS_EVENT_TIMER) {
			etimer_set(&etimer, CLOCK_SECOND * poll_time);
			request_state = poll;
			printf_P(PSTR("D\n"));
		} else if (ev == PROCESS_EVENT_MSG) {
			buf_pos = 0;
			while ((rx=ringbuf_get(&uart_buf))!=-1) {
				if (buf_pos<126 && (char)rx=='\n') {
					buf[buf_pos++] = '\n';
					buf[buf_pos] = '\0';
					//printf("%s\r\n", buf);
					switch(request_state){
						case idle:
							break;
						case poll:
							parseD(buf);
							request_state = idle;
							break;
#if DEBUG
						case debug:
							memcpy(debug_buffer, buf, strlen(buf));
							request_state = idle;
							break;
#endif
						case auto_temperatures:
							if(strncmp_P(buf, PSTR("G[0"), 3) == 0){
								int index = atoi(&buf[3]);
								int temp;
								sscanf_P(&buf[6], PSTR("%x"), &temp);
								temp *= 50;
								switch(index){
									case 1:
										poll_data.frost_temperature = temp;
										printf_P(PSTR("G02\n"));
										break;
									case 2:
										poll_data.energy_temperature = temp;
										printf_P(PSTR("G03\n"));
										break;
									case 3:
										poll_data.comfort_temperature = temp;
										printf_P(PSTR("G04\n"));
										break;
									case 4:
										poll_data.supercomfort_temperature = temp;
										request_state = idle;
										break;
								}
							}
							break;
						case auto_mode:
							if(strncmp_P(buf, PSTR("S[22]"), 5) == 0){
								/*Result is either
								*  G[22]=00 or
								*  G[22]=01 */
								poll_data.automode = atoi(&buf[7]);
							}
							request_state = idle;
						case set_auto_temperatures:
							printf_P(PSTR("G01\n"));
							request_state = auto_temperatures;
							break;
						case get_timer:
							//R[ds]=mttt
							//W[ds]=mttt
							if(buf[0]=='R' || buf[0]=='W'){
								uint8_t index = atoi(&buf[2]);
								uint8_t day = index / 10;
								uint8_t slot = index % 10;
								char temp=buf[6];
								poll_data.timers[day][slot].mode = atoi(&temp);
								
								sscanf_P(&buf[7], PSTR("%x"), &poll_data.timers[day][slot].time);
								if(slot<7){
									slot++;
									printf_P(PSTR("R%d%d\n"), day, slot);
								}
								else{
									request_state = idle;
								}
							}
							else{
								request_state = idle;
							}
							break;
						default:
							break;
					}
					buf_pos = 0;
					continue;
				} else {
					buf[buf_pos++] = (char)rx;
				}
				if (buf_pos==127) {
					buf[buf_pos] = 0;
					buf_pos = 0;
				}
			} // while
		} // events
	}
	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
EVENT_RESOURCE(temperature, METHOD_GET, "temperature", "title=\"Get current temperature\";rt=\"Text\"");
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), poll_data.is_temperature/100, poll_data.is_temperature%100);

	request_state = poll;
	printf_P(PSTR("D\n"));

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

int temperature_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[6];
	
	int size = snprintf_P(content, 6, PSTR("%d.%02d"), poll_data.is_temperature/100, poll_data.is_temperature%100);

	++event_i;
	
	REST.notify_subscribers(r->url, 0, event_i, (uint8_t*)content, size);
	return 1;
}

RESOURCE(battery, METHOD_GET, "battery", "battery");
void battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_data.battery);

	request_state = poll;
	printf_P(PSTR("D\n"));

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(mode, METHOD_GET | METHOD_POST, "mode", "mode");
void mode_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET){
		request_state = poll;
		printf_P(PSTR("D\n"));
		switch(poll_data.mode){
			case manual:
				strncpy_P((char*)buffer, PSTR("manual"), preferred_size);
				break;
			case timers:
				strncpy_P((char*)buffer, PSTR("auto"), preferred_size);
				break;
			case valve:
				strncpy_P((char*)buffer, PSTR("valve"), preferred_size);
				break;
			default:
				strncpy_P((char*)buffer, PSTR("undefined"), preferred_size);
		}
	}
	else{
		const char * string = NULL;
		bool success = true;

		int len = REST.get_post_variable(request, "mode", &string);
		if(len == 0){ 
			success = false;
		}
		else{
			if(strncmp_P(string,PSTR("manual"),len)==0){
				request_state = poll;
				printf_P(PSTR("M00\n"));
				strncpy_P((char*)buffer, PSTR("New mode is: manual"), preferred_size);
			}
			else if(strncmp_P(string,PSTR("auto"),len)==0){
				request_state = poll;
				printf_P(PSTR("M01\n"));
				strncpy_P((char*)buffer, PSTR("New mode is: auto"), preferred_size);
			}
			else if(strncmp_P(string,PSTR("valve"),len)==0){
				request_state = poll;
				printf_P(PSTR("M02\n"));
				strncpy_P((char*)buffer, PSTR("New mode is: valve"), preferred_size);
			}
			else{
				success = false;
			}
		}

		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strcpy_P((char*)buffer, PSTR("Payload format: mode={auto, manual, valve}"));
		}
	}
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(target, METHOD_GET | METHOD_POST, "target", "target");
void target_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
	if (REST.get_method_type(request)==METHOD_GET){
		snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), poll_data.target_temperature/100, poll_data.target_temperature%100);
		request_state = poll;
		printf_P(PSTR("D\n"));
	}
	else{
		const char * string = NULL;
		int success = 1;
		if(REST.get_post_variable(request, "value", &string) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success = 0;
			} 
			else{
				uint16_t value = atoi(string);
				request_state = poll;
				printf_P(PSTR("A%02x\n"),value/5);
				strncpy_P((char*)buffer, PSTR("Successfully set value"), preferred_size);
			}
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: value=ttt, eg: value=155 sets the temperature to 15.5 degrees"), preferred_size);
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(poll, METHOD_GET | METHOD_POST, "poll", "poll");
void poll_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
	if (REST.get_method_type(request)==METHOD_GET){
		snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_time);
	}
	else{
		const char * string = NULL;
		int success = 1;
		if(REST.get_post_variable(request, "value", &string) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success=0;
			} 
			else{
				// the poll intervall has to be bigger than 0
				int poll_intervall = atoi(string);
				if(poll_intervall > 0){
					poll_time = poll_intervall;
					strncpy_P((char*)buffer, PSTR("Successfully set poll intervall"), preferred_size);
				}
				else{
					success = 0;
				}
			}
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: value=aa, eg: value=15 sets the poll interval to 15 seconds"), preferred_size);
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(valve, METHOD_GET | METHOD_POST, "valve", "valve");
void valve_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
	if (REST.get_method_type(request)==METHOD_GET){
		snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_data.valve);
		request_state = poll;
		printf_P(PSTR("D\n"));
	}
	else{
		const char * string = NULL;
		int success = 1;
		if(REST.get_post_variable(request, "value", &string) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success=0;
			} 
			else{
				int new_valve=atoi(string);
				request_state = poll;
				printf_P(PSTR("E%02x\n"),new_valve);
				strncpy_P((char*)buffer, PSTR("Successfully set valve position"), preferred_size);
			}
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: value=aa, eg: value=47 sets the valve 47 percent"), preferred_size);
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(date, METHOD_GET | METHOD_POST, "date", "date");
void date_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
	if (REST.get_method_type(request)==METHOD_GET){
		snprintf_P((char*)buffer, preferred_size, PSTR("%02d.%02d.%02d"), poll_data.day, poll_data.month, poll_data.year);
		request_state = poll;
		printf_P(PSTR("D\n"));
	}
	else{
		const char * string = NULL;
		int success = 1;
		int length = REST.get_post_variable(request, "value", &string);
		if( length == 8 ){
			int day=atoi(&string[0]);
			int month=atoi(&string[3]);
			int year=atoi(&string[6]);

			if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]) && isdigit(string[6]) && isdigit(string[7]))){
				success=0;
			} 
			else if (!(0<=year && year <=99 && 1<=month && month<=12 && 1<=day )){
				success=0;
			}
			else if( (month==4 || month ==6 || month==9 || month==11) && day>30){
				success=0;
			}
			else if( month==2 && !((year%4)==0) && day > 28) {
				success=0;
			}
			else if( month==2 && day>29){
				success=0;
			}
			else if( day > 31){
				success=0;
			}

			if(success){
				request_state = poll;
				printf_P(PSTR("Y%02x%02x%02x\n"),year,month,day);
				strncpy_P((char*)buffer, PSTR("Successfully set date"), preferred_size);
			}
		}
		else{
			success = 0;
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: value=dd.mm.yy"), preferred_size);
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


RESOURCE(time, METHOD_GET | METHOD_POST, "time", "time");
void time_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
	if (REST.get_method_type(request)==METHOD_GET){
		clock_time_t now = clock_time();
		int second = poll_data.second + (now - poll_data.last_poll) / CLOCK_SECOND;
		int minute = poll_data.minute + (second / 60);
		int hour = poll_data.hour + (minute / 60);
		snprintf_P((char*)buffer, preferred_size, PSTR("%02d:%02d:%02d"), hour % 24, minute % 60, second % 60 );
		request_state = poll;
		printf_P(PSTR("D\n"));
	}
	else{
		const char * string = NULL;
		int success = 1;
		int length = REST.get_post_variable(request, "value", &string);
		if(length==8 || length==5){
			int hour=atoi(&string[0]);
			int minute=atoi(&string[3]);
			int second=(length==5)?0:atoi(&string[6]);

			if (length==8 && ! (isdigit(string[6]) && isdigit(string[7]))){
				success = 0;
			}
			else if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]))){
				success = 0;
			}
			else if (!( 0<=hour && hour<=23 && 0<=minute && minute<=59 && 0<=second && second<=59)){
				success = 0; 
			}

			if(success){
				request_state = poll;
				printf_P(PSTR("H%02x%02x%02x\n"),hour,minute,second);
				strncpy_P((char*)buffer, PSTR("Successfully set time"), preferred_size);
			}
		}
		else{
			success = 0;
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: value=hh:mm[:ss]"), preferred_size);
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

static void handle_temperature(int temperature, int index, void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	if (REST.get_method_type(request)==METHOD_GET){
		snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), temperature/100, temperature%100);
		request_state = auto_temperatures;
		printf_P(PSTR("G01\n"));
	}
	else{
		const char * string = NULL;
		int success = 1;
		if(REST.get_post_variable(request, "value", &string) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success = 0;
			} 
			else{
				uint16_t value = atoi(string);
				printf_P(PSTR("S0%d%02x\n"),index, value/5);
				request_state = set_auto_temperatures;
				strncpy_P((char*)buffer, PSTR("Successfully set value"), preferred_size);
			}
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: value=ttt, eg: value=155 sets the temperature to 15.5 degrees (steps of 0.5 between 5 and 30 possible)"), preferred_size);
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(frost, METHOD_GET | METHOD_POST, "auto/frost", "auto/frost");
RESOURCE(energy, METHOD_GET | METHOD_POST, "auto/energy", "auto/energy");
RESOURCE(comfort, METHOD_GET | METHOD_POST, "auto/comfort", "auto/comfort");
RESOURCE(supercomfort, METHOD_GET | METHOD_POST, "auto/supercomfort", "auto/supercomfort");

void frost_handler(void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handle_temperature(poll_data.frost_temperature, 1, request, response, buffer, preferred_size, offset);
}
void energy_handler(void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handle_temperature(poll_data.energy_temperature, 2, request, response, buffer, preferred_size, offset);
}
void comfort_handler(void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handle_temperature(poll_data.comfort_temperature, 3, request, response, buffer, preferred_size, offset);
}
void supercomfort_handler(void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handle_temperature(poll_data.supercomfort_temperature, 4, request, response, buffer, preferred_size, offset);
}

#if DEBUG
RESOURCE(debug, METHOD_GET | METHOD_POST, "debug", "debug");
void debug_handler(void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	if (REST.get_method_type(request)==METHOD_POST){
		const char * string = NULL;
		REST.get_post_variable(request, "value", &string);
		printf("%s\n",string);
		request_state = debug;
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t*)debug_buffer, strlen(debug_buffer));
}
#endif

RESOURCE(timermode, METHOD_GET | METHOD_POST, "auto/timermode", "auto/timermode");
void timermode_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	if (REST.get_method_type(request)==METHOD_POST){
		const char * string = NULL;
		int success = 1;
		int len = REST.get_post_variable(request, "value", &string);
		if( len == 0 ){
			success = 0;
		}
		else {
			if(strncmp_P(string, PSTR("weekdays"), MAX(len,8))==0){
				request_state=auto_mode;
				printf_P(PSTR("S2201\n"));
				strncpy_P((char*)buffer, PSTR("Timermode set to weekdays"), preferred_size);
			}
			else if(strncmp_P(string, PSTR("justOne"), MAX(len,7))==0){
				request_state=auto_mode;
				printf_P(PSTR("S2200\n"));
				strncpy_P((char*)buffer, PSTR("Timermode set to justOne"), preferred_size);
			}
			else{
				success = 0;
			}
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: value={justOne, weekdays}"), preferred_size);
		}
	}
	else{
		strncpy_P((char*)buffer, (poll_data.automode)?PSTR("weekdays"):PSTR("justOne"), preferred_size);
		request_state=auto_mode;
		printf_P(PSTR("S22\n"));
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

static char * getModeString(int mode){
	char * string;
	switch(mode){
		case 0: string = PSTR("frost"); break;
		case 1: string = PSTR("energy"); break;
		case 2: string = PSTR("comfort"); break;
		case 3: string = PSTR("supercomfort"); break;
		default: string = PSTR("undefined");
	}
	return string;
}

static void handleTimer(int day, void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	int strpos = 0;
	const char * query = NULL;
	int success = 0;
	int len;
	int slot;

	char timerString[10];
	snprintf_P(timerString, 10, (day == 0)?PSTR("weektimer"):PSTR("daytimer%d"), day);
	
	if ((len = REST.get_query(request, &query))){
		if(isdigit(query[0]) && len == 1){
			char c[2];
			c[0]=query[0];
			c[1]='\0';
			slot = atoi(c);
			if(slot <= 8 && slot >= 0){
				//slot is in interval [0;7] on honeywell but [1;8] on ravenmote
				slot--;
				success = 1;
			}
		}
	}
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Add a get parameter that specifies the slot in [0;8] eg.: /auto/%s?3 to interact with slot 3\nFor overview set 0"), timerString);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strpos);
		return;
	}
	if(slot==-1){
		if (REST.get_method_type(request)==METHOD_POST){
			REST.set_response_status(response, REST.status.METHOD_NOT_ALLOWED);
			strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("POST not allowed in overview"));
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strpos);
		}
		else{
			//request_state = get_timer;
			//printf_P(PSTR("R%d0\n"),day);

			size_t strpos = 0;
			size_t bufpos = 0;
			int i;
			for (i=0; i<8; i++){
				uint16_t time = poll_data.timers[day][i].time;
				if(time > 23*60 + 59){
					strpos += snprintf_P((char*)buffer+bufpos, REST_MAX_CHUNK_SIZE-bufpos+1, PSTR("Slot %d: disabled\n"), i+1);
				}
				else{
					strpos += snprintf_P((char*)buffer+bufpos, REST_MAX_CHUNK_SIZE-bufpos+1, PSTR("Slot %d: %S at %02d:%02d\n"), i+1, getModeString(poll_data.timers[day][i].mode), time/60, time%60 );
				}

				if (strpos <= *offset)
				{
					/* Discard output before current block */
					bufpos = 0;
				}
				else /* (strpos > *offset) */
				{
					/* output partly in block */
					size_t len = MIN(strpos - *offset, preferred_size);

					/* Block might start in the middle of the output; align with buffer start. */
					if (bufpos == 0)
					{
						memmove(buffer, buffer+strlen((char *)buffer)-strpos+*offset, len);
					}

					bufpos = len;

					if (bufpos >= preferred_size)
					{
						break;
					}
				}
			}

			if (bufpos>0) {
				REST.set_response_payload(response, buffer, bufpos);
			}
			else
			{
				REST.set_response_status(response, REST.status.BAD_OPTION);
				REST.set_response_payload(response, (uint8_t*)"Block out of scope", 18);
			}

			if (i>=7) {
				*offset = -1;
			}
			else
			{
				*offset += bufpos;
			}
		}
		return;
	}
	if (REST.get_method_type(request)==METHOD_POST){
		const char * disable = NULL;
		len = REST.get_post_variable(request, "disable", &disable);
		if(len == 7){
			if(strncmp_P(disable, PSTR("disable"), len)!=0){
				success = 0;
			}
			else{
				request_state = get_timer;
				printf_P(PSTR("W%d%d0fff\n"),day, slot);
				strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Disabled slot %d of %s"), slot + 1, timerString);
			}
		}
		else{
			const char * mode = NULL;
			len = REST.get_post_variable(request, "mode", &mode);
			if(len == 0){
				success = 0;
			}
			else {
				// frost -> 0
				// energy -> 1
				// comfort -> 2
				// supercomfort -> 3
				int level;
				if(strncmp_P(mode, PSTR("frost"), MAX(len,5))==0){
					level = 0;
				}
				else if(strncmp_P(mode, PSTR("energy"), MAX(len,6))==0){
					level = 1;
				}
				else if(strncmp_P(mode, PSTR("comfort"), MAX(len,7))==0){
					level = 2;
				}
				else if(strncmp_P(mode, PSTR("supercomfort"), MAX(len,12))==0){
					level = 3;
				}
				else{
					success = 0;
				}

				if(success){
					const char * time = NULL;
					if(REST.get_post_variable(request, "time", &time)!=5){
						success = 0;
					}
					else{
						if(isdigit(time[0]) && isdigit(time[1]) && isdigit(time[3]) && isdigit(time[4]) ){
							int hour = atoi(&time[0]);
							int minute = atoi(&time[3]);
							if (!( 0<=hour && hour<=23 && 0<=minute && minute<=59 )){
								success = 0; 
							}
							else{
								request_state = get_timer;
								printf_P(PSTR("W%d%d%d%03x\n"),day, slot, level, hour*60 + minute);
								strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Set slot %d of %s to time %02d:%02d and mode %S"), slot + 1, timerString, hour, minute, getModeString(level));
							}
						}
						else{
							success = 0;
						}
					}
				}
			}
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Payload format: [ time=hh:mm&mode={frost,energy,comfort,supercomfort} | disable=disable ]"));
		}
	}
	else{
		request_state = get_timer;
		printf_P(PSTR("R%d%d\n"),day,slot);

		uint16_t time = poll_data.timers[day][slot].time;
		if(time > 23*60 + 59){
			strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("disabled"));
		}
		else{
			strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("%S at %02d:%02d"), getModeString(poll_data.timers[day][slot].mode), time/60, time%60 );
		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strpos);
}

RESOURCE(weektimer, METHOD_GET | METHOD_POST, "auto/weektimer", "auto/weektimer");
RESOURCE(day1timer, METHOD_GET | METHOD_POST, "auto/day1timer", "auto/day1timer");
RESOURCE(day2timer, METHOD_GET | METHOD_POST, "auto/day2timer", "auto/day2timer");
RESOURCE(day3timer, METHOD_GET | METHOD_POST, "auto/day3timer", "auto/day3timer");
RESOURCE(day4timer, METHOD_GET | METHOD_POST, "auto/day4timer", "auto/day4timer");
RESOURCE(day5timer, METHOD_GET | METHOD_POST, "auto/day5timer", "auto/day5timer");
RESOURCE(day6timer, METHOD_GET | METHOD_POST, "auto/day6timer", "auto/day6timer");
RESOURCE(day7timer, METHOD_GET | METHOD_POST, "auto/day7timer", "auto/day7timer");

void weektimer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(0, request, response, buffer, preferred_size, offset);
}
void day1timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(1, request, response, buffer, preferred_size, offset);
}
void day2timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(2, request, response, buffer, preferred_size, offset);
}
void day3timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(3, request, response, buffer, preferred_size, offset);
}
void day4timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(4, request, response, buffer, preferred_size, offset);
}
void day5timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(5, request, response, buffer, preferred_size, offset);
}
void day6timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(6, request, response, buffer, preferred_size, offset);
}
void day7timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	handleTimer(7, request, response, buffer, preferred_size, offset);
}


PROCESS_THREAD(coap_process, ev, data)
{
	PROCESS_BEGIN();

	rest_init_framework();

#if DEBUG
	rest_activate_resource(&resource_debug);
#endif

	rest_activate_resource(&resource_date);
	rest_activate_resource(&resource_time);
	rest_activate_event_resource(&resource_temperature);
	rest_activate_resource(&resource_battery);
	rest_activate_resource(&resource_target);
	rest_activate_resource(&resource_mode);
	rest_activate_resource(&resource_poll);
	rest_activate_resource(&resource_valve);
	rest_activate_resource(&resource_frost);
	rest_activate_resource(&resource_energy);
	rest_activate_resource(&resource_comfort);
	rest_activate_resource(&resource_supercomfort);
	rest_activate_resource(&resource_timermode);
	
	rest_activate_resource(&resource_weektimer);
	rest_activate_resource(&resource_day1timer);
	rest_activate_resource(&resource_day2timer);
	rest_activate_resource(&resource_day3timer);
	rest_activate_resource(&resource_day4timer);
	rest_activate_resource(&resource_day5timer);
	rest_activate_resource(&resource_day6timer);
	rest_activate_resource(&resource_day7timer);

	while(1){
		PROCESS_WAIT_EVENT();
		if(ev == PROCESS_EVENT_MSG){
			temperature_event_handler(&resource_temperature);
		}
	}

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&honeywell_process, &coap_process);
/*---------------------------------------------------------------------------*/
