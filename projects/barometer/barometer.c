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
#include <util/delay.h>
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"


#include "net/uip-ds6.h"
#include "net/rpl/rpl.h"

#include "er-coap-07.h"
#include "er-coap-07-transactions.h"
#include "er-coap-07-separate.h"

#include "dev/interrupttwi.h"

//adds the debug resource that can be used to output the debug buffer
#define DEBUG 1

//sets the size of the request queue
#define REQUEST_QUEUE_SIZE 3

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0

#define BAROMETER_ADDRESS 0x77
#define OSS 1

#define VERSION "0.7.1"


/*--PROCESSES----------------------------------------------------------------*/
PROCESS(barometer_process, "barometer");
PROCESS(coap_process, "coap");

/*---------------------------------------------------------------------------*/
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};

static int16_t ac1, ac2, ac3;
static uint16_t ac4, ac5, ac6;
static int16_t b1, b2;
static int16_t mb, mc, md;

static int32_t temperature;
static int32_t temperature_last = 0;
static int32_t temperature_threshold = 5;

static int32_t pressure;
static int32_t pressure_last;
static int32_t pressure_threshold = 100;

static unsigned short poll_interval = 3;

char ee_identifier[50] EEMEM;
char identifier[50];


clock_time_t last_reading;

static process_event_t get_temperature_response_event;
static process_event_t changed_temperature_event;

static process_event_t get_pressure_response_event;
static process_event_t changed_pressure_event;

typedef struct application_separate_get_temperature_store {
	coap_separate_t request_metadata;
} application_separate_get_temperature_store_t;

static uint8_t separate_get_temperature_active = 0;
static application_separate_get_temperature_store_t separate_get_temperature_store[1];


typedef struct application_separate_get_pressure_store {
	coap_separate_t request_metadata;
} application_separate_get_pressure_store_t;

static uint8_t separate_get_pressure_active = 0;
static application_separate_get_pressure_store_t separate_get_pressure_store[1];


/*--BAROMETER-PROCESS-IMPLEMENTATION-----------------------------------------*/
static int uart_get_char(unsigned char c)
{
	ringbuf_put(&uart_buf, c);
	if (c=='\n' || ringbuf_size(&uart_buf)==127) {
		ringbuf_put(&uart_buf, '\0');
		process_post(&barometer_process, PROCESS_EVENT_MSG, NULL);
	}
	return 1;
}


static unsigned char twi_read_char(uint8_t address){
	TWI_buffer_out[0]=address;
	TWI_master_start_write_then_read(BAROMETER_ADDRESS,1,1);
	while(TWI_busy);
	return ((unsigned char) TWI_buffer_in[0]);
}

static unsigned int twi_read_int(uint8_t address){
	TWI_buffer_out[0]=address;
	TWI_master_start_write_then_read(BAROMETER_ADDRESS,1,2);
	while(TWI_busy);
	return ((unsigned int) (TWI_buffer_in[0]<<8) + TWI_buffer_in[1]);
}

static void get_barometer_parameters(){

	ac1 = twi_read_int(0xaa);
	ac2 = twi_read_int(0xac);
	ac3 = twi_read_int(0xae);
	ac4 = twi_read_int(0xb0);
	ac5 = twi_read_int(0xb2);
	ac6 = twi_read_int(0xb4);
	b1 = twi_read_int(0xb6);
	b2 = twi_read_int(0xb8);
	mb = twi_read_int(0xba);
	mc = twi_read_int(0xbc);
	md = twi_read_int(0xbe);


	printf("ac1: %i\n", ac1);
	printf("ac2: %i\n", ac2);
	printf("ac3: %i\n", ac3);
	printf("ac4: %u\n", ac4);
	printf("ac5: %u\n", ac5);
	printf("ac6: %u\n", ac6);
	printf("b1: %i\n", b1);
	printf("b2: %i\n", b2);
	printf("mb: %i\n", mb);
	printf("mc: %i\n", mc);
	printf("md: %i\n", md);



}


static void do_reading(){
	uint8_t msb, lsb, xlsb;
	int32_t up, ut;
	TWI_buffer_out[0]= 0xf4;
	TWI_buffer_out[1]= 0x2e;
	TWI_master_start_write(BAROMETER_ADDRESS,2);
	while(TWI_busy);

	// Wait for some microseconds
	asm volatile (
	    "    ldi  r18, 73"	"\n"
	    "    ldi  r19, 185"	"\n"
	    "1:  dec  r19"	"\n"
	    "    brne 1b"	"\n"
	    "    dec  r18"	"\n"
	    "    brne 1b"	"\n"
	    "    rjmp 2f"	"\n"
	    "2:"	"\n"
	);

	msb = twi_read_char(0xf6);
	lsb = twi_read_char(0xf7);
	
	ut = (((int32_t) msb) << 8) + (int32_t) lsb;
//	printf("ut: %li\n",ut);
	
	TWI_buffer_out[0]= 0xf4;
	TWI_buffer_out[1]= 0x34 + (OSS<<6);
	TWI_master_start_write(BAROMETER_ADDRESS,2);
	while(TWI_busy);
	
	// Wait for some microseconds
	asm volatile (
	    "    ldi  r18, 2"	"\n"
	    "    ldi  r19, 56"	"\n"
	    "    ldi  r20, 174"	"\n"
	    "1:  dec  r20"	"\n"
	    "    brne 1b"	"\n"
	    "    dec  r19"	"\n"
	    "    brne 1b"	"\n"
	    "    dec  r18"	"\n"
	    "    brne 1b"	"\n"
	);

	msb = twi_read_char(0xf6);
	lsb = twi_read_char(0xf7);
	xlsb = twi_read_char(0xf8);

	up = ((((int32_t) msb) << 16) + (((int32_t) lsb) << 8) + ((int32_t) xlsb) )>> (8-OSS);
//	printf("up: %li\n",up);

/*	Values from Datasheet for Testing
	ac1= 408;
	ac2= -72;
	ac3= -14383;
	ac4= 32741;
	ac5= 32757;
	ac6= 23153;
	b1= 6190;
	b2= 4;
	mb= -32768;
	mc= -8711;
	md= 2868;
	ut= 27898;
	up= 23843;
*/

	int32_t x1, x2, x3, b3, b5, b6, p;
	uint32_t b4, b7;
	x1 = (((int32_t) ut - (int32_t)ac6) * (int32_t)ac5)  >> 15;
	x2 = ((int32_t) mc << 11) / (x1 +md);
	b5 = x1 + x2;
	
	int32_t temp = ((b5 + 8) >> 4);
	printf("temp: %li\n",temp);

	b6 = b5 - 4000;
	x1 = (((int16_t) b2 * (b6 * b6)) >> 12) >> 11;
	x2 = ((int16_t) ac2 *b6) >> 11;
	x3 = x1 + x2;
	b3 = (((((int16_t) ac1) *4 + x3) << OSS) + 2) >> 2;

	x1 = ((long) ac3 * b6) >> 13;
	x2 = ((long) b1 * ((b6 * b6) >> 12)) >> 16;
	x3 = ((x1 + x2) + 2) >> 2;
	b4 = (ac4 * (unsigned long) (x3 + 32768)) >> 15;
	b7 = (unsigned long) ((up -b3) * (50000 >> OSS));
	if (b7 < 0x80000000) {
		p = (b7 *2) / b4;
	}
	else {
		p = (b7 / b4) * 2;
	}
	x1 = (p >> 8) * (p >> 8);
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	p += (x1 + x2 + 3791) >> 4;

	printf("pressure: %li\n",p);

	last_reading = clock_time();
	temperature = temp;
	if (temp > temperature_last + temperature_threshold || temp < temperature_last - temperature_threshold){
		temperature_last = temp;
		process_post(&coap_process, changed_temperature_event, NULL);
	}
	process_post(&coap_process, get_temperature_response_event, NULL);

	pressure = p;
	if (p > pressure_last + pressure_threshold || p < pressure_last - pressure_threshold){
		pressure_last = p;
		process_post(&coap_process, changed_pressure_event, NULL);
	}
	process_post(&coap_process, get_pressure_response_event, NULL);

}

PROCESS_THREAD(barometer_process, ev, data)
{
	PROCESS_BEGIN();
	
	int rx;
	int buf_pos;
	char buf[128];
	static struct etimer etimer;

	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);

	get_temperature_response_event = process_alloc_event();
	changed_temperature_event = process_alloc_event();
	get_pressure_response_event = process_alloc_event();
	changed_pressure_event = process_alloc_event();


	DDRD |= (1<<PD6);
	PORTD &= ~(1<<PD6);
	PROCESS_PAUSE();
	TWI_init(100000UL);
	printf("init\n");

	// finish booting first
	PROCESS_PAUSE();
	get_barometer_parameters();


	etimer_set(&etimer, CLOCK_SECOND * 20);

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
		}
		else if (ev == PROCESS_EVENT_TIMER) {
			do_reading();
			etimer_set(&etimer, CLOCK_SECOND * poll_interval);
		}
		// events

	}
	PROCESS_END();
}

/*------------------------------- COAP RESSOURCES --------------------------------------------*/

/*--------- Poll Inteval ------------------------------------------------------------*/

RESOURCE(poll, METHOD_GET | METHOD_PUT, "config/poll", "title=\"Polling interval\";ct=0;rt=\"time:s\"");
void poll_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  if (REST.get_method_type(request)==METHOD_GET)
  {
    snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_interval);
  }
  else
  {
    const uint8_t * string = NULL;
    int success = 1;
    int len = coap_get_payload(request, &string);
    if(len == 0){
            success = 0;
    }
    else
      {
            int i;
            for(i=0; i<len; i++){
                    if (!isdigit(string[i])){
                            success = 0;
                            break;
                    }
            }
            if(success){
                    int poll_new = atoi((char*)string);
                    if(poll_new < 255 && poll_new > 0){
                            poll_interval = poll_new;
                            strncpy_P((char*)buffer, PSTR("Successfully set poll interval"), preferred_size);
                    }
                    else{
                            success = 0;
                    }
            }
    }
    if(!success){
            REST.set_response_status(response, REST.status.BAD_REQUEST);
            strncpy_P((char*)buffer, PSTR("Payload format: aa, e.g. 15 sets the poll interval to 15 seconds"), preferred_size);
    }
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

/*--------------------------------------- Temperature -----------------------------------*/
EVENT_RESOURCE(temperature, METHOD_GET, "sensors/temperature", "title=\"Current temperature\";ct=0;rt=\"temperature:C\"");
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
        if(last_reading > clock_time()-5*CLOCK_SECOND) {

		snprintf_P((char*)buffer, preferred_size, PSTR("%ld.%01ld"), temperature/10, temperature%10);
        	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else {
		if (!separate_get_temperature_active) {
			coap_separate_accept(request, &separate_get_temperature_store->request_metadata);
			separate_get_temperature_active = 1;
			do_reading();
		}
		else {
			coap_separate_reject();
		}
	}
}

void temperature_event_handler(resource_t *r) {
        static uint32_t event_i = 0;
        char content[6];

        ++event_i;

  	coap_packet_t notification[1]; // This way the packet can be treated as pointer as usual. 
  	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  	coap_set_payload(notification, content, snprintf_P(content, 6, PSTR("%ld.%01ld"), temperature/10, temperature%10));

 	REST.notify_subscribers(r, event_i, notification);
}


void temperature_finalize_handler() {
	if (separate_get_temperature_active){
		char buffer[10];
		coap_transaction_t *transaction = NULL;
		if( (transaction = coap_new_transaction(separate_get_temperature_store->request_metadata.mid, &separate_get_temperature_store->request_metadata.addr, separate_get_temperature_store->request_metadata.port))) {
			coap_packet_t response[1]; // This way the packet can be treated as pointer as usual.
			coap_separate_resume(response, &separate_get_temperature_store->request_metadata, CONTENT_2_05);
			snprintf_P(buffer, 10 , PSTR("%ld.%01ld"), temperature/10, temperature%10);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			coap_set_payload(response, buffer, strlen(buffer));
			coap_set_header_block2(response, separate_get_temperature_store->request_metadata.block2_num, 0, separate_get_temperature_store->request_metadata.block2_size);
			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_temperature_active = 0;
		}
		else {
			separate_get_temperature_active = 0;
      		
		       
		}
	}
}


/*---------------------------------Temperature Threshold --------------------------------------*/
RESOURCE(temperature_threshold, METHOD_GET | METHOD_PUT, "config/temperature_threshold", "title=\"Threshold temperature\";ct=0;rt=\"temperature:C\"");
void temperature_threshold_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%ld.%01ld"), temperature_threshold/10, temperature_threshold%10);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
  	else {
        	const uint8_t * string = NULL;
        	int success = 1;
        	int len = coap_get_payload(request, &string);
		
		int32_t value = 0;
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
		if (value < 0){
			success = 0;
		}
	        if(success){
        		REST.set_response_status(response, REST.status.CHANGED);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			temperature_threshold = value;
	
	        }
        	else{
        		REST.set_response_status(response, REST.status.BAD_REQUEST);
                	strncpy_P((char*)buffer, PSTR("Payload format: tt.t, e.g. 1.0 sets the threshold to 1.0 deg"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
        	}
	}
}



/*---------------------------------- Pressure ------------------------------------------------*/
EVENT_RESOURCE(pressure, METHOD_GET, "sensors/pressure", "title=\"Current pressure\";ct=0;rt=\"pressure:Pa\"");
void pressure_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
        if(last_reading > clock_time()-5*CLOCK_SECOND) {

		snprintf_P((char*)buffer, preferred_size, PSTR("%ld"), pressure);
        	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
	else {
		if (!separate_get_pressure_active) {
			coap_separate_accept(request, &separate_get_pressure_store->request_metadata);
			separate_get_pressure_active = 1;
			do_reading();
		}
		else {
			coap_separate_reject();
		}
	}
}

void pressure_event_handler(resource_t *r) {
        static uint32_t event_i = 0;
        char content[20];

        ++event_i;

  	coap_packet_t notification[1]; // This way the packet can be treated as pointer as usual. 
  	coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );
  	coap_set_payload(notification, content, snprintf_P(content, 6, PSTR("%ld"), pressure));

 	REST.notify_subscribers(r, event_i, notification);
}


void pressure_finalize_handler() {
	if (separate_get_pressure_active){
		char buffer[20];
		coap_transaction_t *transaction = NULL;
		if( (transaction = coap_new_transaction(separate_get_pressure_store->request_metadata.mid, &separate_get_pressure_store->request_metadata.addr, separate_get_pressure_store->request_metadata.port))) {
			coap_packet_t response[1]; // This way the packet can be treated as pointer as usual.
			coap_separate_resume(response, &separate_get_pressure_store->request_metadata, CONTENT_2_05);
			snprintf_P(buffer, 10 , PSTR("%ld"), pressure);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			coap_set_payload(response, buffer, strlen(buffer));
			coap_set_header_block2(response, separate_get_pressure_store->request_metadata.block2_num, 0, separate_get_pressure_store->request_metadata.block2_size);
			transaction->packet_len = coap_serialize_message(response, transaction->packet);
			coap_send_transaction(transaction);
			separate_get_pressure_active = 0;
		}
		else {
			separate_get_pressure_active = 0;
      		
		       
		}
	}
}


/*--------------------------------- Pressure Threshold --------------------------------------*/
RESOURCE(pressure_threshold, METHOD_GET | METHOD_PUT, "config/pressure_threshold", "title=\"Threshold pressure\";ct=0;rt=\"pressure:Pa\"");
void pressure_threshold_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	if (REST.get_method_type(request)==METHOD_GET)
	{
		snprintf_P((char*)buffer, preferred_size, PSTR("%ld"), pressure_threshold);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, strlen((char*)buffer));
	}
  	else {
        	const uint8_t * string = NULL;
        	int success = 1;
        	int len = coap_get_payload(request, &string);
		
		int32_t value = 0;
		int i;
		for (i=0;i<len;i++){
			success = success && isdigit(string[i]);
		}
		value = atoi((char*) string);
			
		if (value < 0){
			success = 0;
		}
	        if(success){
        		REST.set_response_status(response, REST.status.CHANGED);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			pressure_threshold = value;
	
	        }
        	else{
        		REST.set_response_status(response, REST.status.BAD_REQUEST);
                	strncpy_P((char*)buffer, PSTR("Payload format: ppp, e.g. 50 sets the threshold to 50 Pa"), preferred_size);
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, strlen((char*)buffer));
        	}
	}
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


	//activate the resources
	rest_activate_resource(&resource_poll);
	rest_activate_resource(&resource_temperature_threshold);
	rest_activate_resource(&resource_pressure_threshold);
	rest_activate_event_resource(&resource_temperature);
	rest_activate_event_resource(&resource_pressure);
	rest_activate_resource(&resource_identifier);
	rest_activate_resource(&resource_version);





	while(1) {
		PROCESS_WAIT_EVENT();
		if (ev == changed_temperature_event){
			temperature_event_handler(&resource_temperature);
		}
		else if (ev == get_temperature_response_event){
			temperature_finalize_handler();
		}
		else if (ev == changed_pressure_event){
			pressure_event_handler(&resource_pressure);
		}
		else if (ev == get_pressure_response_event){
			pressure_finalize_handler();
		}
	}

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&barometer_process, &coap_process);
/*---------------------------------------------------------------------------*/
