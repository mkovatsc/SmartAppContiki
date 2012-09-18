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


//adds the debug resource that can be used to output the debug buffer
#define DEBUG 1

//sets the size of the request queue
#define REQUEST_QUEUE_SIZE 3

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0

#define VERSION "0.7.1"


/*--PROCESSES----------------------------------------------------------------*/
PROCESS(rfid_process, "RFID_uart");
PROCESS(coap_process, "coap");

/*---------------------------------------------------------------------------*/
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};

static uint32_t rfid_tag;
static process_event_t rfid_event;

char ee_identifier[50] EEMEM;
char identifier[50];


/*-----RFID_UART-PROCESS-IMPLEMENTATION-----------------------------------------*/
static int uart_get_char(unsigned char c)
{
	ringbuf_put(&uart_buf, c);
	if (c==0x03 || ringbuf_size(&uart_buf)==127) {
		process_post(&rfid_process, PROCESS_EVENT_MSG, NULL);
	}
	return 1;
}


PROCESS_THREAD(rfid_process, ev, data)
{
	PROCESS_BEGIN();
	
	int rx;
	int buf_pos;
	char buf[128];

	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);

	rfid_event = process_alloc_event();
	// finish booting first
	PROCESS_PAUSE();

	while (1) {
		PROCESS_WAIT_EVENT();
		if (ev == PROCESS_EVENT_MSG) {
			buf_pos = 0;
			while ((rx=ringbuf_get(&uart_buf))!=-1) {
				printf("%c",rx & 0x7f);
				if ((char) rx==0x02){
					buf_pos=0;
				}
				buf[buf_pos++] = (char)rx & 0x7f;

				if ((char)rx==0x03) {
					buf[buf_pos]='\0';
					unsigned int tag[10];
					unsigned int check[2];
					//printf("\n");
					//printf("%i\n",buf_pos);
					int i;
					for(i=1; i<11;i++){
						sscanf(buf+i,"%1X",&tag[i-1]);
					//	printf("%u\n",tag[i-1]);
					}
					sscanf(buf+11,"%1X",&check[0]);
					sscanf(buf+12,"%1X",&check[1]);
					if (((tag[0] ^ tag[2] ^ tag[4] ^ tag[6] ^ tag[8]) == check[0]) //CHECKSUM
					   && ((tag[1] ^ tag[3] ^ tag[5] ^ tag[7] ^ tag[9]) == check[1])){
					
						uint32_t serial=0;
						int i;
						for (i=4; i<10;i++){
							serial = 16*serial+(uint32_t) tag[i];
						}
						rfid_tag = serial;
						process_post(&coap_process,rfid_event,NULL);
					}
					
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




/***************EVENT RESSOURCES*****************/

EVENT_RESOURCE(rfid, METHOD_GET, "sensors/rfid", "title=\"RFID Reader\";obs");

void
rfid_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  /* Usually, a CoAP server would response with the current resource representation. */
  const char *msg = "";
  REST.set_response_payload(response, (uint8_t *)msg, strlen(msg));

  /* A post_handler that handles subscriptions/observing will be called for periodic resources by the framework. */
}

/* Additionally, a handler function named [resource name]_event_handler must be implemented for each PERIODIC_RESOURCE defined.
 * It will be called by the REST manager process with the defined period. */
void
rfid_event_handler(resource_t *r)
{
	static uint32_t event_i = 0;
	static char content[11];

	++event_i;

	coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  	coap_set_payload(notification, content, snprintf_P(content, 11, PSTR("%lu"),rfid_tag));

	REST.notify_subscribers(r, event_i, notification);
}


/*--------- Node Identifier ------------------------------------------------------------*/
RESOURCE(identifier, METHOD_GET | METHOD_PUT, "config/identifier", "title=\"Identifer\";ct=0;rt=\"string\"");
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
RESOURCE(version, METHOD_GET | METHOD_PUT, "debug/version", "title=\"Version Number\";ct=0;rt=\"number\"");
void version_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

	snprintf_P((char*)buffer, preferred_size, PSTR("%s"), VERSION);
 	REST.set_response_payload(response, buffer, strlen((char*)buffer));
}





PROCESS_THREAD(coap_process, ev, data)
{

	PROCESS_BEGIN();
  
	rest_init_engine();


	eeprom_read_block(&identifier, ee_identifier, 50);
	rest_activate_event_resource(&resource_rfid);
	rest_activate_resource(&resource_identifier);
	rest_activate_resource(&resource_version);

	while(1) {
		PROCESS_WAIT_EVENT();
	
		if (ev == rfid_event) {
			rfid_event_handler(&resource_rfid);

		}	
	}

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&rfid_process, &coap_process);
/*---------------------------------------------------------------------------*/
