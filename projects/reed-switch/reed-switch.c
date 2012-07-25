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
#include <avr/io.h>
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"

#include "net/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "er-coap-07.h"
#include "er-coap-07-transactions.h"

#include "dev/reed-sensor.h"


//adds the debug resource that can be used to output the debug buffer
#define DEBUG 1

//sets the size of the request queue
#define REQUEST_QUEUE_SIZE 3

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0


extern uip_ds6_nbr_t uip_ds6_nbr_cache[];
extern uip_ds6_route_t uip_ds6_routing_table[];

/*--PROCESSES----------------------------------------------------------------*/
PROCESS(rfnode_test_process, "rfNode_test");
PROCESS(coap_process, "coap");

SENSORS(&reed_sensor);

/*---------------------------------------------------------------------------*/
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};


/*--DERFNODE-PROCESS-IMPLEMENTATION-----------------------------------------*/
static int uart_get_char(unsigned char c)
{
	ringbuf_put(&uart_buf, c);
	if (c=='\n' || ringbuf_size(&uart_buf)==127) {
		ringbuf_put(&uart_buf, '\0');
		process_post(&rfnode_test_process, PROCESS_EVENT_MSG, NULL);
	}
	return 1;
}

PROCESS_THREAD(rfnode_test_process, ev, data)
{
	PROCESS_BEGIN();
	
	int rx;
	int buf_pos;
	char buf[128];

	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);
	// finish booting first
	PROCESS_PAUSE();
	
	while (1) {
		PROCESS_WAIT_EVENT();
		if (ev == PROCESS_EVENT_MSG) {
			buf_pos = 0;
			while ((rx=ringbuf_get(&uart_buf))!=-1) {
				if (buf_pos<126 && (char)rx=='\n') {
					buf[buf_pos++] = '\n';
					buf[buf_pos] = '\0';
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



/*--SIMPLE RESOURCES---------------------------------------------------------*/

EVENT_RESOURCE(reed, METHOD_GET, "sensors/reed-switch", "title=\"Reed Switch\";ct=0;rt=\"state:finite\"");

void
reed_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	int state = reed_sensor.value(0);
	const char *msg = state ? "closed" : "open";
	printf("%s\n",msg);
	REST.set_response_payload(response, (uint8_t *)msg, strlen(msg));

}

void
event_reed_handler(resource_t *r)
{
	static uint32_t event_i = 0;
	static char content[10];
	

	++event_i;
	int state = reed_sensor.value(0);


	coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
	coap_init_message(notification, COAP_TYPE_CON, CONTENT_2_05, 0 );
	coap_set_payload(notification, content, snprintf(content, sizeof(content), "%s", state ? "closed" : "open"));

	REST.notify_subscribers(r, event_i, notification);
}





PROCESS_THREAD(coap_process, ev, data)
{
	PROCESS_BEGIN();
  
 	static struct etimer etimer;
	rest_init_engine();
	SENSORS_ACTIVATE(reed_sensor);
	printf("Sensors activated\n");
	  
	rest_activate_event_resource(&resource_reed);

	etimer_set(&etimer, CLOCK_SECOND * 5);
	while(1) {
		PROCESS_WAIT_EVENT();
		if (ev == sensors_event && data == &reed_sensor) {
			event_reed_handler(&resource_reed);
		}
	/* 	else if (ev == PROCESS_EVENT_TIMER){
			int state = reed_sensor.value(0);
			const char *msg = state ? "normal" : "tilted";
			printf("%s\n",msg);
  			etimer_set(&etimer, CLOCK_SECOND * 5);
		}
	*/	
    
	}

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&rfnode_test_process, &coap_process, &sensors_process);
/*---------------------------------------------------------------------------*/
