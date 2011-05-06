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
#include "rest.h"
#include "sys/clock.h"

#include "UsefulMicropendousDefines.h"
// set up external SRAM prior to anything else to make sure malloc() has access to it
void EnableExternalSRAM (void) __attribute__ ((naked)) __attribute__ ((section (".init3")));
void EnableExternalSRAM(void)
{
  PORTE_EXT_SRAM_SETUP;  // set up control port
  ENABLE_EXT_SRAM;       // enable the external SRAM
  XMCRA = ((1 << SRE));  // enable XMEM interface with 0 wait states
  XMCRB = 0;
  SELECT_EXT_SRAM_BANK0; // select Bank 0
}



/*---------------------------------------------------------------------------*/
PROCESS(honeywell_process, "Honeywell comm");

/*---------------------------------------------------------------------------*/

static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};
static char debug_buffer[128];

enum mode {manual=0, timers=1, valve=2};

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

	
	hw_timer_slot_t timers[8][6];
	

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
		poll_data.is_temperature = atoi(&data[35]);
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

static int poll_time = 10;

enum request_type {debug, idle, poll, auto_temperatures, set_auto_temperatures, auto_mode, get_timer};

static enum request_type request_state = idle;


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(honeywell_process, ev, data)
{
	static struct etimer etimer;
	int rx;
	int buf_pos;
	char buf[128];

	PROCESS_BEGIN();

	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);
	Led1_on(); // red

	etimer_set(&etimer, CLOCK_SECOND * poll_time);
	
	printf_P(PSTR("G01\n"));
	request_state = auto_temperatures;

	while (1) {
		PROCESS_WAIT_EVENT();
		if(ev == PROCESS_EVENT_TIMER) {
			etimer_set(&etimer, CLOCK_SECOND * poll_time);
			printf_P(PSTR("D\n"));
			request_state = poll;
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
							request_state = idle;
							parseD(buf);
							break;
						case debug:
							memcpy(debug_buffer, buf, strlen(buf));
							request_state = idle;
							break;
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
							if(buf[0]=='R' || buf[0]=='W'){
								uint8_t index = atoi(&buf[2]);
								uint8_t day = index / 10;
								uint8_t slot = index % 10;
								char temp=buf[6];
								poll_data.timers[day][slot].mode = atoi(&temp);
								
								sscanf_P(&buf[7], PSTR("%x"), &poll_data.timers[day][slot].time);
							}
							request_state = idle;
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


/* For each resource defined, there corresponds an handler method which should be defined too.
 * Name of the handler method should be [resource name]_handler
 * */
RESOURCE(temperature, METHOD_GET, "temperature");
void temperature_handler(REQUEST* request, RESPONSE* response)
{
	char temp[128];
	sprintf_P(temp, PSTR("%d.%02d"), poll_data.is_temperature/100, poll_data.is_temperature%100);

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

RESOURCE(battery, METHOD_GET, "battery");
void battery_handler(REQUEST* request, RESPONSE* response)
{
	char temp[128];
	sprintf_P(temp, PSTR("%d"), poll_data.battery);

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}



RESOURCE(mode, METHOD_GET | METHOD_POST, "mode");
void mode_handler(REQUEST* request, RESPONSE* response)
{
	char temp[128];
	if (rest_get_method_type(request)==METHOD_GET){
		switch(poll_data.mode){
			case manual:
				sprintf_P(temp, PSTR("manual"));
				break;
			case timers:
				sprintf_P(temp, PSTR("auto"));
				break;
			case valve:
				sprintf_P(temp, PSTR("valve"));
				break;
			default:
				sprintf_P(temp, PSTR("undefined"));
		}
	}
	else{
		char string[8];
		if(rest_get_post_variable(request, "mode", string, 7) == 0){ 
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: mode={auto, manual, valve}"));
		}
		else{
			sprintf_P(temp, PSTR("New mode is: %s"), string);
			if(strcmp_P(string,PSTR("manual"))==0){
				printf_P(PSTR("M00\n"));
			}
			else if(strcmp_P(string,PSTR("auto"))==0){
				printf_P(PSTR("M01\n"));
			}
			else if(strcmp_P(string,PSTR("valve"))==0){
				printf_P(PSTR("M02\n"));
			}
			else{
				sprintf_P(temp, PSTR("Payload format: mode={auto, manual, valve}"));
			}
		}

	}
	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}


RESOURCE(target, METHOD_GET | METHOD_POST, "target");
void target_handler(REQUEST* request, RESPONSE* response)
{	
	char temp[128];
	if (rest_get_method_type(request)==METHOD_GET){
		sprintf_P(temp, PSTR("%d.%02d"), poll_data.target_temperature/100, poll_data.target_temperature%100);
	}
	else{
		char string[5];
		int success = 1;
		if(rest_get_post_variable(request, "value", string, 4) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success = 0;
			} 
			else{
				uint16_t value = atoi(string);
				printf("A%02x\n",value/5);
				sprintf_P(temp, PSTR("Successfully set value"));
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=ttt, eg: value=155 sets the temperature to 15.5 degrees"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

RESOURCE(poll, METHOD_GET | METHOD_POST, "poll");
void poll_handler(REQUEST* request, RESPONSE* response)
{	
	char temp[128];
	if (rest_get_method_type(request)==METHOD_GET){
		sprintf_P(temp, PSTR("%d"), poll_time);
	}
	else{
		char string[5];
		int success = 1;
		if(rest_get_post_variable(request, "value", string, 4) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success=0;
			} 
			else{
				// the poll intervall has to be bigger than 0
				int poll_intervall = atoi(string);
				if(temp > 0){
					poll_time = poll_intervall;
					sprintf_P(temp, PSTR("Successfully set poll intervall"));
				}
				else{
					success = 0;
				}
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=aa, eg: value=15 sets the poll interval to 15 seconds"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

RESOURCE(valve, METHOD_GET | METHOD_POST, "valve");
void valve_handler(REQUEST* request, RESPONSE* response)
{	
	char temp[128];
	if (rest_get_method_type(request)==METHOD_GET){
		sprintf_P(temp, PSTR("%d"), poll_data.valve);
	}
	else{
		char string[5];
		int success = 1;
		if(rest_get_post_variable(request, "value", string, 4) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success=0;
			} 
			else{
				int new_valve=atoi(string);
				printf_P(PSTR("E%02x\n"),new_valve);
				sprintf_P(temp, PSTR("Successfully set valve position"));
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=aa, eg: value=47 sets the valve 47 percent"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

RESOURCE(date, METHOD_GET | METHOD_POST, "date");
void date_handler(REQUEST* request, RESPONSE* response)
{	
	char temp[128];
	if (rest_get_method_type(request)==METHOD_GET){
		sprintf_P(temp, PSTR("%02d.%02d.%02d"), poll_data.day, poll_data.month, poll_data.year);
	}
	else{
		char string[16];
		int success = 1;
		if(rest_get_post_variable(request, "value", string, 10) == 0){
			success = 0;
		}
		else{
			if(strlen(string)==8){
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
					printf_P(PSTR("Y%02x%02x%02x\n"),year,month,day);
					sprintf_P(temp, PSTR("Successfully set date"));
				}
			}
			else{
				success = 0;
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=dd.mm.yy"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}


RESOURCE(time, METHOD_GET | METHOD_POST, "time");
void time_handler(REQUEST* request, RESPONSE* response)
{	
	char temp[128];
	if (rest_get_method_type(request)==METHOD_GET){
		clock_time_t now = clock_time();
		int second = poll_data.second + (now - poll_data.last_poll) / CLOCK_SECOND;
		int minute = poll_data.minute + (second / 60);
		int hour = poll_data.hour + (minute / 60);
		sprintf_P(temp, PSTR("%02d:%02d:%02d"), hour % 24, minute % 60, second % 60 );
	}
	else{
		char string[16];
		int success = 1;
		if(rest_get_post_variable(request, "value", string, 10) == 0){
			success = 0;
		}
		else{
			int length = strlen(string);
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
					printf_P(PSTR("H%02x%02x%02x\n"),hour,minute,second);
					sprintf_P(temp, PSTR("Successfully set time"));
				}
			}
			else{
				success = 0;
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=hh:mm[:ss]"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

static void handle_temperature(char * identifier, int temperature, int index, REQUEST * request, RESPONSE * response){
	char temp[128];
	if (rest_get_method_type(request)==METHOD_GET){
		sprintf_P(temp, PSTR("%d.%02d"), temperature/100, temperature%100);
	}
	else{
		char string[5];
		int success = 1;
		if(rest_get_post_variable(request, "value", string, 4) == 0){
			success = 0;
		}
		else{
			if (!isdigit(string[0])){
				success = 0;
			} 
			else{
				uint16_t value = atoi(string);
				printf_P("S0%d%02x\n",index, value/5);
				request_state = set_auto_temperatures;
				sprintf_P(temp, PSTR("Successfully set value"));
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=ttt, eg: value=155 sets the %S temperature to 15.5 degrees"), identifier);
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

RESOURCE(frost, METHOD_GET | METHOD_POST, "auto/frost");
void frost_handler(REQUEST * request, RESPONSE * response){
	handle_temperature(PSTR("frost"), poll_data.frost_temperature, 1, request, response);
}

RESOURCE(energy, METHOD_GET | METHOD_POST, "auto/energy");
void energy_handler(REQUEST * request, RESPONSE * response){
	handle_temperature(PSTR("energy"), poll_data.energy_temperature, 2, request, response);
}

RESOURCE(comfort, METHOD_GET | METHOD_POST, "auto/comfort");
void comfort_handler(REQUEST * request, RESPONSE * response){
	handle_temperature(PSTR("comfort"), poll_data.comfort_temperature, 3, request, response);
}

RESOURCE(supercomfort, METHOD_GET | METHOD_POST, "auto/supercomfort");
void supercomfort_handler(REQUEST * request, RESPONSE * response){
	handle_temperature(PSTR("supercomfort"), poll_data.supercomfort_temperature, 4, request, response);
}

RESOURCE(debug, METHOD_GET | METHOD_POST, "debug");
void debug_handler(REQUEST * request, RESPONSE * response){
	if (rest_get_method_type(request)==METHOD_POST){
		char string[10];
		rest_get_post_variable(request, "value", string, 9);
		printf("%s\n",string);
		request_state = debug;
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)debug_buffer, strlen(debug_buffer));
}


RESOURCE(auto, METHOD_GET, "auto");
void auto_handler(REQUEST* request, RESPONSE* response)
{	
	char temp[256];
	int index = 0;
	index += sprintf_P(temp + index, PSTR("</auto/frost>"));
	index += sprintf_P(temp + index, PSTR("</auto/energy>"));
	index += sprintf_P(temp + index, PSTR("</auto/comfort>"));
	index += sprintf_P(temp + index, PSTR("</auto/supercomfort>"));
	index += sprintf_P(temp + index, PSTR("</auto/timermode>"));
	index += sprintf_P(temp + index, PSTR("</auto/weektimer>"));
	index += sprintf_P(temp + index, PSTR("</auto/daytimer1>"));
	index += sprintf_P(temp + index, PSTR("</auto/daytimer2>"));
	index += sprintf_P(temp + index, PSTR("</auto/daytimer3>"));
	index += sprintf_P(temp + index, PSTR("</auto/daytimer4>"));
	index += sprintf_P(temp + index, PSTR("</auto/daytimer5>"));
	index += sprintf_P(temp + index, PSTR("</auto/daytimer6>"));
	index += sprintf_P(temp + index, PSTR("</auto/daytimer7>"));

	rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
	rest_set_response_payload(response, (uint8_t*)temp , strlen(temp));
}

RESOURCE(timermode, METHOD_GET | METHOD_POST, "auto/timermode");
void timermode_handler(REQUEST* request, RESPONSE* response){
	char temp[128];
	if (rest_get_method_type(request)==METHOD_POST){
		char string[16];
		int success = 1;
		if(rest_get_post_variable(request, "value", string, 10)==0){
			success = 0;
		}
		else {
			if(strncmp_P(string, PSTR("weekdays"), 9)==0){
				request_state=auto_mode;
				printf_P(PSTR("S2201\n"));
				sprintf_P(temp, PSTR("Timermode set to weekdays"));
			}
			else if(strncmp_P(string, PSTR("justOne"), 8)==0){
				request_state=auto_mode;
				printf_P(PSTR("S2200\n"));
				sprintf_P(temp, PSTR("Timermode set to justOne"));
			}
			else{
				success = 0;
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value={justOne, weekdays}"));
		}
	}
	else{
		sprintf_P(temp, (poll_data.automode)?PSTR("weekdays"):PSTR("justOne"));
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

static void handleTimer(int day, int slot, REQUEST * request, RESPONSE * response){
	char temp[128];
	if (rest_get_method_type(request)==METHOD_POST){
		char mode[16];
		char disable[10];
		int success = 1;
		if(rest_get_post_variable(request, "disable", disable, 9)!=0){
			if(strncmp_P(disable, PSTR("disable"), 10)!=0){
				success = 0;
			}
			else{
				request_state = get_timer;
				printf_P(PSTR("W%d%d0fff\n"),day, slot);
				sprintf_P(temp, PSTR("W%d%d0fff\n"),day, slot);
			}
		}
		else if(rest_get_post_variable(request, "mode", mode, 14)==0){
			success = 0;
		}
		else {
			/* frost -> 0
		     * energy -> 1
    		 * comfort -> 2
			 * supercomfort -> 3 */
			int level;
			if(strncmp_P(mode, PSTR("frost"), 14)==0){
				level = 0;
			}
			else if(strncmp_P(mode, PSTR("energy"), 14)==0){
				level = 1;
			}
			else if(strncmp_P(mode, PSTR("comfort"), 14)==0){
				level = 2;
			}
			else if(strncmp_P(mode, PSTR("supercomfort"), 14)==0){
				level = 3;
			}
			else{
				success = 0;
			}

			if(success){
				char time[8];
				if(rest_get_post_variable(request, "time", time, 6)==0){
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
							//sprintf_P(temp, PSTR("Set timer of day %d in slot %d to the mode %s at time %s"), day, slot, mode, time);
							request_state = get_timer;
							printf_P(PSTR("W%d%d%d%03x\n"),day, slot, level, hour*60 + minute);
							sprintf_P(temp, PSTR("W%d%d%d%03x\n"),day, slot, level, hour*60 + minute);
						}
					}
					else{
						success = 0;
					}
				}
			}
		}
		if(!success){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: [ time=hh:mm&mode={frost,energy,comfort,supercomfort} | disable=disable ]"));
		}
	}
	else{
		request_state = get_timer;
		printf_P(PSTR("R%d%d\n"),day,slot);

		uint16_t time = poll_data.timers[day][slot].time;
		if(time > 23*60 + 59){
			sprintf_P(temp, PSTR("disabled"));
		}
		else{
			char * mode;
			switch(poll_data.timers[day][slot].mode){
				case 0: mode = PSTR("frost"); break;
				case 1: mode = PSTR("energy"); break;
				case 2: mode = PSTR("comfort"); break;
				case 3: mode = PSTR("supercomfort"); break;
				default: mode = PSTR("undefined");
			}

			sprintf_P(temp, PSTR("%S at %02d:%02d"), mode, time/60, time%60 );
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
}

static void handleTimerDir(char * type, REQUEST* request, RESPONSE* response){
	char temp[196];
	int index = 0;
	index += sprintf_P(temp + index, PSTR("</auto/%S/slot1>"), type);
	index += sprintf_P(temp + index, PSTR("</auto/%S/slot2>"), type);
	index += sprintf_P(temp + index, PSTR("</auto/%S/slot3>"), type);
	index += sprintf_P(temp + index, PSTR("</auto/%S/slot4>"), type);
	index += sprintf_P(temp + index, PSTR("</auto/%S/slot5>"), type);
	index += sprintf_P(temp + index, PSTR("</auto/%S/slot6>"), type);

	rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
	rest_set_response_payload(response, (uint8_t*)temp , strlen(temp));
}

RESOURCE(weektimer, METHOD_GET, "auto/weektimer");
RESOURCE(daytimer1, METHOD_GET, "auto/daytimer1");
RESOURCE(daytimer2, METHOD_GET, "auto/daytimer2");
RESOURCE(daytimer3, METHOD_GET, "auto/daytimer3");
RESOURCE(daytimer4, METHOD_GET, "auto/daytimer4");
RESOURCE(daytimer5, METHOD_GET, "auto/daytimer5");
RESOURCE(daytimer6, METHOD_GET, "auto/daytimer6");
RESOURCE(daytimer7, METHOD_GET, "auto/daytimer7");
void weektimer_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("weektimer"), request, response);}
void daytimer1_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("daytimer1"), request, response);}
void daytimer2_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("daytimer2"), request, response);}
void daytimer3_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("daytimer3"), request, response);}
void daytimer4_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("daytimer4"), request, response);}
void daytimer5_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("daytimer5"), request, response);}
void daytimer6_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("daytimer6"), request, response);}
void daytimer7_handler(REQUEST* request, RESPONSE* response){handleTimerDir(PSTR("daytimer7"), request, response);}

RESOURCE(weektimer1, METHOD_GET | METHOD_POST, "auto/weektimer/slot1");
RESOURCE(weektimer2, METHOD_GET | METHOD_POST, "auto/weektimer/slot2");
RESOURCE(weektimer3, METHOD_GET | METHOD_POST, "auto/weektimer/slot3");
RESOURCE(weektimer4, METHOD_GET | METHOD_POST, "auto/weektimer/slot4");
RESOURCE(weektimer5, METHOD_GET | METHOD_POST, "auto/weektimer/slot5");
RESOURCE(weektimer6, METHOD_GET | METHOD_POST, "auto/weektimer/slot6");
void weektimer1_handler(REQUEST* request, RESPONSE* response){handleTimer(0,0,request,response);}
void weektimer2_handler(REQUEST* request, RESPONSE* response){handleTimer(0,1,request,response);}
void weektimer3_handler(REQUEST* request, RESPONSE* response){handleTimer(0,2,request,response);}
void weektimer4_handler(REQUEST* request, RESPONSE* response){handleTimer(0,3,request,response);}
void weektimer5_handler(REQUEST* request, RESPONSE* response){handleTimer(0,4,request,response);}
void weektimer6_handler(REQUEST* request, RESPONSE* response){handleTimer(0,5,request,response);}

RESOURCE(day1timer1, METHOD_GET | METHOD_POST, "auto/daytimer1/slot1");
RESOURCE(day1timer2, METHOD_GET | METHOD_POST, "auto/daytimer1/slot2");
RESOURCE(day1timer3, METHOD_GET | METHOD_POST, "auto/daytimer1/slot3");
RESOURCE(day1timer4, METHOD_GET | METHOD_POST, "auto/daytimer1/slot4");
RESOURCE(day1timer5, METHOD_GET | METHOD_POST, "auto/daytimer1/slot5");
RESOURCE(day1timer6, METHOD_GET | METHOD_POST, "auto/daytimer1/slot6");
void day1timer1_handler(REQUEST* request, RESPONSE* response){handleTimer(1,0,request,response);}
void day1timer2_handler(REQUEST* request, RESPONSE* response){handleTimer(1,1,request,response);}
void day1timer3_handler(REQUEST* request, RESPONSE* response){handleTimer(1,2,request,response);}
void day1timer4_handler(REQUEST* request, RESPONSE* response){handleTimer(1,3,request,response);}
void day1timer5_handler(REQUEST* request, RESPONSE* response){handleTimer(1,4,request,response);}
void day1timer6_handler(REQUEST* request, RESPONSE* response){handleTimer(1,5,request,response);}

RESOURCE(day2timer1, METHOD_GET | METHOD_POST, "auto/daytimer2/slot1");
RESOURCE(day2timer2, METHOD_GET | METHOD_POST, "auto/daytimer2/slot2");
RESOURCE(day2timer3, METHOD_GET | METHOD_POST, "auto/daytimer2/slot3");
RESOURCE(day2timer4, METHOD_GET | METHOD_POST, "auto/daytimer2/slot4");
RESOURCE(day2timer5, METHOD_GET | METHOD_POST, "auto/daytimer2/slot5");
RESOURCE(day2timer6, METHOD_GET | METHOD_POST, "auto/daytimer2/slot6");
void day2timer1_handler(REQUEST* request, RESPONSE* response){handleTimer(2,0,request,response);}
void day2timer2_handler(REQUEST* request, RESPONSE* response){handleTimer(2,1,request,response);}
void day2timer3_handler(REQUEST* request, RESPONSE* response){handleTimer(2,2,request,response);}
void day2timer4_handler(REQUEST* request, RESPONSE* response){handleTimer(2,3,request,response);}
void day2timer5_handler(REQUEST* request, RESPONSE* response){handleTimer(2,4,request,response);}
void day2timer6_handler(REQUEST* request, RESPONSE* response){handleTimer(2,5,request,response);}

RESOURCE(day3timer1, METHOD_GET | METHOD_POST, "auto/daytimer3/slot1");
RESOURCE(day3timer2, METHOD_GET | METHOD_POST, "auto/daytimer3/slot2");
RESOURCE(day3timer3, METHOD_GET | METHOD_POST, "auto/daytimer3/slot3");
RESOURCE(day3timer4, METHOD_GET | METHOD_POST, "auto/daytimer3/slot4");
RESOURCE(day3timer5, METHOD_GET | METHOD_POST, "auto/daytimer3/slot5");
RESOURCE(day3timer6, METHOD_GET | METHOD_POST, "auto/daytimer3/slot6");
void day3timer1_handler(REQUEST* request, RESPONSE* response){handleTimer(3,0,request,response);}
void day3timer2_handler(REQUEST* request, RESPONSE* response){handleTimer(3,1,request,response);}
void day3timer3_handler(REQUEST* request, RESPONSE* response){handleTimer(3,2,request,response);}
void day3timer4_handler(REQUEST* request, RESPONSE* response){handleTimer(3,3,request,response);}
void day3timer5_handler(REQUEST* request, RESPONSE* response){handleTimer(3,4,request,response);}
void day3timer6_handler(REQUEST* request, RESPONSE* response){handleTimer(3,5,request,response);}

RESOURCE(day4timer1, METHOD_GET | METHOD_POST, "auto/daytimer4/slot1");
RESOURCE(day4timer2, METHOD_GET | METHOD_POST, "auto/daytimer4/slot2");
RESOURCE(day4timer3, METHOD_GET | METHOD_POST, "auto/daytimer4/slot3");
RESOURCE(day4timer4, METHOD_GET | METHOD_POST, "auto/daytimer4/slot4");
RESOURCE(day4timer5, METHOD_GET | METHOD_POST, "auto/daytimer4/slot5");
RESOURCE(day4timer6, METHOD_GET | METHOD_POST, "auto/daytimer4/slot6");
void day4timer1_handler(REQUEST* request, RESPONSE* response){handleTimer(4,0,request,response);}
void day4timer2_handler(REQUEST* request, RESPONSE* response){handleTimer(4,1,request,response);}
void day4timer3_handler(REQUEST* request, RESPONSE* response){handleTimer(4,2,request,response);}
void day4timer4_handler(REQUEST* request, RESPONSE* response){handleTimer(4,3,request,response);}
void day4timer5_handler(REQUEST* request, RESPONSE* response){handleTimer(4,4,request,response);}
void day4timer6_handler(REQUEST* request, RESPONSE* response){handleTimer(4,5,request,response);}

RESOURCE(day5timer1, METHOD_GET | METHOD_POST, "auto/daytimer5/slot1");
RESOURCE(day5timer2, METHOD_GET | METHOD_POST, "auto/daytimer5/slot2");
RESOURCE(day5timer3, METHOD_GET | METHOD_POST, "auto/daytimer5/slot3");
RESOURCE(day5timer4, METHOD_GET | METHOD_POST, "auto/daytimer5/slot4");
RESOURCE(day5timer5, METHOD_GET | METHOD_POST, "auto/daytimer5/slot5");
RESOURCE(day5timer6, METHOD_GET | METHOD_POST, "auto/daytimer5/slot6");
void day5timer1_handler(REQUEST* request, RESPONSE* response){handleTimer(5,0,request,response);}
void day5timer2_handler(REQUEST* request, RESPONSE* response){handleTimer(5,1,request,response);}
void day5timer3_handler(REQUEST* request, RESPONSE* response){handleTimer(5,2,request,response);}
void day5timer4_handler(REQUEST* request, RESPONSE* response){handleTimer(5,3,request,response);}
void day5timer5_handler(REQUEST* request, RESPONSE* response){handleTimer(5,4,request,response);}
void day5timer6_handler(REQUEST* request, RESPONSE* response){handleTimer(5,5,request,response);}

RESOURCE(day6timer1, METHOD_GET | METHOD_POST, "auto/daytimer6/slot1");
RESOURCE(day6timer2, METHOD_GET | METHOD_POST, "auto/daytimer6/slot2");
RESOURCE(day6timer3, METHOD_GET | METHOD_POST, "auto/daytimer6/slot3");
RESOURCE(day6timer4, METHOD_GET | METHOD_POST, "auto/daytimer6/slot4");
RESOURCE(day6timer5, METHOD_GET | METHOD_POST, "auto/daytimer6/slot5");
RESOURCE(day6timer6, METHOD_GET | METHOD_POST, "auto/daytimer6/slot6");
void day6timer1_handler(REQUEST* request, RESPONSE* response){handleTimer(6,0,request,response);}
void day6timer2_handler(REQUEST* request, RESPONSE* response){handleTimer(6,1,request,response);}
void day6timer3_handler(REQUEST* request, RESPONSE* response){handleTimer(6,2,request,response);}
void day6timer4_handler(REQUEST* request, RESPONSE* response){handleTimer(6,3,request,response);}
void day6timer5_handler(REQUEST* request, RESPONSE* response){handleTimer(6,4,request,response);}
void day6timer6_handler(REQUEST* request, RESPONSE* response){handleTimer(6,5,request,response);}

RESOURCE(day7timer1, METHOD_GET | METHOD_POST, "auto/daytimer7/slot1");
RESOURCE(day7timer2, METHOD_GET | METHOD_POST, "auto/daytimer7/slot2");
RESOURCE(day7timer3, METHOD_GET | METHOD_POST, "auto/daytimer7/slot3");
RESOURCE(day7timer4, METHOD_GET | METHOD_POST, "auto/daytimer7/slot4");
RESOURCE(day7timer5, METHOD_GET | METHOD_POST, "auto/daytimer7/slot5");
RESOURCE(day7timer6, METHOD_GET | METHOD_POST, "auto/daytimer7/slot6");
void day7timer1_handler(REQUEST* request, RESPONSE* response){handleTimer(7,0,request,response);}
void day7timer2_handler(REQUEST* request, RESPONSE* response){handleTimer(7,1,request,response);}
void day7timer3_handler(REQUEST* request, RESPONSE* response){handleTimer(7,2,request,response);}
void day7timer4_handler(REQUEST* request, RESPONSE* response){handleTimer(7,3,request,response);}
void day7timer5_handler(REQUEST* request, RESPONSE* response){handleTimer(7,4,request,response);}
void day7timer6_handler(REQUEST* request, RESPONSE* response){handleTimer(7,5,request,response);}



RESOURCE(discover, METHOD_GET, ".well-known/core");
void discover_handler(REQUEST* request, RESPONSE* response)
{
	char temp[128];
	int index = 0;
	index += sprintf_P(temp + index, PSTR("</temperature>;rt=\"CurrentTemperature\""));
	index += sprintf_P(temp + index, PSTR("</target>"));
	index += sprintf_P(temp + index, PSTR("</mode>"));
	index += sprintf_P(temp + index, PSTR("</poll>"));
	index += sprintf_P(temp + index, PSTR("</valve>"));
	index += sprintf_P(temp + index, PSTR("</battery>"));
	index += sprintf_P(temp + index, PSTR("</date>"));
	index += sprintf_P(temp + index, PSTR("</time>"));
	index += sprintf_P(temp + index, PSTR("</auto>"));
	index += sprintf_P(temp + index, PSTR("</debug>"));


	rest_set_response_payload(response, (uint8_t*)temp, strlen(temp));
	rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
}


PROCESS(coap_process, "coap");
PROCESS_THREAD(coap_process, ev, data)
{
	PROCESS_BEGIN();

	rest_init();

	
	rest_activate_resource(&resource_debug);

	rest_activate_resource(&resource_date);
	rest_activate_resource(&resource_time);
	rest_activate_resource(&resource_temperature);
	rest_activate_resource(&resource_battery);
	rest_activate_resource(&resource_target);
	rest_activate_resource(&resource_mode);
	rest_activate_resource(&resource_poll);
	rest_activate_resource(&resource_valve);
	rest_activate_resource(&resource_auto);
	rest_activate_resource(&resource_frost);
	rest_activate_resource(&resource_energy);
	rest_activate_resource(&resource_comfort);
	rest_activate_resource(&resource_supercomfort);
	rest_activate_resource(&resource_timermode);
	
	rest_activate_resource(&resource_weektimer);
	rest_activate_resource(&resource_daytimer1);
	rest_activate_resource(&resource_daytimer2);
	rest_activate_resource(&resource_daytimer3);
	rest_activate_resource(&resource_daytimer4);
	rest_activate_resource(&resource_daytimer5);
	rest_activate_resource(&resource_daytimer6);
	rest_activate_resource(&resource_daytimer7);



	rest_activate_resource(&resource_weektimer1);
	rest_activate_resource(&resource_weektimer2);
	rest_activate_resource(&resource_weektimer3);
	rest_activate_resource(&resource_weektimer4);
	rest_activate_resource(&resource_weektimer5);
	rest_activate_resource(&resource_weektimer6);

	rest_activate_resource(&resource_day1timer1);
	rest_activate_resource(&resource_day1timer2);
	rest_activate_resource(&resource_day1timer3);
	rest_activate_resource(&resource_day1timer4);
	rest_activate_resource(&resource_day1timer5);
	rest_activate_resource(&resource_day1timer6);
	
	rest_activate_resource(&resource_day2timer1);
	rest_activate_resource(&resource_day2timer2);
	rest_activate_resource(&resource_day2timer3);
	rest_activate_resource(&resource_day2timer4);
	rest_activate_resource(&resource_day2timer5);
	rest_activate_resource(&resource_day2timer6);

	rest_activate_resource(&resource_day3timer1);
	rest_activate_resource(&resource_day3timer2);
	rest_activate_resource(&resource_day3timer3);
	rest_activate_resource(&resource_day3timer4);
	rest_activate_resource(&resource_day3timer5);
	rest_activate_resource(&resource_day3timer6);

	rest_activate_resource(&resource_day4timer1);
	rest_activate_resource(&resource_day4timer2);
	rest_activate_resource(&resource_day4timer3);
	rest_activate_resource(&resource_day4timer4);
	rest_activate_resource(&resource_day4timer5);
	rest_activate_resource(&resource_day4timer6);

	rest_activate_resource(&resource_day5timer1);
	rest_activate_resource(&resource_day5timer2);
	rest_activate_resource(&resource_day5timer3);
	rest_activate_resource(&resource_day5timer4);
	rest_activate_resource(&resource_day5timer5);
	rest_activate_resource(&resource_day5timer6);

	rest_activate_resource(&resource_day6timer1);
	rest_activate_resource(&resource_day6timer2);
	rest_activate_resource(&resource_day6timer3);
	rest_activate_resource(&resource_day6timer4);
	rest_activate_resource(&resource_day6timer5);
	rest_activate_resource(&resource_day6timer6);

	rest_activate_resource(&resource_day7timer1);
	rest_activate_resource(&resource_day7timer2);
	rest_activate_resource(&resource_day7timer3);
	rest_activate_resource(&resource_day7timer4);
	rest_activate_resource(&resource_day7timer5);
	rest_activate_resource(&resource_day7timer6);

	rest_activate_resource(&resource_discover);


	PROCESS_END();
}




/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&honeywell_process, &coap_process);
/*---------------------------------------------------------------------------*/

