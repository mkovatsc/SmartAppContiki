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
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "contiki-raven.h"
#include "rs232.h"
#include "ringbuf.h"
#include "rest.h"
#include "sys/clock.h"


/*---------------------------------------------------------------------------*/
PROCESS(honeywell_process, "Honeywell comm");
PROCESS(telnetd_process, "Telnet server");

/*---------------------------------------------------------------------------*/
// request
#define AT_COMMAND_PREFIX "AT"
// command for info
#define AT_COMMAND_INFO AT_COMMAND_PREFIX"I"
// command for reset
#define AT_COMMAND_RESET AT_COMMAND_PREFIX"Z"
// command for hangup
#define AT_COMMAND_HANGUP AT_COMMAND_PREFIX"H"
// command for unicast: AT+UCAST:<16Byte src addr>,<response>\r\n
#define AT_COMMAND_UNICAST AT_COMMAND_PREFIX"+UCAST:"
// response when everything is ok
#define AT_RESPONSE_OK "OK"
// response when an error occured
#define AT_RESPONSE_ERROR "ERROR"



#define ISO_nl       0x0a
#define ISO_cr       0x0d

#ifndef TELNETD_CONF_LINELEN
#define TELNETD_CONF_LINELEN 110
#endif
#ifndef TELNETD_CONF_NUMLINES
#define TELNETD_CONF_NUMLINES 1
#endif

struct telnetd_state {
	char buf[TELNETD_CONF_LINELEN + 1];
	char bufptr;
	char continued;
	uint16_t numsent;
	uint8_t state;
#define STATE_NORMAL 0
#define STATE_IAC    1
#define STATE_WILL   2
#define STATE_WONT   3
#define STATE_DO     4
#define STATE_DONT   5

#define STATE_CLOSE  6
};

#define TELNET_IAC   255
#define TELNET_WILL  251
#define TELNET_WONT  252
#define TELNET_DO    253
#define TELNET_DONT  254


/*---------------------------------------------------------------------------*/
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};

static void telnet(char *str);

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

enum mode {manual=0, timers=1, valve=2};

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

	clock_time_t last_poll;
} poll_data;

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

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(honeywell_process, ev, data)
{
	static struct etimer etimer;
	int rx;
	int buf_pos;
	char buf[128];
	static bool poll = false;

	PROCESS_BEGIN();

	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
	rs232_set_input(RS232_PORT_0, uart_get_char);
	Led1_on(); // red

	etimer_set(&etimer, CLOCK_SECOND * poll_time);

	while (1) {
		PROCESS_WAIT_EVENT();
		if(ev == PROCESS_EVENT_TIMER) {
			etimer_set(&etimer, CLOCK_SECOND * poll_time);
			telnet("INFO: polling...\r\nHONEYWELL> ");
			printf("D\n");
			poll = true;
		} else if (ev == PROCESS_EVENT_MSG) {
			buf_pos = 0;
			while ((rx=ringbuf_get(&uart_buf))!=-1) {
				if (buf_pos<126 && (char)rx=='\n') {
					buf[buf_pos++] = '\n';
					buf[buf_pos] = '\0';
					//printf("%s\r\n", buf);
					if(poll){
						poll = false;
						parseD(buf);
					}
					else{
						telnet(buf);
						telnet("HONEYWELL> ");
					}
					//ATInterpreterProcessCommand(buf);
					buf_pos = 0;
					continue;
				} else {
					buf[buf_pos++] = (char)rx;
				}
				if (buf_pos==127) {
					buf[buf_pos] = 0;
					telnet("ERROR: RX buffer overflow\r\n");
					telnet("HONEYWELL> ");
					buf_pos = 0;
				}
			} // while
		} // events
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/

struct telnetd_buf {
	char bufmem[TELNETD_CONF_NUMLINES * TELNETD_CONF_LINELEN];
	int  ptr;
	int  size;
};

static struct telnetd_buf send_buf;
static struct telnetd_state s;

#define MIN(a, b) ((a) < (b)? (a): (b))
/*---------------------------------------------------------------------------*/
static void buf_init(struct telnetd_buf *buf)
{
	buf->ptr = 0;
	buf->size = sizeof(buf->bufmem); // rows * cols
}
/*---------------------------------------------------------------------------*/
static int buf_append(struct telnetd_buf *buf, const char *data, int len)
{
	int copylen;

	copylen = MIN(len, buf->size - buf->ptr);
	memcpy(&buf->bufmem[buf->ptr], data, copylen);
	buf->ptr += copylen;

	return copylen;
}
/*---------------------------------------------------------------------------*/
static void buf_copyto(struct telnetd_buf *buf, char *to, int len)
{
	memcpy(to, &buf->bufmem[0], len);
}
/*---------------------------------------------------------------------------*/
static void buf_pop(struct telnetd_buf *buf, int len)
{
	int poplen;

	poplen = MIN(len, buf->ptr);
	memcpy(&buf->bufmem[0], &buf->bufmem[poplen], buf->ptr - poplen);
	buf->ptr -= poplen;
}
/*---------------------------------------------------------------------------*/
static int buf_len(struct telnetd_buf *buf)
{
	return buf->ptr;
}

/*---------------------------------------------------------------------------*/
static void telnetd_get_char(u8_t c)
{
	if (c == 0) {
		return;
	}

	if (c != ISO_nl && c != ISO_cr) {
		s.buf[(int)s.bufptr] = c;
		++s.bufptr;
	}

	if ((c == ISO_nl || c == ISO_cr) && s.bufptr > 0 ) {
		s.buf[(int)s.bufptr] = 0;
		printf("%s\r\n", s.buf);
		s.continued = 0;
		//telnet("HONEYWELL> ");
		s.bufptr = 0;
	} else if (s.bufptr == sizeof(s.buf)-1) {
		s.buf[(int)s.bufptr] = 0;
		printf(s.buf);
		s.continued = 1;
		s.bufptr = 0;
	}
}
/*---------------------------------------------------------------------------*/
static void sendopt(u8_t option, u8_t value)
{
	char line[4];
	line[0] = (char)TELNET_IAC;
	line[1] = option;
	line[2] = value;
	line[3] = 0;
	buf_append(&send_buf, line, 4);
}
/*---------------------------------------------------------------------------*/
static void newdata(void)
{
	u16_t len;
	u8_t c;
	uint8_t *ptr;

	len = uip_datalen();
	ptr = uip_appdata;

	while(len > 0 && s.bufptr < sizeof(s.buf)) {
		c = *ptr;
		++ptr;
		--len;

		switch(s.state) {
			case STATE_IAC:
				if(c == TELNET_IAC) {
					telnetd_get_char(c);
					s.state = STATE_NORMAL;
				} else {
					switch(c) {
						case TELNET_WILL:
							s.state = STATE_WILL;
							break;
						case TELNET_WONT:
							s.state = STATE_WONT;
							break;
						case TELNET_DO:
							s.state = STATE_DO;
							break;
						case TELNET_DONT:
							s.state = STATE_DONT;
							break;
						default:
							s.state = STATE_NORMAL;
							break;
					}
				}
				break;
			case STATE_WILL:
				/* Reply with a DONT */
				sendopt(TELNET_DONT, c);
				s.state = STATE_NORMAL;
				break;

			case STATE_WONT:
				/* Reply with a DONT */
				sendopt(TELNET_DONT, c);
				s.state = STATE_NORMAL;
				break;
			case STATE_DO:
				/* Reply with a WONT */
				sendopt(TELNET_WONT, c);
				s.state = STATE_NORMAL;
				break;
			case STATE_DONT:
				/* Reply with a WONT */
				sendopt(TELNET_WONT, c);
				s.state = STATE_NORMAL;
				break;
			case STATE_NORMAL:
				if(c == TELNET_IAC) {
					s.state = STATE_IAC;
				} else {
					telnetd_get_char(c);
				}
				break;
		}
	}
}
/*---------------------------------------------------------------------------*/
static void closed(void)
{
}
/*---------------------------------------------------------------------------*/
static void acked(void)
{
	buf_pop(&send_buf, s.numsent);
}
/*---------------------------------------------------------------------------*/
static void senddata(void)
{
	int len;
	len = MIN(buf_len(&send_buf), uip_mss());
	buf_copyto(&send_buf, uip_appdata, len);
	uip_send(uip_appdata, len);
	s.numsent = len;
}
/*---------------------------------------------------------------------------*/
void telnetd_appcall(void *ts)
{
	if(uip_connected()) {
		tcp_markconn(uip_conn, &s);
		buf_init(&send_buf);
		s.bufptr = 0;
		s.continued = 0;
		s.state = STATE_NORMAL;
		telnet("HONEYWELL> ");
	}

	if(s.state == STATE_CLOSE) {
		s.state = STATE_NORMAL;
		uip_close();
		return;
	}
	if(uip_closed() ||
			uip_aborted() ||
			uip_timedout()) {
		closed();
	}
	if(uip_acked()) {
		acked();
	}
	if(uip_newdata()) {
		newdata();
	}
	if(uip_rexmit() ||
			uip_newdata() ||
			uip_acked() ||
			uip_connected() ||
			uip_poll()) {
		senddata();
	}
}
/*---------------------------------------------------------------------------*/
void telnetd_quit(void)
{
	process_exit(&telnetd_process);
	LOADER_UNLOAD();
}
/*---------------------------------------------------------------------------*/
void telnet(char *str)
{
	buf_append(&send_buf, str, (int)strlen(str));
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(telnetd_process, ev, data)
{
	PROCESS_BEGIN();

	tcp_listen(UIP_HTONS(23));
	buf_init(&send_buf);

	while(1) {
		PROCESS_WAIT_EVENT();
		if(ev == tcpip_event) {
			telnetd_appcall(data);
		} else if(ev == PROCESS_EVENT_EXIT) {
			telnetd_quit();
		} else {

		}
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
	rest_set_response_payload(response, temp, strlen(temp));
}

RESOURCE(battery, METHOD_GET, "battery");
void battery_handler(REQUEST* request, RESPONSE* response)
{
	char temp[128];
	sprintf_P(temp, PSTR("%d"), poll_data.battery);

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, temp, strlen(temp));
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
				sprintf_P(temp, PSTR("Undefined"));
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
	rest_set_response_payload(response, temp, strlen(temp));
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
		if(rest_get_post_variable(request, "value", string, 4) == 0){ 
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=abc, eg: value=155 sets the temperature 15.5 degrees"));
		}
		else{
			uint16_t value = atoi(string);
			printf("A%02x\n",value/5);
			sprintf_P(temp, PSTR("Successfully set value"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, temp, strlen(temp));
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
		if(rest_get_post_variable(request, "value", string, 4) == 0){ 
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=aa, eg: value=15 sets the poll intervall to 15 seconds"));
		}
		else{
			poll_time = atoi(string);
			sprintf_P(temp, PSTR("Successfully set poll intervall"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, temp, strlen(temp));
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
		if(rest_get_post_variable(request, "value", string, 4) == 0){
			rest_set_response_status(response, BAD_REQUEST_400);
			sprintf_P(temp, PSTR("Payload format: value=aa, eg: value=47 sets the valve 47 percent"));
		}
		else{
			int new_valve=atoi(string);
			printf_P(PSTR("E%02x\n"),new_valve);
			sprintf_P(temp, PSTR("Successfully set valve position"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, temp, strlen(temp));
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
	rest_set_response_payload(response, temp, strlen(temp));
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
			sprintf_P(temp, PSTR("Payload format: value=mm:hh[:ss]"));
		}
	}

	rest_set_header_content_type(response, TEXT_PLAIN);
	rest_set_response_payload(response, temp, strlen(temp));
}



RESOURCE(discover, METHOD_GET, ".well-known/core");
void discover_handler(REQUEST* request, RESPONSE* response)
{
	char temp[128];
	int index = 0;
	index += sprintf_P(temp + index, PSTR("</temperature>"));
	index += sprintf_P(temp + index, PSTR("</target>"));
	index += sprintf_P(temp + index, PSTR("</mode>"));
	index += sprintf_P(temp + index, PSTR("</poll>"));
	index += sprintf_P(temp + index, PSTR("</valve>"));
	index += sprintf_P(temp + index, PSTR("</battery>"));
	index += sprintf_P(temp + index, PSTR("</date>"));
	index += sprintf_P(temp + index, PSTR("</time>"));


	rest_set_response_payload(response, temp, strlen(temp));
	rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
}


PROCESS(rest_server_example, "Rest Server Example");
PROCESS_THREAD(rest_server_example, ev, data)
{
	PROCESS_BEGIN();

	rest_init();
	
	rest_activate_resource(&resource_date);
	rest_activate_resource(&resource_time);
	rest_activate_resource(&resource_temperature);
	rest_activate_resource(&resource_battery);
	rest_activate_resource(&resource_target);
	rest_activate_resource(&resource_mode);
	rest_activate_resource(&resource_poll);
	rest_activate_resource(&resource_valve);
	rest_activate_resource(&resource_discover);

	PROCESS_END();
}




/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&telnetd_process, &honeywell_process, &rest_server_example);
/*---------------------------------------------------------------------------*/

