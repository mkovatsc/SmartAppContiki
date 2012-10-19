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

/* USE PIN E7 on the ATmega128rfa, for the interrupt and a 1500 Ohms Pull-Down Resistor  */

/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"

#include "net/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "er-coap-07.h"
#include "er-coap-07-engine.h"
#include "er-coap-07-transactions.h"

#include "dev/radio-sensor.h"
#include "dev/motion-sensor.h"


//adds the debug resource that can be used to output the debug buffer
#define DEBUG 1

//sets the size of the request queue
#define REQUEST_QUEUE_SIZE 3

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0

#define VERSION "0.10.3"
#define EPTYPE "PIR-Sensor"


/*--PROCESSES----------------------------------------------------------------*/
PROCESS(coap_process, "coap");

SENSORS(&motion_sensor, &radio_sensor);

/*---------------------------------------------------------------------------*/
static uip_ip6addr_t rd_ipaddr;
//uip_ip6addr(&rd_ipaddr,0x2001,0x620,0x8,0x101f,0x0,0x0,0x0,0x1);


static struct etimer interval_timer;
static struct	stimer rdpost;
static struct stimer rdput;
static char * location;
static char loc[40];
static uint8_t registred = 0;

static uint8_t interval = 5;
static uint8_t event_number = 12;
static uint8_t last_state = 0;
static unsigned long move_count = 0;

char ee_identifier[50] EEMEM;
char identifier[50];
static unsigned long last_movement = 0;
static unsigned long start_movement = 0;
static unsigned long movement_duration = 0;

static int16_t rssi_value[5];
static int16_t rssi_count=0;
static int16_t rssi_position=0;
static int16_t rssi_avg=0;

/*--SIMPLE RESOURCES---------------------------------------------------------*/

EVENT_RESOURCE(motion, METHOD_GET, "sensors/motion", "title=\"Motion Sensor\";obs;rt=\"pir-sensor\"");

	void
motion_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	snprintf_P((char*) buffer, preferred_size, PSTR("%lu"), move_count);
	REST.set_response_payload(response, buffer, strlen((char*) buffer));

}

	void
event_motion_handler(resource_t *r)
{
	static uint32_t event_i = 0;
	static char content[12];


	++event_i;
	//	int state = motion_sensor.value(0);


	coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
	coap_init_message(notification, COAP_TYPE_CON, CONTENT_2_05, 0 );
	coap_set_payload(notification, content, snprintf(content, sizeof(content), "%lu", move_count));

	REST.notify_subscribers(r, event_i, notification);
}


/*--------- Number of events ---------------------------------------------------------*/
RESOURCE(trigger, METHOD_GET | METHOD_PUT, "config/triggerNumber", "title=\"Number of Events\";rt=\"number\"");
void trigger_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%u"), event_number);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else {
		const uint8_t * string = NULL;
		int success = 1;
		int len = coap_get_payload(request, &string);
		uint8_t value = 0;
		int i;
		for (i=0; i<len; i++){
			if(!isdigit(string[i])){
				success = 0;
				break;
			}
		}
		value = (atoi((char*) string));

		if(success && value > 0){
			event_number=value;
			REST.set_response_status(response, REST.status.CHANGED);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}
	}
}

/*--------- Event Interval ---------------------------------------------------------*/
RESOURCE(interval, METHOD_GET | METHOD_PUT, "config/interval", "title=\"Time Interval\";rt=\"seconds\"");
void interval_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%u"), interval);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else {
		const uint8_t * string = NULL;
		int success = 1;
		int len = coap_get_payload(request, &string);
		uint8_t value = 0;
		int i;
		for (i=0; i<len; i++){
			if(!isdigit(string[i])){
				success = 0;
				break;
			}
		}
		value = (atoi((char*) string));

		if(success && value > 2){
			interval=value;
			REST.set_response_status(response, REST.status.CHANGED);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

		}
		else{
			REST.set_response_status(response, REST.status.BAD_REQUEST);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
			return;
		}
	}
}

/*--------- Last Movement --------------------------------------------------------------*/
RESOURCE(last, METHOD_GET, "sensor/last", "title=\"Last Movement Detected\";rt=\"seconds\"");
void last_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	unsigned long now = clock_seconds();
	snprintf_P((char*)buffer, preferred_size, PSTR("%lu"), (now-last_movement < interval) ? 0 : (now-last_movement));
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));


}

/*--------- Node Identifier ------------------------------------------------------------*/
RESOURCE(identifier, METHOD_GET | METHOD_PUT, "config/identifier", "title=\"Identifer\";rt=\"string\"");
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


/*-------------------- Version ---------------------------------------------------------------------------*/
RESOURCE(version, METHOD_GET | METHOD_PUT, "debug/version", "title=\"Version Number\";rt=\"string\"");
void version_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

	snprintf_P((char*)buffer, preferred_size, PSTR("%s"), VERSION);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


/*------------------- HeartBeat --------------------------------------------------------------------------*/
PERIODIC_RESOURCE(heartbeat, METHOD_GET, "debug/heartbeat", "title=\"heartbeat\";obs;rt=\"string\"",60*CLOCK_SECOND);
void heartbeat_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

	snprintf_P((char*)buffer, preferred_size, PSTR("version:%s\nuptime:%lu\nrssi:%i"),VERSION,clock_seconds(),rssi_avg);
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
	coap_set_payload(notification, content, snprintf_P(content, sizeof(content), PSTR("version:%s\nuptime:%lu\nrssi:%i"),VERSION,clock_seconds(),rssi_avg));

	REST.notify_subscribers(r, event_counter, notification);

}

/*--------- RD MSG ACK Handle-------------------------------------*/

void rd_post_response_handler(void *response){

	if (((coap_packet_t *) response)->code == CREATED_2_01 || ((coap_packet_t *) response)->code == CHANGED_2_04 ) {
		coap_get_header_location_path(response, &location);
		strcpy(loc,location);
		printf("REGISTRED AT RD\n");
		registred=1;
		stimer_set(&rdput, 60);
	}
}

void rd_put_response_handler(void *response){

	if (((coap_packet_t *) response)->code == NOT_FOUND_4_04 ) {
		registred=0;
		stimer_set(&rdpost, 300);
	}
	else{
		printf("Updated Status at RD\n");
		stimer_set(&rdput, 3600);
	}
	
}


PROCESS_THREAD(coap_process, ev, data)
{
	PROCESS_BEGIN();

	DDRE &= ~_BV(PE7);
	PORTE &= ~_BV(PE7);
	
	rest_init_engine();

	SENSORS_ACTIVATE(radio_sensor);
	SENSORS_ACTIVATE(motion_sensor);

	printf("Sensors activated\n");

	DDRE &= ~_BV(PE7);
	PORTE &= ~_BV(PE7);
	
	static uint8_t event_counter = 0;
	COAP_RD_SET_IPV6(&rd_ipaddr);

	eeprom_read_block(&identifier, ee_identifier, 50);

	rest_activate_resource(&resource_identifier);
	rest_activate_event_resource(&resource_motion);
	rest_activate_resource(&resource_trigger);
	rest_activate_resource(&resource_interval);
	rest_activate_periodic_resource(&periodic_resource_heartbeat);

	etimer_set(&interval_timer, CLOCK_SECOND * interval);
	stimer_set(&rdpost, 60);

	while(1) {
		PROCESS_WAIT_EVENT();
		if (ev == sensors_event && data == &motion_sensor) {
			event_counter++;
		}

		else if (ev == PROCESS_EVENT_TIMER){
			if(etimer_expired(&interval_timer)){
				uint16_t next_interval; 
				uint8_t current_state;
				if (event_counter >= event_number-1){ //only fire event if multiple movements detected in short period
					current_state = 1;
					last_movement = clock_seconds();
					movement_duration = last_movement - start_movement;
					next_interval = interval*5;
				}
				else{
					start_movement = clock_seconds();
					current_state = 0;
					next_interval = interval;
				
				}
				if (last_state != current_state){
					if(current_state){
						if(move_count % 2){
							move_count += 2;
						}
						else{
							move_count += 1;
						}
					}
					else{
						if(!(move_count % 2)){
							move_count += 2;
						}
						else{
							move_count += 1;
						}
					}
					last_state = current_state;
					event_motion_handler(&resource_motion);
				}
				event_counter = 0;
				etimer_set(&interval_timer, CLOCK_SECOND * next_interval);
			}
		}
		if (!registred && stimer_expired(&rdpost)) {
				static coap_packet_t post[1];
				coap_init_message(post,COAP_TYPE_CON, COAP_POST,0);

				coap_set_header_uri_path(post,"/rd");
				const char query[40];
				uint8_t addr[8]=EUI64_ADDRESS;

				snprintf(query,39,"ep=\"%x-%x-%x-%x-%x-%x-%x-%x\"", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
				coap_set_header_uri_query(&post,query); 	

				printf("send post\n");
				COAP_BLOCKING_REQUEST(&rd_ipaddr, COAP_RD_PORT , post, rd_post_response_handler);
				stimer_set(&rdpost, 300);

		}
		if (registred && stimer_expired(&rdput)) {
				static coap_packet_t put[1];
				coap_init_message(put,COAP_TYPE_CON, COAP_PUT,0);

				coap_set_header_uri_path(put,loc);
				const char query[40];

				snprintf(query,39,"rt=\"%s\"",EPTYPE);
				coap_set_header_uri_query(&put,query); 	

				printf("send put\n");
				COAP_BLOCKING_REQUEST(&rd_ipaddr, COAP_RD_PORT , put, rd_put_response_handler);
				stimer_set(&rdput, 3600);
		}
	
	}

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&coap_process, &sensors_process);
/*---------------------------------------------------------------------------*/
