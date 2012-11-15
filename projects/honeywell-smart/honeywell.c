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

#include "contiki.h"
#include "contiki-net.h"
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"

#include "net/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "er-coap-07-engine.h"
#include "er-coap-07.h"
#include "er-coap-07-transactions.h"
#include "er-coap-07-separate.h"

#include "dev/radio-sensor.h"

//#include "erbium.h"

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
#define VERSION "0.11.10"


extern uip_ds6_nbr_t uip_ds6_nbr_cache[];
extern uip_ds6_route_t uip_ds6_routing_table[];


/*------ PROCESSES ------------------------------------------------------------*/
PROCESS(honeywell_process, "Honeywell comm");
PROCESS(coap_process, "coap");
SENSORS(&radio_sensor);

/*---------------------------------------------------------------------------*/
enum mode {manual_target=0, manual_timers=1, auto_target=2, auto_valve=3, auto_timers=4};
typedef struct {
	uint8_t mode;
	uint16_t time;
} hw_timer_slot_t;

static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};

static uip_ip6addr_t rd_ipaddr;

static struct etimer event_gen;
static struct	stimer rdpost;
static struct stimer rdput;
static char * location;
static char loc[40];
static uint8_t registred = 0;

static int16_t rssi_value[5];
static int16_t rssi_count=0;
static int16_t rssi_position=0;
static int16_t rssi_avg;

/* EEPROM variables */
//uint16_t ee_error_ip[8] EEMEM;
//uint16_t ee_error_port EEMEM;
//char ee_error_uri[50] EEMEM;
char ee_identifier[50] EEMEM;

static char last_setting[20];

/* events for observing/separate responses */
static process_event_t changed_valve_event;
static process_event_t changed_temp_event;
static process_event_t changed_battery_event;
static process_event_t changed_wheel_event;
static process_event_t changed_mode_event;

static process_event_t get_date_response_event;
static process_event_t get_time_response_event;
static process_event_t get_target_response_event;
static process_event_t get_threshold_temp_response_event;
static process_event_t get_threshold_bat_response_event;
static process_event_t get_valve_wanted_response_event;
static process_event_t get_predefined_response_event;
static process_event_t get_slots_response_event;

static process_event_t set_response_event;

static process_event_t error_event;

static uint8_t error_active = 0;

//uint16_t error_ip[8];
//uint16_t error_port;
//char error_uri[50];
char identifier[50];


//struct for the cache
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
	uint16_t last_battery;
	uint8_t valve_wanted;
	uint8_t valve_is;
	uint8_t mode;

/*	// values used in the auto mode
	uint16_t frost_temperature;
	uint16_t energy_temperature;
	uint16_t comfort_temperature;
	uint16_t supercomfort_temperature;
*/
	uint16_t threshold_temperature;
	uint16_t threshold_battery;

//	hw_timer_slot_t timers[8][8];

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

} poll_data;


/*------- Separate Responses Store ------------------------------------*/
typedef struct application_separate_set_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_set_store_t;

static uint8_t separate_set_active = 0;
static application_separate_set_store_t separate_set_store[1];


typedef struct application_separate_get_date_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_get_date_store_t;

static uint8_t separate_get_date_active = 0;
static application_separate_get_date_store_t separate_get_date_store[1];


typedef struct application_separate_get_time_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_get_time_store_t;

static uint8_t separate_get_time_active = 0;
static application_separate_get_time_store_t separate_get_time_store[1];

typedef struct application_separate_get_target_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_get_target_store_t;

static uint8_t separate_get_target_active = 0;
static application_separate_get_target_store_t separate_get_target_store[1];


typedef struct application_separate_get_valve_wanted_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_get_valve_wanted_store_t;

static uint8_t separate_get_valve_wanted_active = 0;
static application_separate_get_valve_wanted_store_t separate_get_valve_wanted_store[1];
/*
typedef struct application_separate_get_predefined_store {
	coap_separate_t request_metadata;
	uint8_t index;
	uint8_t error;
} application_separate_get_predefined_store_t;

static uint8_t separate_get_predefined_active = 0;
static application_separate_get_predefined_store_t separate_get_predefined_store[1];


typedef struct application_separate_get_slots_store {
	coap_separate_t request_metadata;
	uint8_t day;
	uint8_t slot;
	uint8_t error;
} application_separate_get_slots_store_t;

static uint8_t separate_get_slots_active = 0;
static application_separate_get_slots_store_t separate_get_slots_store[1];
*/

typedef struct application_separate_get_threshold_temp_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_get_threshold_temp_store_t;

static uint8_t separate_get_threshold_temp_active = 0;
static application_separate_get_threshold_temp_store_t separate_get_threshold_temp_store[1];

typedef struct application_separate_get_threshold_bat_store {
	coap_separate_t request_metadata;
	uint8_t error;
} application_separate_get_threshold_bat_store_t;

static uint8_t separate_get_threshold_bat_active = 0;
static application_separate_get_threshold_bat_store_t separate_get_threshold_bat_store[1];


/*--HONEYWELL-PROCESS-IMPLEMENTATION-----------------------------------------*/
static int uart_get_char(unsigned char c)
{
	ringbuf_put(&uart_buf, c);
	if (c=='\n' || ringbuf_size(&uart_buf)==127) {
		ringbuf_put(&uart_buf, '\0');
		process_post(&honeywell_process, PROCESS_EVENT_MSG, NULL);
	}
	return 1;
}
/*
//fill in the cache
static void parseD(char * data) {
	//D: d5 01.01.10 14:20:07 AV V: 30 I: 2287 S: 1700 B: 2707 Is: 00000000 Ib: 00 Ic: 28 Ie: 17 X
	if(data[0]=='D'){
		uint8_t valve_state = atoi(&data[30]);
		if(poll_data.valve_wanted != valve_state){
			//send event to the coap process that the valve changed to notify all the subscribers
		}
		poll_data.valve_wanted = valve_state;
		poll_data.last_valve_wanted_reading = clock_time();

		poll_data.target_temperature = atoi(&data[44]);
		poll_data.last_target_reading = clock_time();

		poll_data.battery = atoi(&data[52]);
		poll_data.last_battery_reading = clock_time();

		switch(data[24]){
			case 'M':
				switch(data[25]){
					case 'T':
						poll_data.mode=manual_target;
						break;
					case 'P':
						poll_data.mode=manual_timers;
						break;
				}
				break;
			case 'A':
				switch(data[25]){
					case 'T':
						poll_data.mode=auto_target;
						break;
					case 'V':
						poll_data.mode=auto_valve;
						break;
					case 'P':
						poll_data.mode=auto_timers;
						break;
				}
				break;
		}

		poll_data.day=atoi(&data[6]);
		poll_data.month=atoi(&data[9]);
		poll_data.year=atoi(&data[12]);

		poll_data.hour=atoi(&data[15]);
		poll_data.minute=atoi(&data[18]);
		poll_data.second=atoi(&data[21]);

	}
}
*/

PROCESS_THREAD(honeywell_process, ev, data)
{
	PROCESS_BEGIN();

	int rx;
	int buf_pos;
	char buf[128];

	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);

	changed_valve_event = process_alloc_event();
	changed_temp_event = process_alloc_event();
	changed_battery_event = process_alloc_event();

	changed_wheel_event = process_alloc_event();
	changed_mode_event = process_alloc_event();
	set_response_event = process_alloc_event();
	get_date_response_event = process_alloc_event();
	get_time_response_event = process_alloc_event();
	get_target_response_event = process_alloc_event();
	get_threshold_temp_response_event = process_alloc_event();
	get_threshold_bat_response_event = process_alloc_event();
	get_valve_wanted_response_event = process_alloc_event();
	//	get_temperature_response_event = process_alloc_event();
	get_predefined_response_event = process_alloc_event();
	get_slots_response_event = process_alloc_event();

	error_event = process_alloc_event();	

	poll_data.mode=3;
	poll_data.threshold_battery = 100;
/*
	eeprom_read_block(&error_uri, ee_error_uri, 50);

	error_port = eeprom_read_word(&ee_error_port);

	int i;
	for(i=0;i<8;i++){
		error_ip[i] = eeprom_read_word(&ee_error_ip[i]);
	}
*/
	eeprom_read_block(&identifier, ee_identifier, 50);
	// finish booting first
	PROCESS_PAUSE();

	printf_P(PSTR("GM\n"));
	printf_P(PSTR("GV\n"));
	printf_P(PSTR("GT\n"));

	while (1) {
		PROCESS_WAIT_EVENT();
		if (ev == PROCESS_EVENT_MSG) {
			buf_pos = 0;
			while ((rx=ringbuf_get(&uart_buf))!=-1) {
				if (buf_pos<126 && (char)rx=='\n') {
					buf[buf_pos++] = '\n';
					buf[buf_pos] = '\0';

					switch (buf[0]){
						case 'D':
							//parseD(buf);
							break;

						case 'S':
							if (buf[2]=='1'){
								//successful
								separate_set_store->error=FALSE;
								switch(buf[1]){
									case 'T':
										poll_data.mode=auto_target;
										printf_P(PSTR("GT\n"));
										break;
									case 'V':
										poll_data.mode=auto_valve;
										printf_P(PSTR("GV\n"));
										break;
									case 'M':
										printf_P(PSTR("GM\n"));
										break;
									case 'P':
										printf_P(PSTR("GP\n"));
										break;
									case 'S':
										{
											int day;
											sscanf_P(&buf[4],PSTR("%d"), &day);
											printf_P(PSTR("GS%d\n"),day);
											break;
										}
									case 'D':
										printf_P(PSTR("GD\n"));
										break;
									case 'A':
										printf_P(PSTR("GA\n"));
										break;
									default:
										break;
								}
							}	
							else{
								//failure
								separate_set_store->error=TRUE;
							}
							process_post(&coap_process, set_response_event, NULL);
							break;

						case 'G':
							//Response for get 
							switch (buf[1]){
								case 'M':
									//Mode
									if (buf[2]=='1'){
										poll_data.mode = atoi(&buf[4]);	
									}
									break;
								case 'Y':
									//Date
									if (buf[2]=='1'){
										poll_data.day=atoi(&buf[4]);
										poll_data.month=atoi(&buf[7]);
										poll_data.year=atoi(&buf[10]);
										separate_get_date_store->error=FALSE;
									}
									else{
										separate_get_date_store->error=TRUE;
									}
									process_post(&coap_process, get_date_response_event, NULL);

									break;
								case 'H':
									//Time
									if (buf[2]=='1'){
										poll_data.hour=atoi(&buf[4]);
										poll_data.minute=atoi(&buf[7]);
										poll_data.second=atoi(&buf[10]);
										separate_get_time_store->error=FALSE;
									}
									else{
										separate_get_time_store->error=TRUE;
									}
									process_post(&coap_process, get_time_response_event, NULL);
									break;

								case 'T':
									//Target
									if(buf[2]=='1'){
										poll_data.target_temperature = atoi(&buf[4]);
										separate_get_target_store->error=FALSE;
										poll_data.last_target_reading = clock_time();
									}
									else {
										separate_get_target_store->error=TRUE;
									}
									process_post(&coap_process, get_target_response_event, NULL);
									break;
								case 'V':
									//Valve
									if(buf[2]=='1'){
										uint8_t valve_state = atoi(&buf[4]);
										poll_data.valve_wanted = valve_state;
										separate_get_valve_wanted_store->error=FALSE;
										poll_data.last_valve_wanted_reading = clock_time();
									}
									else {
										separate_get_valve_wanted_store->error=TRUE;
									}
									process_post(&coap_process, get_valve_wanted_response_event, NULL);
									break;
						/*		case 'P':
									//Predefined Temperature
									if(buf[2]=='1'){
										int temp;
										sscanf_P(&buf[5], PSTR("%x"), &temp);
										temp *= 50;
										poll_data.frost_temperature = temp;
										sscanf_P(&buf[9], PSTR("%x"), &temp);
										temp *= 50;
										poll_data.energy_temperature = temp;
										sscanf_P(&buf[13], PSTR("%x"), &temp);
										temp *= 50;
										poll_data.comfort_temperature = temp;
										sscanf_P(&buf[17], PSTR("%x"), &temp);
										temp *= 50;
										poll_data.supercomfort_temperature = temp;
										poll_data.last_predefined_reading = clock_time();
										separate_get_predefined_store->error=FALSE;
									}
									else{
										separate_get_predefined_store->error=TRUE;
									}
									process_post(&coap_process, get_predefined_response_event, NULL);
									break;
								case 'S':
									//Day Slots
									if(buf[2]=='1'){
										int day;
										sscanf_P(&buf[4],PSTR("%d"), &day);
										int i;
										for (i=0;i<8;i++){
											char temp=buf[6+i*5];
											poll_data.timers[day][i].mode = atoi(&temp);
											sscanf_P(&buf[7+i*5], PSTR("%x"), &poll_data.timers[day][i].time);
										}
										separate_get_slots_store->error=FALSE;
										poll_data.last_slots_reading[day] = clock_time();
									}
									else{
										separate_get_slots_store->error=TRUE;

									}
									process_post(&coap_process, get_slots_response_event, NULL);
									break;	
								*/
						
								case 'D':
									//Temperature Threshold
									if(buf[2]=='1'){
										poll_data.threshold_temperature = atoi(&buf[4]);
										separate_get_threshold_temp_store->error=FALSE;
										poll_data.last_threshold_temp_reading = clock_time();
									}
									else {
										separate_get_threshold_temp_store->error=TRUE;
									}
									process_post(&coap_process, get_threshold_temp_response_event, NULL);
									break;

								case 'A':
									//Battery Threshold
									if(buf[2]=='1'){
										poll_data.threshold_battery = atoi(&buf[4]);
										separate_get_threshold_bat_store->error=FALSE;
										poll_data.last_threshold_bat_reading = clock_time();
									}
									else {
										separate_get_threshold_bat_store->error=TRUE;
									}
									process_post(&coap_process, get_threshold_bat_response_event, NULL);
									break;

								default:
									//Unknown Response 
									break;
							}
							break;

						case 'E':
							//Event msg
							switch (buf[1]){
								case 'W':
									{
										//Wheel was turned
										uint8_t value = atoi(&buf[3]);

										if (value<116){
											poll_data.wheel_event_value -=2;
										}
										else if (value<128){
											poll_data.wheel_event_value -=1;
										} 
										else if (value>140){
											poll_data.wheel_event_value +=2;
										}
										else if (value>128){
											poll_data.wheel_event_value +=1;
										}
										process_post(&coap_process, changed_wheel_event, NULL);
										break;
									}
								case 'M':
									//Mode was changed by User
									poll_data.mode = atoi(&buf[3]);	
									process_post(&coap_process, changed_mode_event, NULL);
									printf_P(PSTR("GT\n"));
									break;
								case 'T':
									//Temperature changed
									poll_data.is_temperature = atoi(&buf[3]);
									process_post(&coap_process, changed_temp_event, NULL);
									break;
								case 'B':
									//Battery Value
									poll_data.battery = atoi(&buf[3]);
									process_post(&coap_process, changed_battery_event, NULL);
									break;

								case 'V':
									//Valve Value
									poll_data.valve_is = atoi(&buf[3]);
									process_post(&coap_process, changed_valve_event, NULL);
									break;


								case 'E':
									//ERROR MESSAGE
									{
										uint8_t code;
										uint8_t change;
										change = 0;	
										sscanf_P(&buf[3], PSTR("%x"), &code);
										if (code & BATT_LOW) {
											if (!(error_active & BATT_LOW)){
												error_active |= BATT_LOW;
												change = 1;
											}
										}
										if (code & BATT_WARNING) {
											if (!(error_active & BATT_WARNING)){
												error_active |= BATT_WARNING;
												change = 1;
											}
										}
										if (code & ERROR_MONTAGE) {
											if (!(error_active & ERROR_MONTAGE)){
												error_active |= ERROR_MONTAGE;
												change = 1;
											}
										}
										if (code & ERROR_MOTOR) {
											if (!(error_active & ERROR_MOTOR)){
												error_active |= ERROR_MOTOR;
												change = 1;
											}
										}
										if(!code){
											if (error_active){
												error_active = 0;
												change = 1;
											}
										}
										if (change) {
											process_post(&coap_process, error_event, NULL);
										}
										break;
									}		
								default:
									//Unkown Event
									break;
							}			

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



/*--------- Timers --------------------------------------------------------------*/
/*
static char * getEnergyLevelString(int mode){
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

	//char timerString[10];
	//snprintf_P(timerString, 10, (day == 0)?PSTR("weektimer"):PSTR("daytimer%d"), day);

	if ((len = REST.get_query(request, &query))){ //GET variable check
		if(isdigit(query[0]) && len == 1){
			char c[2];
			c[0]=query[0];
			c[1]='\0';
			slot = atoi(c);
			if(slot <= 7 && slot >= 0){
				success = 1;
			}
		}
	}
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Add a get parameter that specifies the slot"));
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strpos);
		printf_P(PSTR("GS%d\n"),day);
		return; //no or invalid GET variable specified
	}
	if (REST.get_method_type(request)==METHOD_PUT){
		const uint8_t * disable = NULL;
		len = coap_get_payload(request, &disable);
		if(len == 7){
			if(strncmp_P((char*)disable, PSTR("disable"), len)!=0){
				success = 0;
			}
			else{
				if (!separate_set_active){
					coap_separate_accept(request, &separate_set_store->request_metadata);
					separate_set_active = 1;
					printf_P(PSTR("SS%d%d0fff\n"),day,slot);
				}
				else {
					coap_separate_reject();
				}
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
						if(isdigit(time[0]) && isdigit(time[1]) && time[2]==':' && isdigit(time[3]) && isdigit(time[4]) ){ //digit checks
							int hour = atoi(&time[0]);
							//the time string is not NULL terminated
							char minutes[3];
							strncpy(minutes, &time[3], 2);
							minutes[2] = 0;
							int minute = atoi(minutes);
							if (!( 0<=hour && hour<=23 && 0<=minute && minute<=59 )){ //range checks
								success = 0; 
							}
							else{
								if (!separate_set_active){
									coap_separate_accept(request, &separate_set_store->request_metadata);
									separate_set_active = 1;
									printf_P(PSTR("SS%d%d%d%03x\n"),day,slot,level, hour*60 + minute);
								}
								else {
									coap_separate_reject();
								}	
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
	else{ //GET request
		if(poll_data.last_slots_reading[day] > clock_time()-60*CLOCK_SECOND) {
			uint16_t time = poll_data.timers[day][slot].time;
			if(time > 23*60 + 59){
				strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("disabled"));
			}
			else{
				strpos += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("%S at %02d:%02d"), getEnergyLevelString(poll_data.timers[day][slot].mode), time/60, time%60 );
			}
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strpos);
			printf_P(PSTR("GS%d\n"),day);

		}
		else{
			if (!separate_get_slots_active){
				coap_separate_accept(request, &separate_get_slots_store->request_metadata);
				separate_get_slots_store->day=day;
				separate_get_slots_store->slot=slot;
				separate_get_slots_active=1;
				printf_P(PSTR("GS%d\n"),day);
			}
			else {
				coap_separate_reject();
				printf_P(PSTR("GS%d\n"),day);
			}
		}
	}
}



void slots_finalize_handler(){
	if (separate_get_slots_active) {
		char buffer[30];
		coap_transaction_t *transaction = NULL;
		if( (transaction = coap_new_transaction(separate_get_slots_store->request_metadata.mid, &separate_get_slots_store->request_metadata.addr, separate_get_slots_store->request_metadata.port))) {
			coap_packet_t response[1]; // This way the packet can be treated as pointer as usual. 
			if(separate_get_slots_store->error) {
				coap_separate_resume(response, &separate_get_slots_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_slots_store->request_metadata, CONTENT_2_05);
				uint8_t day  = separate_get_slots_store->day;
				uint8_t slot  = separate_get_slots_store->slot;
				uint16_t time = poll_data.timers[day][slot].time;
				if(time > 23*60 + 59){
					snprintf_P((char*)buffer, 30, PSTR("disabled"));
				}
				else{
					snprintf_P((char*)buffer, 30, PSTR("%S at %02d:%02d"), getEnergyLevelString(poll_data.timers[day][slot].mode), time/60, time%60 );
				}
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));
			}
			coap_set_header_block2(response, separate_get_slots_store->request_metadata.block2_num, 0, separate_get_slots_store->request_metadata.block2_size);
			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_slots_active = 0;
		}
		else {
			separate_get_slots_active = 0;
		}
	}

}


RESOURCE(day1timer, METHOD_GET | METHOD_PUT, "set/slot/monday", "title=\"Slots Monday\";rt=\"settings slottate\"");
RESOURCE(day2timer, METHOD_GET | METHOD_PUT, "set/slot/tuesday", "title=\"Slots Tuesday\";rt=\"settings slotstate\"");
RESOURCE(day3timer, METHOD_GET | METHOD_PUT, "set/slot/wednesday", "title=\"Slots Wednesday\";rt=\"settings slotstate\"");
RESOURCE(day4timer, METHOD_GET | METHOD_PUT, "set/slot/thursday", "title=\"Slots Thursday\";rt=\"settings slotstate\"");
RESOURCE(day5timer, METHOD_GET | METHOD_PUT, "set/slot/friday", "title=\"Slots Friday\";rt=\"settings slotstate\"");
RESOURCE(day6timer, METHOD_GET | METHOD_PUT, "set/slot/saturday", "title=\"Slots Saturday\";rt=\"settings slotstate\"");
RESOURCE(day7timer, METHOD_GET | METHOD_PUT, "set/slot/sunday", "title=\"Slots Sunday\";rt=\"settings slotstate\"");

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




*/


/*--------- Predefined Temperatures ---------------------------------------------*/
/*
static void handle_temperature(int temperature, int index, void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	if (REST.get_method_type(request)==METHOD_GET){

		if(poll_data.last_predefined_reading > clock_time()-5*CLOCK_SECOND) {

			snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), temperature/100, temperature%100);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			printf_P(PSTR("GP\n"));
		}
		else {
			if (!separate_get_predefined_active) {
				coap_separate_accept(request, &separate_get_predefined_store->request_metadata);
				separate_get_predefined_store->index=index;
				separate_get_predefined_active = 1;
				printf_P(PSTR("GP\n"));
			}
			else {
				coap_separate_reject();
				printf_P(PSTR("GP\n"));
			}
		}
	}
	else{
		const uint8_t * string = NULL;
		int success = 1;
		uint8_t len = coap_get_payload(request, &string);

		uint16_t value;
		if(len == 2){
			if (isdigit(string[0]) && isdigit(string[1])){
				value = (atoi((char*) string)) * 10;
			}
			else {
				success = 0;
			}
		}
		else if (len == 3) {
			if (isdigit(string[0]) && isdigit(string[2]) && string[1]=='.'){
				value = (atoi((char*) string)) * 10;
				value += atoi((char*) string+2);
			}
			else {
				success = 0;
			}
		}
		else if(len == 4){
			if (isdigit(string[0]) && isdigit(string[1]) && isdigit(string[3]) && string[2]=='.'){
				value = (atoi((char*) string) *10);
				value += atoi((char*) string+3);
			}
			else {
				success = 0;
			}
		}
		else {
			success = 0;
		}
		if(success){
			if (!separate_set_active){
				uint16_t value =atoi((char*) string);
				coap_separate_accept(request, &separate_set_store->request_metadata);
				separate_set_active = 1;
				printf_P(PSTR("SP0%d%02x\n"),index, value/5);
			}
			else {
				coap_separate_reject();
			}

		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: ttt, e.g. 15.5 sets the temperature to 15.5 deg"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}
	}
}



void predefined_finalize_handler() {
	if (separate_get_predefined_active){
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if( (transaction = coap_new_transaction(separate_get_predefined_store->request_metadata.mid, &separate_get_predefined_store->request_metadata.addr, separate_get_predefined_store->request_metadata.port))) {
			coap_packet_t response[1]; // This way the packet can be treated as pointer as usual. 
			if(separate_get_predefined_store->error) {
				coap_separate_resume(response, &separate_get_predefined_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_predefined_store->request_metadata, CONTENT_2_05);
				int temperature=0;
				switch(separate_get_predefined_store->index){
					case 1:
						temperature = poll_data.frost_temperature;
						break;
					case 2:
						temperature = poll_data.energy_temperature;
						break;
					case 3:
						temperature = poll_data.comfort_temperature;
						break;
					case 4:
						temperature = poll_data.supercomfort_temperature;
						break;
				}

				snprintf_P(buffer, 10 , PSTR("%d.%02d"), temperature/100, temperature%100);
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));
			}
			coap_set_header_block2(response, separate_get_predefined_store->request_metadata.block2_num, 0, separate_get_predefined_store->request_metadata.block2_size);
			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_predefined_active = 0;
		}
		else {
			separate_get_predefined_active = 0;
		}
	}
}

RESOURCE(frost, METHOD_GET | METHOD_PUT, "config/predefined/frost", "title=\"Frost\";rt=\"config temperature\"");
RESOURCE(energy, METHOD_GET | METHOD_PUT, "config/predefined/energy", "title=\"Energy\";rt=\"config temperature\"");
RESOURCE(comfort, METHOD_GET | METHOD_PUT, "config/predefined/comfort", "title=\"Comfort\";rt=\"config temperature\"");
RESOURCE(supercomfort, METHOD_GET | METHOD_PUT, "config/predefined/supercomfort", "title=\"Supercomfort\";rt=\"config temperature\"");

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

*/


/*--------- Temperature ---------------------------------------------------------*/

EVENT_RESOURCE(temperature, METHOD_GET, "sensors/temperature", "title=\"Current temperature\";obs;rt=\"temperature\"");
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), poll_data.is_temperature/100, poll_data.is_temperature%100);
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

void temperature_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[6];

	++event_i;

	coap_packet_t notification[1]; // This way the packet can be treated as pointer as usual. 
	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
	coap_set_payload(notification, content, snprintf_P(content, 6, PSTR("%d.%02d"), poll_data.is_temperature/100, poll_data.is_temperature%100));

	REST.notify_subscribers(r, event_i, notification);

}

/*--------- Battery ---------------------------------------------------------*/

EVENT_RESOURCE(battery, METHOD_GET, "sensors/battery", "title=\"Battery voltage\";obs;rt=\"voltage\"");
void battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	snprintf_P((char*)buffer, preferred_size, PSTR("%u"), poll_data.battery);
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


void battery_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[10];

	++event_i;

	coap_packet_t notification[1]; // This way the packet can be treated as pointer as usual.
	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
	coap_set_payload(notification, content, snprintf_P(content, 10, PSTR("%u"), poll_data.battery));

	REST.notify_subscribers(r, event_i, notification);
}

/*--------- Valve is ---------------------------------------------------------*/

EVENT_RESOURCE(valve_is, METHOD_GET, "sensors/valve", "title=\"Valve Position\";obs;rt=\"valve\"");
void valve_is_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	snprintf_P((char*)buffer, preferred_size, PSTR("%u"), poll_data.valve_is);
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


void valve_is_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[10];

	++event_i;

	coap_packet_t notification[1]; // This way the packet can be treated as pointer as usual.
	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
	coap_set_payload(notification, content, snprintf_P(content, 10, PSTR("%u"),poll_data.valve_is));

	REST.notify_subscribers(r, event_i, notification);
}



/*--------- Target ---------------------------------------------------------*/
RESOURCE(target, METHOD_GET | METHOD_PUT, "set/target", "title=\"Target temperature\";rt=\"temperature\"");
void target_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		if(poll_data.last_target_reading>clock_time()-5*CLOCK_SECOND){
			snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), poll_data.target_temperature/100, poll_data.target_temperature%100);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			printf_P(PSTR("GT\n"));
		}
		else {
			if (!separate_get_target_active){
				coap_separate_accept(request, &separate_get_target_store->request_metadata);
				separate_get_target_active = 1;
				printf_P(PSTR("GT\n"));
			}
			else {
				coap_separate_reject();
				printf_P(PSTR("GT\n"));
			}
		}

	}
	else {
		const uint8_t * string = NULL;
		int success = 1;
		int len = coap_get_payload(request, &string);
		uint16_t value;
		if(len == 2){
			if (isdigit(string[0]) && isdigit(string[1])){
				value = (atoi((char*) string)) * 10;
			}
			else {
				success = 0;
			}
		}
		else if (len == 3) {
			if (isdigit(string[0]) && isdigit(string[2]) && string[1]=='.'){
				value = (atoi((char*) string)) * 10;
				value += atoi((char*) string+2);
			}
			else {
				success = 0;
			}
		}
		else if(len == 4){
			if (isdigit(string[0]) && isdigit(string[1]) && isdigit(string[3]) && string[2]=='.'){
				value = (atoi((char*) string) *10);
				value += atoi((char*) string+3);
			}
			else {
				success = 0;
			}
		}
		else {
			success = 0;
		}
		if(success){
			if (!separate_set_active){
				coap_separate_accept(request, &separate_set_store->request_metadata);
				separate_set_active = 1;
				snprintf_P(last_setting, 20, PSTR("ST%02x"),value/5);
				printf_P(PSTR("ST%02x\n"),value/5);
			}
			else {
				printf("%s\n",last_setting);
				coap_separate_reject();
			}

		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: tt.t, e.g. 15.5 sets the temperature to 15.5 deg"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}
	}
}

void target_finalize_handler() {
	if (separate_get_target_active){
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_get_target_store->request_metadata.mid, &separate_get_target_store->request_metadata.addr, separate_get_target_store->request_metadata.port)) ){
			coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */
			if(separate_get_target_store->error){
				coap_separate_resume(response, &separate_get_target_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_target_store->request_metadata, CONTENT_2_05);
				snprintf_P(buffer, 10 , PSTR("%d.%02d"), poll_data.target_temperature/100, poll_data.target_temperature%100);
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));
			}

			coap_set_header_block2(response, separate_get_target_store->request_metadata.block2_num, 0, separate_get_target_store->request_metadata.block2_size);

			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_target_active = 0;
		}
		else {
			separate_get_target_active = 0;
			/*
			 * TODO: ERROR HANDLING: Set timer for retry, send error message, ...
			 */	
		}
	}
}



/*--------- Threshold Temperature---------------------------------------------------------*/
/*
RESOURCE(threshold_temp, METHOD_GET | METHOD_PUT, "config/threshold_temperature", "title=\"Temperature Threshold\";rt=\"threshold\"");
void threshold_temp_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		if(poll_data.last_threshold_temp_reading>clock_time()-5*CLOCK_SECOND){
			snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), poll_data.threshold_temperature/100, poll_data.threshold_temperature%100);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			printf_P(PSTR("GD\n"));
		}
		else {
			if (!separate_get_threshold_temp_active){
				coap_separate_accept(request, &separate_get_threshold_temp_store->request_metadata);
				separate_get_threshold_temp_active = 1;
				printf_P(PSTR("GD\n"));
			}
			else {
				coap_separate_reject();
				printf_P(PSTR("GD\n"));
			}
		}

	}
	else {
		const uint8_t * string = NULL;
		int success = 1;
		int len = coap_get_payload(request, &string);
		uint16_t value;
		if(len == 2){
			if (isdigit(string[0]) && isdigit(string[1])){
				value = (atoi((char*) string)) * 10;
			}
			else {
				success = 0;
			}
		}
		else if (len == 3) {
			if (isdigit(string[0]) && isdigit(string[2]) && string[1]=='.'){
				value = (atoi((char*) string)) * 10;
				value += atoi((char*) string+2);
			}
			else {
				success = 0;
			}
		}
		else if(len == 4){
			if (isdigit(string[0]) && isdigit(string[1]) && isdigit(string[3]) && string[2]=='.'){
				value = (atoi((char*) string) *10);
				value += atoi((char*) string+3);
			}
			else {
				success = 0;
			}
		}
		else {
			success = 0;
		}
		if(success && value<255){
			if (!separate_set_active){
				coap_separate_accept(request, &separate_set_store->request_metadata);
				separate_set_active = 1;
				snprintf_P(*last_setting, strlen(last_setting), PSTR("SD%02x"),value);
				printf_P(PSTR("SD%02x\n"),value);
			}
			else {
				printf("%s\n",last_setting);
				coap_separate_reject();
			}

		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: tt.t, e.g. 1.0 sets the threshold to 1.0 deg"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}
	}
}

void threshold_temp_finalize_handler() {
	if (separate_get_threshold_temp_active){
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_get_threshold_temp_store->request_metadata.mid, &separate_get_threshold_temp_store->request_metadata.addr, separate_get_threshold_temp_store->request_metadata.port)) ){
			coap_packet_t response[1]; / This way the packet can be treated as pointer as usual. 
			if(separate_get_threshold_temp_store->error){
				coap_separate_resume(response, &separate_get_threshold_temp_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_threshold_temp_store->request_metadata, CONTENT_2_05);
				snprintf_P(buffer, 10 , PSTR("%d.%02d"), poll_data.threshold_temperature/100, poll_data.threshold_temperature%100);
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));
			}

			coap_set_header_block2(response, separate_get_threshold_temp_store->request_metadata.block2_num, 0, separate_get_threshold_temp_store->request_metadata.block2_size);

			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_threshold_temp_active = 0;
		}
		else {
			separate_get_threshold_temp_active = 0;
		}
	}
}

/*---------------Threshold Battery ---------------------------------------------*/
/*
RESOURCE(threshold_bat, METHOD_GET | METHOD_PUT, "config/threshold_battery", "title=\"Battery Threshold\";rt=\"threshold\"");
void threshold_bat_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		if(poll_data.last_threshold_bat_reading>clock_time()-5*CLOCK_SECOND){
			snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_data.threshold_battery);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			printf_P(PSTR("GA\n"));
		}
		else {
			if (!separate_get_threshold_bat_active){
				coap_separate_accept(request, &separate_get_threshold_bat_store->request_metadata);
				separate_get_threshold_bat_active = 1;
				printf_P(PSTR("GA\n"));
			}
			else {
				coap_separate_reject();
				printf_P(PSTR("GA\n"));
			}
		}

	}
	else
	{
		const uint8_t * string = NULL;
		int success = 1;
		uint16_t thresh;
		int len = coap_get_payload(request, &string);
		int i;
		for(i=0; i<len; i++){
			if (!isdigit(string[i])){
				success = 0;
				break;
			}
		}
		if(success){
			thresh = atoi((char*)string);
			if(!(thresh < 255 && thresh > 0)){
				success = 0;
			}
		}

		if(success){
			if (!separate_set_active){
				coap_separate_accept(request, &separate_set_store->request_metadata);
				separate_set_active = 1;
				printf_P(PSTR("SA%02x\n"),thresh);
				snprintf_P(last_setting, 20, PSTR("SA%02x"),thresh);
			}
			else {
				printf("%s\n",last_setting);
				coap_separate_reject();
			}

		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			return;
		}
	}
}

void threshold_bat_finalize_handler() {
	if (separate_get_threshold_bat_active){
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_get_threshold_bat_store->request_metadata.mid, &separate_get_threshold_bat_store->request_metadata.addr, separate_get_threshold_bat_store->request_metadata.port)) ){
			coap_packet_t response[1]; // This way the packet can be treated as pointer as usual.
			if(separate_get_threshold_bat_store->error){
				coap_separate_resume(response, &separate_get_threshold_bat_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_threshold_bat_store->request_metadata, CONTENT_2_05);
				snprintf_P(buffer, 10, PSTR("%d"), poll_data.threshold_battery);
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));
			}

			coap_set_header_block2(response, separate_get_threshold_bat_store->request_metadata.block2_num, 0, separate_get_threshold_bat_store->request_metadata.block2_size);

			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_threshold_bat_active = 0;
		}
		else {
			separate_get_threshold_bat_active = 0;
		}
	}
}



/*--------- Valve ---------------------------------------------------------*/
RESOURCE(valve_wanted, METHOD_GET | METHOD_PUT, "set/valve", "title=\"Valve opening\";rt=\"valve\"");
void valve_wanted_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		if(poll_data.last_valve_wanted_reading>clock_time()-5*CLOCK_SECOND){
			snprintf_P((char*)buffer, preferred_size, PSTR("%u"), poll_data.valve_wanted);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			printf_P(PSTR("GV\n"));
		}
		else {
			if (!separate_get_valve_wanted_active){
				coap_separate_accept(request, &separate_get_valve_wanted_store->request_metadata);
				separate_get_valve_wanted_active = 1;
				printf_P(PSTR("GV\n"));
			}
			else {
				coap_separate_reject();
				printf_P(PSTR("GV\n"));
			}
		}

	}
	else {
		const uint8_t * string = NULL;
		int success = 1;
		int len = coap_get_payload(request, &string);
		int new_valve = 0;

		if (len > 3) {
			success = 0;
		}
		else {
			int i;
			for (i=0; i<len; ++i) {
				if (!isdigit(string[i])) {
					success = 0;
					break;
				}
			}
		}
		if (success) {
			new_valve = atoi((char*)string);
			if (new_valve <0 || new_valve > 100) {
				success = 0;
			}
		}

		if(success){
			if (!separate_set_active){
				coap_separate_accept(request, &separate_set_store->request_metadata);
				separate_set_active = 1;
				snprintf_P(last_setting, 20, PSTR("SV%02x"),new_valve);
				printf_P(PSTR("SV%02x\n"),new_valve);
			}
			else {
				printf("%s\n",last_setting);
				coap_separate_reject();
			}
		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("payload format: aa, e.g. 47 sets the valve to 47 percent"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}
	}
}

void valve_wanted_finalize_handler() {
	if (separate_get_valve_wanted_active){
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_get_valve_wanted_store->request_metadata.mid, &separate_get_valve_wanted_store->request_metadata.addr, separate_get_valve_wanted_store->request_metadata.port)) ){
			coap_packet_t response[1]; /* this way the packet can be treated as pointer as usual. */
			if(separate_get_valve_wanted_store->error){
				coap_separate_resume(response, &separate_get_valve_wanted_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_valve_wanted_store->request_metadata, CONTENT_2_05);
				snprintf_P(buffer, 10, PSTR("%u"), poll_data.valve_wanted);
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));
			}

			coap_set_header_block2(response, separate_get_valve_wanted_store->request_metadata.block2_num, 0, separate_get_valve_wanted_store->request_metadata.block2_size);
			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_valve_wanted_active = 0;
		}
		else {
			separate_get_valve_wanted_active = 0;
			/*
			 * todo: error handling: set timer for retry, send error message, ...
			 */
		}
	}
}

/*---- mode event ------------------------------------------------------*/

EVENT_RESOURCE(mode, METHOD_GET | METHOD_PUT, "set/mode", "title=\"mode event\";rt=\"mode\"");

void
mode_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{	
	if (REST.get_method_type(request)==METHOD_GET) {

		printf_P(PSTR("GM\n"));

		static  char msg[20];

		if (poll_data.mode==manual_target) {
			strncpy_P((char*)msg, PSTR("manual target"), 19);
		}
		else if (poll_data.mode==manual_timers) {
			strncpy_P((char*)msg, PSTR("manual timer"), 19);
		}
		else if (poll_data.mode==auto_target) {
			strncpy_P((char*)msg, PSTR("auto target"), 19);
		}
		else if (poll_data.mode==auto_timers) {
			strncpy_P((char*)msg, PSTR("auto timer"), 19);
		}
		else if (poll_data.mode==auto_valve) {
			strncpy_P((char*)msg, PSTR("auto valve"), 19);
		}
		else {
			strncpy_P((char*)msg, PSTR("undefined"), 19);
		}

		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response,msg, strlen(msg));
	}
	else {
		const uint8_t * string = NULL;
		uint8_t success = 1;
		char cmd[6];
		int len = coap_get_payload(request, &string);
		if(len == 0){ 
			success = 0;
		}
		else{
			if(strncmp_P((char*)string,PSTR("manual target"),MAX(len,13))==0){
				strncpy_P((char*)cmd, PSTR("SM00\n"),6);
			}
			else if(strncmp_P((char*)string,PSTR("manual timer"),MAX(len,12))==0){
				strncpy_P((char*)cmd, PSTR("SM01\n"),6);
			}
			else if(strncmp_P((char*)string, PSTR("auto target"),MAX(len,11))==0){
				strncpy_P((char*)cmd, PSTR("SM02\n"),6);
			}
			else if(strncmp_P((char*)string, PSTR("auto valve"),MAX(len,10))==0){
				strncpy_P((char*)cmd, PSTR("SM03\n"),6);
			}
			else if(strncmp_P((char*)string, PSTR("auto timer"),MAX(len,10))==0){
				strncpy_P((char*)cmd, PSTR("SM04\n"),6);
			}
			else{
				success = 0;
			}
		}
		if (success) {
			if (!separate_set_active){
				coap_separate_accept(request, &separate_set_store->request_metadata);
				separate_set_active = 1;
				snprintf_P(last_setting, 20, PSTR("%s"),cmd);
				printf("%s",cmd);
			}
			else {
				printf("%s\n",last_setting);
				coap_separate_reject();
			}
		}
		else {
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			return;
		}
	}
}

	void
mode_event_handler(resource_t *r)
{
	static char content[20];
	static uint32_t event_counter = 0;

	event_counter++;

	if (poll_data.mode==manual_target)
	{
		strncpy_P((char*)content, PSTR("manual target"),19);
	}
	else if (poll_data.mode==manual_timers)
	{
		strncpy_P((char*)content, PSTR("manual programmed"),19);
	}
	else if (poll_data.mode==auto_target)
	{
		strncpy_P((char*)content, PSTR("auto target"),19);
	}
	else if (poll_data.mode==auto_timers)
	{
		strncpy_P((char*)content, PSTR("auto programmed"),19);
	}
	else if (poll_data.mode==auto_valve)
	{
		strncpy_P((char*)content, PSTR("auto valve"),19);
	}
	else
	{
		strncpy_P((char*)content, PSTR("undefined"),19);
	}
	coap_packet_t notification[1]; /* this way the packet can be treated as pointer as usual. */
	coap_init_message(notification, COAP_TYPE_CON, CONTENT_2_05, 0 );
	coap_set_payload(notification, content, strlen(content));

	REST.notify_subscribers(r, event_counter, notification);
}

/*---- wheel event -----------------------------------------------------*/

EVENT_RESOURCE(wheel, METHOD_GET, "sensors/wheel", "title=\"Wheel Event\";obs;rt=\"wheel\"");

void
wheel_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	snprintf_P((char*)buffer, preferred_size, PSTR("%li"), poll_data.wheel_event_value);
	/* Usually, a CoAP server would response with the current resource representation. */
	REST.set_response_payload(response, buffer, strlen((char*)buffer));

}

	void
wheel_event_handler(resource_t *r)
{
	static char content[12];
	static uint32_t event_counter = 0;

	event_counter++;

	coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
	coap_set_payload(notification, content, snprintf(content, sizeof(content), "%li", poll_data.wheel_event_value));

	REST.notify_subscribers(r, event_counter, notification);
}


/*---- Date & Time -----------------------------------------------------*/

RESOURCE(date, METHOD_GET | METHOD_PUT, "config/date", "title=\"Thermostat date\";rt=\"date:dd.mm.yy\"");

void date_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET){

		if (!separate_get_date_active){
			coap_separate_accept(request, &separate_get_date_store->request_metadata);
			separate_get_date_active = 1;
			printf_P(PSTR("GY\n"));
		}
		else {
			coap_separate_reject();
			printf_P(PSTR("GY\n"));
		}

	}
	else{
		const uint8_t * string = NULL;
		int success = 1;
		int length = coap_get_payload(request, &string);
		if( length == 8 ){
			int day=atoi((char*)&string[0]);
			int month=atoi((char*)&string[3]);
			int year=atoi((char*)&string[6]);

			if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]) && isdigit(string[6]) && isdigit(string[7]))){ //digit check
				success=0;
			}
			else if ( string[2]!='.' || string[5]!='.' ){ //delimiter check
				success=0;
			}
			else if (!(0<=year && year <=99 && 1<=month && month<=12 && 1<=day)){ //month and day check
				success=0;
			}
			else if( (month==4 || month==6 || month==9 || month==11) && day>30){ //30 days check
				success=0;
			}
			else if( month==2 && !((year%4)==0) && day > 28) { //no leap year check
				success=0;
			}
			else if( month==2 && day>29){ //leap year check
				success=0;
			}
			else if( day > 31){ //31 days check
				success=0;
			}

			if(success){
				if (!separate_set_active){
					coap_separate_accept(request, &separate_set_store->request_metadata);
					separate_set_active = 1;
					snprintf_P(last_setting, 20, PSTR("SY%02x%02x%02x"),year,month,day);
					printf_P(PSTR("SY%02x%02x%02x\n"),year,month,day);
				}
				else {
					printf("%s\n",last_setting);
					coap_separate_reject();
				}
			}
		}
		else{
			success = 0;
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: dd.mm.yy"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}
	}
}

void date_finalize_handler() {

	if (separate_get_date_active)
	{
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_get_date_store->request_metadata.mid, &separate_get_date_store->request_metadata.addr, separate_get_date_store->request_metadata.port)) ){
			coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */
			if(separate_get_date_store->error){
				coap_separate_resume(response, &separate_get_date_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_date_store->request_metadata, CONTENT_2_05);
				snprintf_P(buffer, 9, PSTR("%02d.%02d.%02d"), poll_data.day, poll_data.month, poll_data.year);
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));

			}
			coap_set_header_block2(response, separate_get_date_store->request_metadata.block2_num, 0, separate_get_date_store->request_metadata.block2_size);

			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_date_active = 0;
		}
		else {
			separate_get_date_active = 0;
			/*
			 * TODO: ERROR HANDLING: Set timer for retry, send error message, ...
			 */
		}
	}		
}



RESOURCE(time, METHOD_GET | METHOD_PUT, "config/time", "title=\"Thermostat time\";rt=\"time:hh:mm:ss\"");
void time_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET){


		if (!separate_get_time_active){
			coap_separate_accept(request, &separate_get_time_store->request_metadata);
			separate_get_time_active = 1;
			printf_P(PSTR("GH\n"));
		}
		else {
			coap_separate_reject();
			printf_P(PSTR("GH\n"));
		}

	}
	else{
		const uint8_t * string = NULL;
		int success = 1;
		int length = coap_get_payload(request, &string);
		if(length==8 || length==5){
			int hour=atoi((char*)&string[0]);
			int minute=atoi((char*)&string[3]);
			int second=(length==5)?0:atoi((char*)&string[6]);

			if (length==8 && ! (isdigit(string[6]) && isdigit(string[7]) && string[5]==':' )){ //seconds digit and delimiter checks
				success = 0;
			}
			else if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]))){ //hours and minute digit checks
				success = 0;
			}
			else if ( string[2]!=':' ){ //delimiter check
				success = 0;
			}
			else if (!( 0<=hour && hour<=23 && 0<=minute && minute<=59 && 0<=second && second<=59)){ //range check
				success = 0;
			}


			if(success){
				if (!separate_set_active){

					coap_separate_accept(request, &separate_set_store->request_metadata);
					separate_set_active = 1;
					snprintf_P(last_setting, 20, PSTR("SH%02x%02x%02x\n"),hour,minute,second);
					printf_P(PSTR("SH%02x%02x%02x\n"),hour,minute,second);
				}
				else {
					printf("%s", last_setting);
					coap_separate_reject();
				}
			}
		}
		else{
			success = 0;
		}
		if(!success){
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			strncpy_P((char*)buffer, PSTR("Payload format: hh:mm[:ss]"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}

	}
}


void time_finalize_handler() {

	if (separate_get_time_active)
	{
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_get_time_store->request_metadata.mid, &separate_get_time_store->request_metadata.addr, separate_get_time_store->request_metadata.port)) ){
			coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */
			if(separate_get_time_store->error){
				coap_separate_resume(response, &separate_get_time_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_get_time_store->request_metadata, CONTENT_2_05);
				snprintf_P(buffer, 9, PSTR("%02d:%02d:%02d"), poll_data.hour, poll_data.minute, poll_data.second);
				REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
				coap_set_payload(response, buffer, strlen(buffer));

			}

			coap_set_header_block2(response, separate_get_time_store->request_metadata.block2_num, 0, separate_get_time_store->request_metadata.block2_size);

			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_time_active = 0;
		}
		else {
			separate_get_time_active = 0;
			/*
			 * TODO: ERROR HANDLING: Set timer for retry, send error message, ...
			 */
		}
	}
}





/*------------- SET RESPONSE HANDLER ------------------------------------------------------------------*/

void set_finalize_handler() {

	if (separate_set_active)
	{
		coap_transaction_t *transaction = NULL;
		if ( (transaction = coap_new_transaction(separate_set_store->request_metadata.mid, &separate_set_store->request_metadata.addr, separate_set_store->request_metadata.port)) ){
			coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */
			if(separate_set_store->error){
				coap_separate_resume(response, &separate_set_store->request_metadata, INTERNAL_SERVER_ERROR_5_00);
			}
			else {
				coap_separate_resume(response, &separate_set_store->request_metadata, CHANGED_2_04);

			}	
			coap_set_header_block2(response, separate_set_store->request_metadata.block2_num, 0, separate_set_store->request_metadata.block2_size);

			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_set_active = 0;
		}
		else {
			separate_set_active = 0;
			/*
			 * TODO: ERROR HANDLING: Set timer for retry, send error message, ...
			 */
		}
	}
}


/*--------- Error Address ------------------------------------------------------------*/
/*
RESOURCE(error_address, METHOD_GET | METHOD_PUT, "config/error_address", "title=\"Error URI\";rt=\"uri\"");
void error_address_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		if(error_uri[0]=='/'){
			snprintf_P((char*)buffer, preferred_size, PSTR("[%x:%x:%x:%x:%x:%x:%x:%x]%s"), error_ip[0],error_ip[1],error_ip[2],error_ip[3],error_ip[4],error_ip[5],error_ip[6],error_ip[7],error_uri);
		}
		else{
			snprintf_P((char*)buffer, preferred_size, PSTR("[%x:%x:%x:%x:%x:%x:%x:%x]/%s"), error_ip[0],error_ip[1],error_ip[2],error_ip[3],error_ip[4],error_ip[5],error_ip[6],error_ip[7],error_uri);

		}
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else
	{
		int success = 1;
		const uint8_t * string = NULL;
		int len;

		len = coap_get_payload(request, &string);
		uint16_t ip[8];
		uint16_t port;
		char uri[50];
		if (sscanf(string,"%*c %x %*c %x %*c %x %*c %x %*c %x %*c %x %*c %x %*c %x %*c %*c %x %s", &ip[0],&ip[1],&ip[2],&ip[3],&ip[4],&ip[5],&ip[6],&ip[7], &port,uri)==10){
			int i;
			for (i=0; i<8;i++){
				error_ip[i]=ip[i];
				eeprom_write_word(&ee_error_ip[i],ip[i]);

			}
			error_port = port;
			eeprom_write_word(&ee_error_port,port);
			strncpy(error_uri,uri,50);
			eeprom_write_block(uri,ee_error_uri,50);
		}
		else{
			success=0;
		}

		if(success){
			REST.set_response_status(response,CHANGED_2_04);
		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
		}
	}


	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
}




/*--------- Node Identifier ------------------------------------------------------------*/
RESOURCE(identifier, METHOD_GET | METHOD_PUT, "config/identifier", "title=\"Identifier\";rt=\"id\"");
void identifier_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%s"), identifier);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else
	{
		int success = 1;
		const uint8_t * string = NULL;
		int len;

		len = coap_get_payload(request, &string);
		if (len > 3){
			strncpy(identifier,string,50);
			eeprom_write_block(identifier,ee_identifier,50);
		}
		else{
			success=0;
		}

		if(success){
			REST.set_response_status(response,CHANGED_2_04);
		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
		}
	}


	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
}




/*--------- Error ---------------------------------------------------------*/
/*
EVENT_RESOURCE(error, METHOD_GET, "sensors/error", "title=\"Error\";obs;rt=\"error\"");
void error_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

	if(!error_active) {
		snprintf_P((char *)buffer, preferred_size, PSTR("No errors"));
	}
	else {
		int p=0;
		if (error_active & ERROR_MOTOR) {
			p+=snprintf_P((char *)buffer+p, 4, PSTR("E3\n"));
		}
		if (error_active & ERROR_MONTAGE) {
			p += snprintf_P((char *)buffer+p, 4, PSTR("E2\n"));
		}
		if (error_active & BATT_LOW){
			p += snprintf_P((char *)buffer+p, 15, PSTR("Battery empty\n"));
		}
		else if( error_active & BATT_WARNING) {
			p += snprintf_P((char *)buffer+p, 15, PSTR("Battery low\n"));

		}
	}

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


void error_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[30];

	++event_i;

	coap_packet_t notification[1]; 
	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );

	if(!error_active) {
		snprintf_P(content, 30, PSTR("No errors"));
	}
	else {
		int p;
		p=0;
		if (error_active & ERROR_MOTOR) {
			p+=snprintf_P(content+p, 4, PSTR("E3"));
		}
		if (error_active & ERROR_MONTAGE) {
			p += snprintf_P(content+p, 4, PSTR("E2"));
		}
		if (error_active & BATT_LOW){
			p += snprintf_P(content+p, 15, PSTR("Battery empty"));
		}
		else if( error_active & BATT_WARNING) {
			p += snprintf_P(content+p, 15, PSTR("Battery low"));
		}
	}

	coap_set_payload(notification, content, strlen(content));

	REST.notify_subscribers(r, event_i, notification);

}



/*-------------------- Version ---------------------------------------------------------------------------*/
RESOURCE(version, METHOD_GET | METHOD_PUT, "debug/version", "title=\"Version Number\";rt=\"string\"");
void version_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

	snprintf_P((char*)buffer, preferred_size, PSTR("%s"), VERSION);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}




/*------------------- HeartBeat --------------------------------------------------------------------------*/
PERIODIC_RESOURCE(heartbeat, METHOD_GET, "debug/heartbeat", "title=\"heartbeat\";obs;rt=\"heartbeat\"",60*CLOCK_SECOND);
void heartbeat_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

	snprintf_P((char*)buffer, preferred_size, PSTR("version:%s,uptime:%lu,rssi:%i"),VERSION,clock_seconds(),rssi_avg);
 	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

void heartbeat_periodic_handler(resource_t *r){
	static uint32_t event_counter=0;
	static char	content[50];

	++event_counter;
	
	int new = radio_sensor.value(RADIO_SENSOR_LAST_PACKET);
	rssi_value[rssi_position]=new;
	if(rssi_count<5){
		rssi_count++;
	}
	rssi_avg =(rssi_count>0)?(rssi_value[0]+rssi_value[1]+rssi_value[2]+rssi_value[3]+rssi_value[4])/rssi_count:0;

	rssi_position++;
	rssi_position = (rssi_position) % 5;

	coap_packet_t notification[1];
	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0);
	coap_set_payload(notification, content, snprintf_P(content, sizeof(content), PSTR("version:%s,uptime:%lu,rssi:%i"),VERSION,clock_seconds(),rssi_avg));

	REST.notify_subscribers(r, event_counter, notification);

}

/*--------- RD MSG ACK Handle-------------------------------------*/

void rd_post_response_handler(void *response){

	if(response==NULL){
		return;
	}

	if (((coap_packet_t *) response)->code == CREATED_2_01 || ((coap_packet_t *) response)->code == CHANGED_2_04 ) {
		coap_get_header_location_path(response, &location);
		strcpy(loc,location);
		registred=1;
		stimer_set(&rdput, 3600);
	}
}

void rd_put_response_handler(void *response){
	if(response==NULL){
		return;
	}
	if (((coap_packet_t *) response)->code !=CHANGED_2_04 ) {
		registred=0;
		stimer_set(&rdpost, 300);
	}
	else{
		stimer_set(&rdput, 3600);
	}
	
}


/*--------- COAP PROCESS ----------------------------------------------------------------*/

PROCESS_THREAD(coap_process, ev, data)
{
	PROCESS_BEGIN();

	rest_init_engine();

	//activate the resources
	SENSORS_ACTIVATE(radio_sensor);

	COAP_RD_SET_IPV6(&rd_ipaddr);
	rest_activate_resource(&resource_date);

	rest_activate_resource(&resource_time);
	rest_activate_resource(&resource_valve_wanted);
	rest_activate_resource(&resource_target);
//	rest_activate_resource(&resource_threshold_temp);
//	rest_activate_resource(&resource_threshold_bat);
//	rest_activate_resource(&resource_error_address);
	rest_activate_resource(&resource_identifier);
	// rest_activate_resource(&resource_version);

	rest_activate_event_resource(&resource_temperature);
	rest_activate_event_resource(&resource_battery);
	rest_activate_event_resource(&resource_mode);
	rest_activate_event_resource(&resource_valve_is);
	rest_activate_event_resource(&resource_wheel);
//	rest_activate_event_resource(&resource_error);
	rest_activate_periodic_resource(&periodic_resource_heartbeat);

/*
	rest_activate_resource(&resource_frost);
	rest_activate_resource(&resource_energy);
	rest_activate_resource(&resource_comfort);
	rest_activate_resource(&resource_supercomfort);

	rest_activate_resource(&resource_day1timer);
	rest_activate_resource(&resource_day2timer);
	rest_activate_resource(&resource_day3timer);
	rest_activate_resource(&resource_day4timer);
	rest_activate_resource(&resource_day5timer);
	rest_activate_resource(&resource_day6timer);
	rest_activate_resource(&resource_day7timer);
*/
	stimer_set(&rdpost, 60);
	etimer_set(&event_gen, 5*CLOCK_SECOND);

	while(1){
		PROCESS_WAIT_EVENT();
		if (ev == changed_temp_event){
			temperature_event_handler(&resource_temperature);
		}
		else if (ev == changed_valve_event){
			valve_is_event_handler(&resource_valve_is);
		}
		else if (ev == changed_battery_event){
			battery_event_handler(&resource_battery);
		}

		else if (ev == changed_wheel_event){
			wheel_event_handler(&resource_wheel);
		}
		else if (ev == changed_mode_event){
			mode_event_handler(&resource_mode);
		}
		else if (ev == get_date_response_event){
			date_finalize_handler();
		}
		else if (ev == get_time_response_event){
			time_finalize_handler();
		}
		else if (ev == get_target_response_event){
			target_finalize_handler();
		}
/*		else if (ev == get_threshold_temp_response_event){
			threshold_temp_finalize_handler();
		}
		else if (ev == get_threshold_bat_response_event){
			threshold_bat_finalize_handler();
		}
*/		else if (ev == get_valve_wanted_response_event){
			valve_wanted_finalize_handler();
		}/*
		else if (ev == get_predefined_response_event){
			predefined_finalize_handler();
		}
		else if (ev == get_slots_response_event){
			slots_finalize_handler();
		}*/
		else if (ev == set_response_event){
			set_finalize_handler();
/*		}
		else if (ev == error_event){

			error_event_handler(&resource_error);

			uip_ip6addr_t error_server_ipaddr;
			static coap_packet_t post[1];
			uip_ip6addr(&error_server_ipaddr,error_ip[0],error_ip[1],error_ip[2],error_ip[3],error_ip[4],error_ip[5],error_ip[6],error_ip[7]);
			coap_init_message(post,COAP_TYPE_CON, COAP_POST,0);

			coap_set_header_uri_path(post,error_uri);
			char buffer[80];

			if(!error_active) {
				snprintf_P(buffer, 80, PSTR("%s\n All errors fixed"), identifier);
			}
			else {
				int p;
				p = snprintf_P(buffer, 51, PSTR("%s\n"), identifier);
				if (error_active & ERROR_MOTOR) {
					p+=snprintf_P(buffer+p, 4, PSTR("E3\n"));
				}
				if (error_active & ERROR_MONTAGE) {
					p += snprintf_P(buffer+p, 4, PSTR("E2\n"));

				}
				if (error_active & BATT_LOW){
					p += snprintf_P(buffer+p, 15, PSTR("Battery empty\n"));
				}
				else if( error_active & BATT_WARNING) {
					p += snprintf_P(buffer+p, 15, PSTR("Battery low\n"));
				}
			}
			coap_set_payload(post, buffer, strlen(buffer));

			COAP_BLOCKING_REQUEST(&error_server_ipaddr, UIP_HTONS(error_port), post, error_msg_response_handler);
*/
		}
	
		if (!registred && stimer_expired(&rdpost)) {
				static coap_packet_t post[1];
				coap_init_message(post,COAP_TYPE_CON, COAP_POST,0);

				coap_set_header_uri_path(post,"/rd");
				const char query[50];
				uint8_t addr[8]=EUI64_ADDRESS;

				snprintf(query,49,"ep=\"%x-%x-%x-%x-%x-%x-%x-%x\"&rt=\"%s\"", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],EPTYPE);
				coap_set_header_uri_query(&post,query); 	

				COAP_BLOCKING_REQUEST(&rd_ipaddr, COAP_RD_PORT , post, rd_post_response_handler);

				stimer_set(&rdpost, 300);

		}
		if (registred && stimer_expired(&rdput)) {
				static coap_packet_t put[1];
				coap_init_message(put,COAP_TYPE_CON, COAP_PUT,0);

				coap_set_header_uri_path(put,loc);

				COAP_BLOCKING_REQUEST(&rd_ipaddr, COAP_RD_PORT , put, rd_put_response_handler);

				stimer_set(&rdput, 3600);
		}
		if(etimer_expired(&event_gen)) {
			etimer_set(&event_gen, 5 * CLOCK_SECOND);
		}	
		
	
	}

	PROCESS_END();

}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&honeywell_process, &coap_process, &sensors_process);
/*---------------------------------------------------------------------------*/
