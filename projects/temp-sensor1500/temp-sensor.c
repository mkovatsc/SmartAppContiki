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
/* ADC PIN = PF2 = ADC2 */

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
#include "er-coap-07-separate.h"

#include "dev/radio-sensor.h"
#include "dev/adc.h"

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0

#define VERSION "0.11.5"
#define EPTYPE "SteelHead"


/*--PROCESSES----------------------------------------------------------------*/
PROCESS(coap_process, "coap");
SENSORS(&radio_sensor);

/*---------------------------------------------------------------------------*/
static uip_ip6addr_t rd_ipaddr;

static struct etimer adc;
static struct	stimer rdpost;
static struct stimer rdput;
static char * location;
static char loc[40];
static uint8_t registred = 0;

static int16_t temperature;
static int16_t temperature_array[4];
static int8_t temperature_position=0;
static int16_t temperature_last;
static process_event_t changed_temperature_event;
//clock_time_t last_temperature_reading;

static int16_t threshold = 20;
static uint8_t poll_time = 5;

static int16_t rssi_value[5];
static int16_t rssi_count=0;
static int16_t rssi_position=0;
static int16_t rssi_avg=0;

const uint16_t lookup[] PROGMEM = {-7422,-3169,-2009,-1280,-737,-300,70,393,679,939,1176,1395,1600,1792,1974,2146,2310,2467,2617,2762,2902,3037,3169,3296,3420,3541,3659,3775,3888,3999,4108,4215,4320,4423,4526,4626,4726,4824,4921,5018,5113,5207,5301,5394,5487,5578,5670,5760,5851,5941,6030,6120,6209,6298,6387,6476,6565,6653,6742,6831,6920,7010,7100,7189,7279};

char ee_identifier[50] EEMEM;
char identifier[50];

/*--ADC-IMPLEMENTATION-----------------------------------------*/
static void read_temperature(void){
	static int16_t reading;
	static int16_t raw_temperature;
	static int16_t delta;
	static int16_t low;
	static int16_t high;

	adc_init(ADC_CHAN_ADC2, ADC_MUX5_0,ADC_TRIG_FREE_RUN, ADC_REF_INT_1_6, ADC_PS_32, ADC_ADJ_RIGHT);

	reading = doAdc(ADC_CHAN_ADC2, ADC_MUX5_0, 2);
	
	adc_deinit();
	/*--- Computation for real value, with x ohm resistor (Steinhart-Hart-Equation)
	temp = log(x/(reading/3.3*1.6/1024)-x);
	kelvin = 1 / (0.001129148 + (0.000234125 * temp) + (8.76741e-8 * temp * temp * temp) );
	celsius = kelvin-273.15;
	------*/

	printf("%i\n",reading);
	
	delta = reading % 16;
	low = pgm_read_word(&lookup[reading/16]);
	high = pgm_read_word(&lookup[(reading/16)+1]);
	raw_temperature=low+delta*(high-low)/16;

	if(raw_temperature < -2000 || raw_temperature > 5000){
		return;
	}
	
	temperature_array[temperature_position] = raw_temperature;

	temperature_position++;
	temperature_position = temperature_position%4;	

	temperature = (temperature_array[0]+temperature_array[1]+temperature_array[2]+temperature_array[3])/4;

	//last_temperature_reading =  clock_time();
	
	//printf_P(PSTR("Temp: %d.%02d\n"),new_temperature/100, new_temperature>0 ? new_temperature%100 : (-1*new_temperature)%100);

	if (temperature_last - threshold >= temperature || temperature_last + threshold <= temperature){
		temperature_last =  temperature;
		process_post(&coap_process, changed_temperature_event, NULL);
	}
}

/*--CoAP - Process---------------------------------------------------------*/

/*--------- Temperature ---------------------------------------------------------*/
EVENT_RESOURCE(temperature, METHOD_GET, "sensors/temperature", "title=\"Temperature\";obs;rt=\"temperature\"");
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
		snprintf_P((char*)buffer, preferred_size, PSTR(" %d.%02d"),temperature/100, temperature>0 ? temperature%100 : (-1*temperature)%100);
  		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

void temperature_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[10];

	++event_i;

  	coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  	coap_set_payload(notification, content, snprintf_P(content, 9, PSTR(" %d.%02d"),temperature/100, temperature>0 ? temperature%100 : (-1*temperature)%100));

	REST.notify_subscribers(r, event_i, notification);

}

/*--------- Threshold ---------------------------------------------------------*/
/*
RESOURCE(threshold, METHOD_GET | METHOD_PUT, "config/threshold", "title=\"Threshold temperature\";rt=\"threshold\"");
void threshold_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%d.%01d"), threshold/100, (threshold%100)/10);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else {
        	const uint8_t * string = NULL;
        	int success = 1;
        	int len = coap_get_payload(request, &string);
		uint16_t value;
		if(len == 1){
			if (isdigit(string[0])){
				value = (atoi((char*) string))*100;
			}
			else {
				success = 0;
			}
		}
		else if(len == 2){
			if (isdigit(string[0]) && isdigit(string[1])){
				value = (atoi((char*) string)) * 100;
			}
			else {
				success = 0;
			}
		}
		else if (len == 3) {
			if (isdigit(string[0]) && isdigit(string[2]) && string[1]=='.'){
				value = (atoi((char*) string) *100);
				value += atoi((char*) string+2)*10;
			}
			else {
				success = 0;
			}
		}
		else if(len == 4){
			if (isdigit(string[0]) && isdigit(string[1]) && isdigit(string[3]) && string[2]=='.'){
				value = (atoi((char*) string) *100);
				value += atoi((char*) string+3)*10;
			}
			else {
				success = 0;
			}
		}
		else {
			success = 0;
		}
	        if(success){
			threshold=value;
        		REST.set_response_status(response, REST.status.CHANGED);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	
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
*/


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


/*-------------------- Version ---------------------------------------------------------------------------*/
RESOURCE(version, METHOD_GET, "debug/version", "title=\"Version Number\";rt=\"version\"");
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
	char content[40];

	++event_counter;
	
	int new = radio_sensor.value(RADIO_SENSOR_LAST_PACKET);
	rssi_value[rssi_position]=new;
	if(rssi_count<5){
		rssi_count++;
	}
	rssi_position++;
	rssi_position = rssi_position % 5;

	rssi_avg = (rssi_count>0)?(rssi_value[0]+rssi_value[1]+rssi_value[2]+rssi_value[3]+rssi_value[4])/rssi_count:0;
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
//		printf("REGISTRED AT RD\n");
		registred=1;
		stimer_set(&rdput, 3600);
	}
}

void rd_put_response_handler(void *response){

	if(response==NULL){
		return;
	}
	if (((coap_packet_t *) response)->code != CHANGED_2_04) {
		registred=0;
		stimer_set(&rdpost, 300);
	}
	else{
//		printf("Updated Status at RD\n");
		stimer_set(&rdput, 3600);
	}
}




PROCESS_THREAD(coap_process, ev, data)
{
	PROCESS_BEGIN();

	rest_init_engine();

	SENSORS_ACTIVATE(radio_sensor);

	COAP_RD_SET_IPV6(&rd_ipaddr);

	eeprom_read_block(&identifier, ee_identifier, 50);

	rest_activate_event_resource(&resource_temperature);	
//	rest_activate_resource(&resource_threshold);
	rest_activate_resource(&resource_identifier);
	rest_activate_periodic_resource(&periodic_resource_heartbeat);

	DDRF = 0;      //set all pins of port b as inputs
	PORTF = 0;     //write data on port 

	changed_temperature_event = process_alloc_event();
	etimer_set(&adc, poll_time*CLOCK_SECOND);
	stimer_set(&rdpost, 60);

	while(1) {
		PROCESS_WAIT_EVENT();
		if (ev == changed_temperature_event){
			temperature_event_handler(&resource_temperature);	
		}

		if (!registred &&stimer_expired(&rdpost)) {
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
		if(etimer_expired(&adc)) {
				etimer_set(&adc, CLOCK_SECOND * poll_time);
				read_temperature();			


		}

	}
	
	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&coap_process, &sensors_process);
/*---------------------------------------------------------------------------*/
