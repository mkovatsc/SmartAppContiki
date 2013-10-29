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

#include "contiki.h"
#include "contiki-net.h"
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"

#include "er-coap-engine.h"
#include "er-coap.h"

#include "honeywell.h"


/*------ PROCESSES ------------------------------------------------------------*/
PROCESS(honeywell_process, "control");
PROCESS(coap_process, "coap");

SENSORS(&radio_sensor);

/*---------------------------------------------------------------------------*/

extern resource_t
    res_datetime,
    res_battery,
    res_temperature,
    res_valve_current,
    res_wheel,
    res_mode,
    res_target,
    res_valve,
    res_channel,
    res_mac,
    res_reset,
    res_heartbeat,
    res_version,
    res_identifier;

application_separate_store_t separate_store_datetime;
application_separate_store_t separate_store_mode;
application_separate_store_t separate_store_target;
application_separate_store_t separate_store_valve;

static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};

static uip_ip6addr_t rd_ipaddr;

static struct	stimer rdpost;
static struct stimer rdput;
static const char * location;
static char loc[40];
static uint8_t registred = 0;

/* EEPROM variables */
char ee_identifier[50] EEMEM;
char ee_rdaddr[40] EEMEM = COAP_RD_ADDRESS;

/* events for observing/separate responses */
static process_event_t changed_valve_event;
static process_event_t changed_temp_event;
static process_event_t changed_battery_event;
static process_event_t changed_wheel_event;
static process_event_t changed_mode_event;

static process_event_t get_mode_response_event;
static process_event_t get_datetime_response_event;
static process_event_t get_target_response_event;
static process_event_t get_valve_response_event;

static process_event_t error_event;

static uint8_t error_active = 0;

//uint16_t error_ip[8];
//uint16_t error_port;
//char error_uri[50];
char identifier[50];
char rdaddrstr[40];
int channel_num;

poll_data_t poll_data;


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

  get_mode_response_event = process_alloc_event();
	get_datetime_response_event = process_alloc_event();
	get_target_response_event = process_alloc_event();
  get_valve_response_event = process_alloc_event();

	error_event = process_alloc_event();	

	poll_data.mode = manual_target; // default on HR20
	poll_data.threshold_battery = 10;

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
			while((rx=ringbuf_get(&uart_buf))!=-1) {
				if(buf_pos<126 && (char)rx=='\n') {
					buf[buf_pos++] = '\n';
					buf[buf_pos] = '\0';

					switch (buf[0]) {
						case 'S':
							if(buf[2]=='1') {
								//successful
								switch(buf[1]) {
                  case 'Y':
                    //DateTime
                    printf_P(PSTR("GY\n"));
                    break;
									case 'T':
									  //Target
										poll_data.mode=radio_target;
										printf_P(PSTR("GT\n"));
										break;
									case 'V':
									  //Valve
										poll_data.mode=radio_valve;
										printf_P(PSTR("GV\n"));
										break;
									case 'M':
									  //Mode
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
							}	else {
								//failure
							}
							break;

						case 'G':
							//Response for get 
							switch (buf[1]) {
								case 'M':
									//Mode
									if(buf[2]=='1') {
										poll_data.mode = atoi(&buf[4]);	
									} else {
                    separate_store_mode.error = 1;
                  }
                  process_post(&coap_process, get_mode_response_event, NULL);
									break;
                case 'Y':
                  //Date
                  if(buf[2]=='1') {
                    poll_data.year=atoi(&buf[4]);
                    poll_data.month=atoi(&buf[7]);
                    poll_data.day=atoi(&buf[10]);
                    poll_data.hour=atoi(&buf[13]);
                    poll_data.minute=atoi(&buf[16]);
                    poll_data.second=atoi(&buf[19]);
                  } else {
                    separate_store_datetime.error = 1;
                  }
                  process_post(&coap_process, get_datetime_response_event, NULL);
                  break;
								case 'T':
									//Target
									if(buf[2]=='1'){
										poll_data.target_temperature = atoi(&buf[4]);
										poll_data.last_target_reading = clock_time();
									} else {
                    separate_store_target.error = 1;
                  }

                  process_post(&coap_process, get_target_response_event, NULL);
									break;
								case 'V':
									//Valve
									if(buf[2]=='1') {
										poll_data.valve_wanted = atoi(&buf[4]);
										poll_data.last_valve_wanted_reading = clock_time();
									} else {
                    separate_store_valve.error = 1;
                  }
									printf("v\n");
                  process_post(&coap_process, get_valve_response_event, NULL);
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

  SENSORS_ACTIVATE(radio_sensor);

  eeprom_read_block(&rdaddrstr, ee_rdaddr, 40);
  uiplib_ipaddrconv(rdaddrstr, &rd_ipaddr);
  printf("Using RD %s\r\n", rdaddrstr);

  //activate the resources
  rest_activate_resource(&res_datetime, "config/datetime");
  rest_activate_resource(&res_identifier, "config/identifier");

  rest_activate_resource(&res_battery, "sensors/battery");
  rest_activate_resource(&res_temperature, "sensors/temperature");
  rest_activate_resource(&res_valve_current, "sensors/valve");
  rest_activate_resource(&res_wheel, "sensors/user");

  rest_activate_resource(&res_mode, "set/mode");
  rest_activate_resource(&res_target, "set/target");
  rest_activate_resource(&res_valve, "set/valve");

  rest_activate_resource(&res_channel, "debug/channel");
  rest_activate_resource(&res_heartbeat, "debug/heartbeat");
  rest_activate_resource(&res_mac, "debug/mac");
  rest_activate_resource(&res_reset, "debug/reset");
  rest_activate_resource(&res_version, "debug/version");

  stimer_set(&rdpost, 60);

  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == changed_temp_event) {
      res_temperature.trigger();
    } else if(ev == changed_valve_event) {
      res_valve_current.trigger();
    } else if(ev == changed_battery_event) {
      res_battery.trigger();
    } else if(ev == changed_wheel_event) {
      res_wheel.trigger();
    } else if(ev == get_mode_response_event) {
      res_mode.resume();
    } else if(ev == get_datetime_response_event) {
      res_datetime.resume();
    } else if(ev == get_target_response_event) {
      res_target.resume();
    } else if(ev == get_valve_response_event) {
      res_valve.resume();
    }

    if(!registred && stimer_expired(&rdpost)) {
      static coap_packet_t post[1];
      coap_init_message(post,COAP_TYPE_CON, COAP_POST,0);

      coap_set_header_uri_path(post,"/rd");
      char query[50];
      uint8_t addr[8]=EUI64_ADDRESS;

      snprintf(query, 49, "ep=\"%x-%x-%x-%x-%x-%x-%x-%x\"&rt=\"%s\"", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],EPTYPE);
      coap_set_header_uri_query(&post,query);

      COAP_BLOCKING_REQUEST(&rd_ipaddr, COAP_RD_PORT , post, rd_post_response_handler);

      stimer_set(&rdpost, 300);
    }

    if(registred && stimer_expired(&rdput)) {
      static coap_packet_t put[1];
      coap_init_message(put,COAP_TYPE_CON, COAP_PUT,0);

      coap_set_header_uri_path(put,loc);

      COAP_BLOCKING_REQUEST(&rd_ipaddr, COAP_RD_PORT , put, rd_put_response_handler);

      stimer_set(&rdput, 3600);
    }
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&honeywell_process, &coap_process, &sensors_process);
/*---------------------------------------------------------------------------*/
