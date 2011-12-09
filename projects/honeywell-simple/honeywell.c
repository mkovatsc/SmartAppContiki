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
#include "rs232.h"
#include "ringbuf.h"
#include "sys/clock.h"

#if WITH_COAP == 3
#include "er-coap-03.h"
#include "er-coap-03-transactions.h"
#elif WITH_COAP == 6
#include "er-coap-06.h"
#include "er-coap-06-transactions.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#include "er-coap-07-transactions.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

//adds the debug resource that can be used to output the debug buffer
#define DEBUG 0

//sets the size of the request queue
#define REQUEST_QUEUE_SIZE 3

#define MAX(a,b) ((a)<(b)?(b):(a))
#define TRUE 1
#define FALSE 0

/*--PROCESSES----------------------------------------------------------------*/
PROCESS(honeywell_process, "Honeywell comm");
PROCESS(coap_process, "coap");

/*---------------------------------------------------------------------------*/
enum mode {manual=0, timers=1, valve=2};
enum request_type {
#if DEBUG
	debug,
#endif
	idle, 
	poll, 
	auto_temperatures, 
	auto_mode, 
	get_timer
};
typedef struct {
	uint8_t mode;
	uint16_t time;
} hw_timer_slot_t;

typedef struct {
	char command[15];
	enum request_type type;
} request;

static enum request_type request_state = idle;
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};

#if DEBUG
//debug buffer
static char debug_buffer[128];
#endif

static uint8_t poll_time = 90;

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
	uint8_t automode;

	clock_time_t last_poll;
} poll_data;


/*--REQUEST-QUEUE-IMPLEMENTATION---------------------------------------------*/
static request queue[REQUEST_QUEUE_SIZE];
static int queueTail = 0;
static int queueHead = -1;

static uint8_t queueEmpty(){
	return (queueHead == -1);
}

static void deQueue(){
	if(queueHead!=-1 && request_state==idle){
		printf(queue[queueHead].command);
		request_state=queue[queueHead].type;
		queueHead = (queueHead + 1) % REQUEST_QUEUE_SIZE;
		if(queueHead == queueTail){
			queueHead = -1;
		}
	}
}

static void enQueue(char * command, uint8_t rom, enum request_type type){
	if(queueEmpty() && request_state==idle){
		request_state = type;
		if(rom){
			printf_P(command);
		}
		else{
			printf(command);
		}
	}
	else{
		int newPos = (queueTail + 1) % REQUEST_QUEUE_SIZE;
		if(newPos == queueHead){
			request_state = idle;
			//queue is full deQueue a request
			deQueue();
		}
		queue[newPos].type = type;
		if(rom){
			strncpy_P(queue[newPos].command, command, 12);
		}
		else{
			strncpy(queue[newPos].command, command, 12);
		}
		queueTail = newPos;
		if(queueHead==-1){
			queueHead = newPos;
		}
	}
}


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

//fill in the cache
static void parseD(char * data){
	//D: d5 01.01.10 14:20:07 A V: 30 I: 2287 S: 1700 B: 2707 Is: 00000000 Ib: 00 Ic: 28 Ie: 17 X
	if(data[0]=='D'){
		poll_data.valve = atoi(&data[29]);
		uint16_t is_temperature = atoi(&data[35]);
		if(poll_data.is_temperature != is_temperature){
			//send event to the coap process that the temperature changed to notify all the subscribers
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
	
	printf("TESTING");

        // target temperature mode
        enQueue(PSTR("M00\n"), TRUE, poll);
        // 22 degC
        enQueue("A16\n", FALSE, poll);

	etimer_set(&etimer, CLOCK_SECOND * poll_time);
	
	request_state = idle;

	while (1) {
		PROCESS_WAIT_EVENT();
		if(ev == PROCESS_EVENT_TIMER) {
			etimer_set(&etimer, CLOCK_SECOND * poll_time);
			enQueue(PSTR("D\n"), TRUE,poll);
		} else if (ev == PROCESS_EVENT_MSG) {
			buf_pos = 0;
			while ((rx=ringbuf_get(&uart_buf))!=-1) {
				if (buf_pos<126 && (char)rx=='\n') {
					buf[buf_pos++] = '\n';
					buf[buf_pos] = '\0';
					//switch statement for the different request states
					switch(request_state){
						case idle:
							break;
						case poll:
							parseD(buf);
							break;
#if DEBUG
						case debug:
							memcpy(debug_buffer, buf, strlen(buf));
							break;
#endif
						case auto_temperatures:
							if(strncmp_P(buf, PSTR("G[0"), 3) == 0 || strncmp_P(buf, PSTR("S[0"), 3) == 0){
								int index = atoi(&buf[3]);
								int temp;
								sscanf_P(&buf[6], PSTR("%x"), &temp);
								temp *= 50;
								switch(index){
									case 1:
										poll_data.frost_temperature = temp;
										break;
									case 2:
										poll_data.energy_temperature = temp;
										break;
									case 3:
										poll_data.comfort_temperature = temp;
										break;
									case 4:
										poll_data.supercomfort_temperature = temp;
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
							}
							break;
					}
					//we are done so we set back the request state do idle
					request_state = idle;
					//de queue another job if there is one in the queue
					deQueue();
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
EVENT_RESOURCE(temperature, METHOD_GET, "sensors/temp", "title=\"Current temperature\";ct=0;rt=\"temperature:C\"");
void temperature_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
        snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), poll_data.is_temperature/100, poll_data.is_temperature%100);

        enQueue(PSTR("D\n"), TRUE, poll);

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

RESOURCE(battery, METHOD_GET, "sensors/battery", "title=\"Battery voltage\";ct=0;rt=\"voltage:mV\"");
void battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
        snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_data.battery);

        enQueue(PSTR("D\n"), TRUE, poll);

        REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
        REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


RESOURCE(target, METHOD_GET | METHOD_PUT, "set/target", "title=\"Target temperature\";ct=0;rt=\"temperature:C\"");
void target_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  if (REST.get_method_type(request)==METHOD_GET)
  {
    snprintf_P((char*)buffer, preferred_size, PSTR("%d.%02d"), poll_data.target_temperature/100, poll_data.target_temperature%100);
    enQueue(PSTR("D\n"), TRUE, poll);
  }
  else
  {
    //TODO tt.t format

          const uint8_t * string = NULL;
          int success = 1;
          int len = coap_get_payload(request, &string);
          if(len != 3 && len != 2){
                  success = 0;
          }
          else{
                  int i;
                  for(i=0; i<len; i++){
                          if (!isdigit(string[i])){
                                  success = 0;
                                  break;
                          }
                  }
          }
          if(!success){
                  REST.set_response_status(response, REST.status.BAD_REQUEST);
                  strncpy_P((char*)buffer, PSTR("Payload format: ttt, e.g. 155 sets the temperature to 15.5 deg"), preferred_size);
          }
          else{
                  uint16_t value = atoi((char*)string);
                  char buf[10];
                  snprintf_P(buf, 8, PSTR("A%02x\n"),value/5);

                  // target mode
                  enQueue(PSTR("M00\n"), TRUE, poll);

                  enQueue(buf, FALSE, poll);
                  strncpy_P((char*)buffer, PSTR("Success"), preferred_size);
          }
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(valve, METHOD_GET | METHOD_PUT, "set/valve", "title=\"Valve opening\";ct=0;rt=\"state:percent\"");
void valve_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  if (REST.get_method_type(request)==METHOD_GET){
          snprintf_P((char*)buffer, preferred_size, PSTR("%d"), (poll_data.valve-30)*2);
          enQueue(PSTR("D\n"), TRUE, poll);
  }
  else
  {
          const uint8_t * string = NULL;
          int success = 1;
          int len = coap_get_payload(request, &string);
          int new_valve = 0;

          if (len > 3)
          {
            success = 0;
          }
          else
          {
            int i;
            for (i=0; i<len; ++i)
            {
              if (!isdigit(string[i]))
              {
                success = 0;
                break;
              }
            }
            if (success)
            {
              new_valve = atoi((char*)string);
              if (new_valve > 100)
              {
                success = 0;
              }
              else
              {
                // internal valve values: 30-80
                new_valve = (++new_valve)/2 + 30;
              }
            }
          }

          if (!success)
          {
            REST.set_response_status(response, REST.status.BAD_REQUEST);
            strncpy_P((char*)buffer, PSTR("Payload format: aa, e.g. 47 sets the valve to 47 percent"), preferred_size);
          }
          else
          {
            char buf[12];
            snprintf_P(buf, 10, PSTR("E%02x\n"), new_valve);

            // valve mode
            enQueue(PSTR("M02\n"), TRUE, poll);

            enQueue(buf, FALSE, poll);

            strncpy_P((char*)buffer, PSTR("Success"), preferred_size);
          }
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


RESOURCE(mode, METHOD_GET, "config/mode", "title=\"Control state (read-only)\";ct=0;rt=\"state:finite\"");
void mode_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  enQueue(PSTR("D\n"), 1, poll);

  if (poll_data.mode==manual)
  {
    strncpy_P((char*)buffer, PSTR("manual"), preferred_size);
  }
  else if (poll_data.mode==timers)
  {
    strncpy_P((char*)buffer, PSTR("auto"), preferred_size);
  }
  else if (poll_data.mode==valve)
  {
    strncpy_P((char*)buffer, PSTR("valve"), preferred_size);
  }
  else
  {
    strncpy_P((char*)buffer, PSTR("undefined"), preferred_size);
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(poll, METHOD_GET | METHOD_PUT, "config/poll", "title=\"Polling interval\";ct=0;rt=\"time:s\"");
void poll_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  if (REST.get_method_type(request)==METHOD_GET)
  {
    snprintf_P((char*)buffer, preferred_size, PSTR("%d"), poll_time);
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
                    int poll_intervall = atoi((char*)string);
                    if(poll_intervall < 255 && poll_intervall > 0){
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
            strncpy_P((char*)buffer, PSTR("Payload format: aa, e.g. 15 sets the poll interval to 15 seconds"), preferred_size);
    }
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, buffer, strlen((char*)buffer));
}

RESOURCE(date, METHOD_GET | METHOD_PUT, "config/date", "title=\"Thermostat date\";ct=0;rt=\"datetime:dd.mm.yy\"");
void date_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
        if (REST.get_method_type(request)==METHOD_GET){
                snprintf_P((char*)buffer, preferred_size, PSTR("%02d.%02d.%02d"), poll_data.day, poll_data.month, poll_data.year);
                enQueue(PSTR("D\n"), TRUE, poll);
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
                                char buf[12];
                                snprintf_P(buf, 10, PSTR("Y%02x%02x%02x\n"),year,month,day);
                                enQueue(buf, FALSE, poll);

                                strncpy_P((char*)buffer, PSTR("Successfully set date"), preferred_size);
                        }
                }
                else{
                        success = 0;
                }
                if(!success){
                        REST.set_response_status(response, REST.status.BAD_REQUEST);
                        strncpy_P((char*)buffer, PSTR("Payload format: dd.mm.yy"), preferred_size);
                }
        }

        REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
        REST.set_response_payload(response, buffer, strlen((char*)buffer));
}


RESOURCE(time, METHOD_GET | METHOD_PUT, "config/time", "title=\"Thermostat time\";ct=0;rt=\"datetime:hh:mm:ss\"");
void time_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
        if (REST.get_method_type(request)==METHOD_GET){
                clock_time_t now = clock_time();
                int second = poll_data.second + (now - poll_data.last_poll) / CLOCK_SECOND;
                int minute = poll_data.minute + (second / 60);
                int hour = poll_data.hour + (minute / 60);
                snprintf_P((char*)buffer, preferred_size, PSTR("%02d:%02d:%02d"), hour % 24, minute % 60, second % 60 );
                enQueue(PSTR("D\n"), TRUE, poll);
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
                                char buf[12];
                                snprintf_P(buf, 10, PSTR("H%02x%02x%02x\n"),hour,minute,second);
                                enQueue(buf, FALSE, poll);
                                strncpy_P((char*)buffer, PSTR("Successfully set time"), preferred_size);
                        }
                }
                else{
                        success = 0;
                }
                if(!success){
                        REST.set_response_status(response, REST.status.BAD_REQUEST);
                        strncpy_P((char*)buffer, PSTR("Payload format: hh:mm[:ss]"), preferred_size);
                }
        }

        REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
        REST.set_response_payload(response, buffer, strlen((char*)buffer));
}



#if DEBUG
RESOURCE(debug, METHOD_GET | METHOD_PUT, "debug", "debug");
void debug_handler(void * request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
        if (REST.get_method_type(request)==METHOD_PUT){
                const char * string = NULL;
                REST.get_post_variable(request, "value", &string);
                enQueue((char*)string, 0, debug);
        }

        REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
        REST.set_response_payload(response, (uint8_t*)debug_buffer, strlen(debug_buffer));
}
#endif


PROCESS_THREAD(coap_process, ev, data)
{
  PROCESS_BEGIN();

  rest_init_framework();

  //activate the resources
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

  //call the temperature handler if the temperature changed
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
