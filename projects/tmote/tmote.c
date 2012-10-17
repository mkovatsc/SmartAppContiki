/*
 * Copyright (c) 2011, Matthias Kovatsch and other contributors.
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
 */

/**
 * \file
 *      Erbium (Er) REST Engine example (with CoAP-specific code)
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#define VERSION "0.8.2"
#define EPTYPE "Tmote-Sky"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "contiki.h"
#include "contiki-net.h"

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

#include "er-coap-07.h"
#include "er-coap-07-engine.h"
#include "er-coap-07-separate.h"
#include "er-coap-07-transactions.h"

#include "dev/radio-sensor.h"
#include "dev/sht11-sensor.h"

static uip_ip6addr_t rd_ipaddr;

static struct etimer sht;
static struct	stimer rdpost;
static struct stimer rdput;
static char * location;
static char loc[40];
static uint8_t registred = 0;

static int16_t rssi_value[3];
static int16_t rssi_count=0;
static int16_t rssi_position=0;
static int16_t rssi_avg=0;

static int16_t temperature=0;
static int16_t temperature_last=0;
static int16_t threshold = 50;
static uint8_t poll_time=5;

/*--------------------COAP Resources-----------------------------------------------------------*/
EVENT_RESOURCE(temperature, METHOD_GET, "sensors/temperature", "title=\"Temperature\";obs;rt=\"temperature\"");
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	snprintf((char*)buffer, preferred_size, "%d.%02d\n",temperature/100, temperature>0 ? temperature%100 : (-1*temperature)%100);
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, buffer, strlen((char*)buffer));

}

void temperature_event_handler(resource_t *r) {
	static uint32_t event_i = 0;
	char content[6];

	++event_i;

  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_CON, CONTENT_2_05, 0 );
  coap_set_payload(notification, content, snprintf(content, 6, "%d.%02d\n",temperature/100, temperature>0 ? temperature%100 : (-1*temperature)%100));

	REST.notify_subscribers(r, event_i, notification);

}



/*--------- Threshold ---------------------------------------------------------*/
RESOURCE(threshold, METHOD_GET | METHOD_PUT, "config/threshold", "title=\"Threshold\";rt=\"threshold\"");
void threshold_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf((char*)buffer, preferred_size,"%d.%02d", threshold/100, threshold%100);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else {
		const uint8_t * string = NULL;
    int success = 0;
    int len = coap_get_payload(request, &string);
		uint16_t value;
		if (len == 3) {
			if (isdigit(string[0]) && isdigit(string[2]) && string[1]=='.'){
				value = (atoi((char*) string)) * 100;
				value += atoi((char*) string+2)*10;
				success = 1;
			}
		}
	  if(success){
			threshold=value;
    	REST.set_response_status(response, REST.status.CHANGED);
    }
   	else{
   		REST.set_response_status(response, REST.status.BAD_REQUEST);
   	}
	}
}

/*------------------- HeartBeat --------------------------------------------------------------------------*/
PERIODIC_RESOURCE(heartbeat, METHOD_GET, "debug/heartbeat", "title=\"Heartbeat\";obs;rt=\"string\"",30*CLOCK_SECOND);
void heartbeat_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	snprintf((char*)buffer, preferred_size, "version:%s\nuptime:%lu\nrssi:%i",VERSION,clock_seconds(),rssi_avg);
 	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

void heartbeat_periodic_handler(resource_t *r){
	static uint32_t event_counter=0;
	static char	content[50];

	++event_counter;
	
	rssi_value[rssi_position]= radio_sensor.value(RADIO_SENSOR_LAST_PACKET);
	if(rssi_count<3){
		rssi_count++;
	}
	rssi_position++;
	rssi_position = (rssi_position) % 3;
	
	rssi_avg = (rssi_count>0)?(rssi_value[0]+rssi_value[1]+rssi_value[2])/rssi_count:0;
	coap_packet_t notification[1];
	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0);
	coap_set_payload(notification, content, snprintf(content, sizeof(content), "version:%s\nuptime:%lu\nrssi:%i",VERSION,clock_seconds(),rssi_avg));

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

PROCESS(tmote_temperature, "Tmote Sky Temperature Sensor");
AUTOSTART_PROCESSES(&tmote_temperature);

PROCESS_THREAD(tmote_temperature, ev, data)
{
	PROCESS_BEGIN();

	uip_ip6addr(&rd_ipaddr,0x2001,0x620,0x8,0x101f,0x0,0x0,0x0,0x1);

	/* if static routes are used rather than RPL */
	#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
	set_global_address();
	configure_routing();
	#endif

	/* Initialize the REST engine. */
	rest_init_engine();

	SENSORS_ACTIVATE(sht11_sensor);
	SENSORS_ACTIVATE(radio_sensor);

	rest_activate_event_resource(&resource_temperature);
	rest_activate_resource(&resource_threshold);
	rest_activate_periodic_resource(&periodic_resource_heartbeat);
	
	stimer_set(&rdpost, 60);
	etimer_set(&sht, 10*CLOCK_SECOND);

  /* Define application-specific events here. */
	while(1) {
		PROCESS_WAIT_EVENT();
		if (ev == PROCESS_EVENT_TIMER){
			if(etimer_expired(&sht)){
				temperature = -3960+sht11_sensor.value(SHT11_SENSOR_TEMP);
				if (temperature < temperature_last - threshold || temperature > temperature_last + threshold){
					temperature_last=temperature;
					temperature_event_handler(&resource_temperature);	
				}
				etimer_set(&sht, CLOCK_SECOND * poll_time);
			}
		}
		if (!registred && stimer_expired(&rdpost)) {
				static coap_packet_t post[1];
				coap_init_message(post,COAP_TYPE_CON, COAP_POST,0);

				coap_set_header_uri_path(post,"/rd");
				const char query[40];
				uint8_t addr[8];
		    rimeaddr_copy((rimeaddr_t *)&addr, &rimeaddr_node_addr);

				snprintf(query,39,"ep=\"%x-%x-%x-%x-%x-%x-%x-%x\"", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
				coap_set_header_uri_query(&post,query); 	

				COAP_BLOCKING_REQUEST(&rd_ipaddr, UIP_HTONS(5683) , post, rd_post_response_handler);
				stimer_set(&rdpost, 300);

		}
		if (registred && stimer_expired(&rdput)) {
				static coap_packet_t put[1];
				coap_init_message(put,COAP_TYPE_CON, COAP_PUT,0);

				coap_set_header_uri_path(put,loc);
				const char query[40];

				snprintf(query,39,"rt=\"%s\"",EPTYPE);
				coap_set_header_uri_query(&put,query); 	

				COAP_BLOCKING_REQUEST(&rd_ipaddr, UIP_HTONS(5683) , put, rd_put_response_handler);
				stimer_set(&rdput, 3600);
		}
		
		
	} /* while (1) */

	PROCESS_END();
}
