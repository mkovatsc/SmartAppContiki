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
#include <stdlib.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <ctype.h>
#include "contiki-raven.h"
#include "rs232.h"
#include "ringbuf.h"

#if WITH_COAP == 3
#include "coap-03.h"
#include "coap-03-transactions.h"
#elif WITH_COAP == 6
#include "coap-06.h"
#include "coap-06-transactions.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

#ifndef MAX 
#define MAX(a,b) ((a)<(b)?(b):(a))
#endif /* MAX */

#ifndef MIN
#define MIN(a, b) ((a) < (b)? (a) : (b))
#endif /* MIN */

PROCESS(coap_process, "Coap");
PROCESS(plogg_process, "Plogg comm");

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


/*_________________________RS232_____________________________________________*/
/*---------------------------------------------------------------------------*/
static struct ringbuf uart_buf;
static unsigned char uart_buf_data[128] = {0};
static char state = 0;
static uint8_t poll_number = 6;
static uint8_t mode_switch_number = 255;
static char poll_return[128];
enum mode{MANUAL=0, AUTO=1};

// Set Up EEPROM variables
uint16_t ee_start_time0 EEMEM=0;
uint16_t ee_end_time0 EEMEM=0;
uint16_t ee_start_time1 EEMEM=0;
uint16_t ee_end_time1 EEMEM=0;
uint16_t ee_start_time2 EEMEM=0;
uint16_t ee_end_time2 EEMEM=0;
uint16_t ee_start_time3 EEMEM=0;
uint16_t ee_end_time3 EEMEM=0;


// Caching Struct
static struct {

 	uint16_t date_y;
 	char date_m[4];
 	uint16_t date_d;
	uint16_t time_h;
	uint16_t time_m;
	uint16_t time_s;

	uint16_t plogg_time_d;
	uint16_t plogg_time_h;
	uint16_t plogg_time_m;
	uint16_t plogg_time_s;
	uint16_t equipment_time_d;
	uint16_t equipment_time_h;
	uint16_t equipment_time_m;
	uint16_t equipment_time_s;

	long current_max_value;
 	uint16_t current_max_date_y;
	char current_max_date_m[4];
 	uint16_t current_max_date_d;
	uint16_t current_max_time_h;
	uint16_t current_max_time_m;
	uint16_t current_max_time_s;

	long voltage_max_value;
 	uint16_t voltage_max_date_y;
 	char voltage_max_date_m[4];
 	uint16_t voltage_max_date_d;
	uint16_t voltage_max_time_h;
	uint16_t voltage_max_time_m;
	uint16_t voltage_max_time_s;

	long watts_max_value;
 	uint16_t watts_max_date_y;
 	char watts_max_date_m[4];
 	uint16_t watts_max_date_d;
	uint16_t watts_max_time_h;
	uint16_t watts_max_time_m;
	uint16_t watts_max_time_s;

	long frequency;
	long current;
	long voltage;
	long phase_angle;

	long active_total;
	unsigned long active_con;
	unsigned long active_gen;

	long reactive_total;
	unsigned long reactive_gen;
	unsigned long reactive_co;

	uint16_t tariff_zone;
	uint16_t tariff0_start;
	uint16_t tariff0_end;
	
	uint16_t tariff0_rate;
	uint16_t tariff1_rate;
	unsigned long tariff0_consumed;
	unsigned long tariff0_cost;
	unsigned long tariff1_consumed;
	unsigned long tariff1_cost;
	
	uint16_t mode;
	bool powered;

} poll_data;

/*---------------------------------------------------------------------------*/
static int uart_get_char(unsigned char c)
{
  ringbuf_put(&uart_buf, c);
  if (c=='\r') ++state;
  if ((state==1 && c=='\n') || ringbuf_size(&uart_buf)==127) {
    ringbuf_put(&uart_buf, '\0');
    process_post(&plogg_process, PROCESS_EVENT_MSG, NULL);
    state = 0;
  }
  return 1;
}


// Parses a signed float with up to 3 decimals and stores it as long int.
static long get_signed_pseudo_float_3(char* start){
	long number=0;
	char deci = '0';
	char centi = '0';
	char mili = '0';
	sscanf_P(start,PSTR("%ld%*c%c%c%c"),&number, &deci, &centi, &mili);
	if (number< 0){
		number *= 1000;
		if (isdigit(deci)){
			number -= (deci -'0') * 100;
			if(isdigit(centi)){
				number -= (centi - '0') *10;
				if(isdigit(mili)){
					number-= (mili - '0');
				}
			}
		}
	}
	else {
		number *= 1000;
		if (isdigit(deci)){
			number += (deci -'0') * 100;
			if(isdigit(centi)){
				number += (centi - '0')*10;
				if(isdigit(mili)){
					number+= (mili - '0');
				}
			}
		}
	}
	return number;
}

// Parses a unsigned float with up to 3 decimals and stores it as unsigned long int.
static unsigned long get_unsigned_pseudo_float_3(char* start){
	unsigned long number=0;
	char deci = '0';
	char centi = '0';
	char mili = '0';
	sscanf_P(start,PSTR("%lu%*c%c%c%c"),&number, &deci, &centi, &mili);
	number *= 1000;
	if (isdigit(deci)){
		number += (deci -'0') * 100;
		if(isdigit(centi)){
			number += (centi - '0') * 10;
			if(isdigit(mili)){
				number+= (mili - '0');
			}
		}
	}
	return number;
}


// Parses the responses from the Plogg and stores the values in the caching struct
// Make sure the strings matches the responses of the plogg
static void parse_Poll(){

	if( strncmp_P(poll_return,PSTR("Time entry"),10) == 0) {
		sscanf_P(poll_return+27,PSTR("%u %3s %u %u:%u:%u"),&poll_data.date_y,&poll_data.date_m,&poll_data.date_d,&poll_data.time_h,&poll_data.time_m,&poll_data.time_s);
		poll_data.date_m[3]='\0';
	}
	else if (strncmp_P(poll_return,PSTR("Watts (-Gen +Con)"),17) == 0) {
		poll_data.active_total = get_signed_pseudo_float_3(poll_return+27);
	}
	
	else if (strncmp_P(poll_return,PSTR("Cumulative Watts (Gen)"),22) == 0) {
		poll_data.active_gen = get_unsigned_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Cumulative Watts (Con)"),22) == 0) {
		poll_data.active_con = get_unsigned_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Frequency"),9) == 0){
		poll_data.frequency = get_signed_pseudo_float_3(poll_return+27);
		printf("%d", sizeof(poll_data));
	}
	else if (strncmp_P(poll_return,PSTR("RMS Voltage"),11) == 0){
		poll_data.voltage = get_signed_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("RMS Current"),11) == 0){
		poll_data.current = get_signed_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Reactive Power (-G/+C)"),22) == 0){
		poll_data.reactive_total = get_signed_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Acc Reactive Pwr (Gen)"),22) == 0){
		poll_data.reactive_gen = get_unsigned_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Acc Reactive Pwr (Con)"),22) == 0){
		poll_data.reactive_co = get_unsigned_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Phase Angle (V/I)"),17) == 0){
		poll_data.phase_angle = get_signed_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Plogg on time"),13) == 0){
		sscanf_P(poll_return+27,PSTR("%u %*s %u:%u:%u"),&poll_data.plogg_time_d,&poll_data.plogg_time_h,&poll_data.plogg_time_m,&poll_data.plogg_time_s);
	}
	else if (strncmp_P(poll_return,PSTR("Equipment on time"),17) == 0){
		sscanf_P(poll_return+27,PSTR("%u %*s %u:%u:%u"),&poll_data.equipment_time_d,&poll_data.equipment_time_h,&poll_data.equipment_time_m,&poll_data.equipment_time_s);
	}
	else if (strncmp_P(poll_return,PSTR("Highest RMS voltage"),19) == 0){
		poll_data.voltage_max_value = get_signed_pseudo_float_3(poll_return+24);
		sscanf_P(poll_return+24,PSTR("%*s %*c %*s %u %3s %u %u:%u:%u"),&poll_data.voltage_max_date_y,&poll_data.voltage_max_date_m,&poll_data.voltage_max_date_d,&poll_data.voltage_max_time_h,&poll_data.voltage_max_time_m,&poll_data.voltage_max_time_s);
		poll_data.voltage_max_date_m[3]='\0';

	}
	else if (strncmp_P(poll_return,PSTR("Highest RMS current"),19) == 0){
		poll_data.current_max_value = get_signed_pseudo_float_3(poll_return+24);

		sscanf_P(poll_return+24,PSTR("%*s %*c %*s %u %3s %u %u:%u:%u"),&poll_data.current_max_date_y,&poll_data.current_max_date_m,&poll_data.current_max_date_d,&poll_data.current_max_time_h,&poll_data.current_max_time_m,&poll_data.current_max_time_s);
		poll_data.current_max_date_m[3]='\0';
	}
	else if (strncmp_P(poll_return,PSTR("Highest wattage"),15) == 0){
		poll_data.watts_max_value = get_signed_pseudo_float_3(poll_return+20);
		sscanf_P(poll_return+20,PSTR("%*s %*c %*s %u %3s %u %u:%u:%u"),&poll_data.watts_max_date_y,&poll_data.watts_max_date_m,&poll_data.watts_max_date_d,&poll_data.watts_max_time_h,&poll_data.watts_max_time_m,&poll_data.watts_max_time_s);
		poll_data.watts_max_date_m[3]='\0';
	}
	else if (strncmp_P(poll_return,PSTR("No highest voltage was recorded"),31) == 0){
		poll_data.voltage_max_value=0;
	}
	else if (strncmp_P(poll_return,PSTR("No highest current was recorded"),31) == 0){
		poll_data.current_max_value=0;
	}
	else if (strncmp_P(poll_return,PSTR("No highest wattage was recorded"),31) == 0){
		poll_data.watts_max_value=0;
	}
	else if (strncmp_P(poll_return,PSTR("Tarrif 0 Cost"),13) == 0){ //Tarrif is not a typo. Plogg returns Tarrif in this case
		sscanf_P(poll_return+16,PSTR("%u"),&poll_data.tariff0_rate);
	}
	else if (strncmp_P(poll_return,PSTR("Tarrif 1 Cost"),13) == 0){ //Tarrif is not a typo. Plogg returns Tarrif in this case
		sscanf_P(poll_return+16,PSTR("%u"),&poll_data.tariff1_rate);
	}
	else if (strncmp_P(poll_return,PSTR("Current tarrif zone"),19)==0){
		sscanf_P(poll_return+22,PSTR("%d"),&poll_data.tariff_zone);
	}
	else if (strncmp_P(poll_return,PSTR("Tariff 0 from"),13) ==0){
		sscanf_P(poll_return+22,PSTR("%d%*c%d"),&poll_data.tariff0_start,&poll_data.tariff0_end);
	}
	else if (strncmp_P(poll_return,PSTR("Tariff0 :"),9)==0){
		poll_data.tariff0_consumed = get_unsigned_pseudo_float_3(poll_return+12);
		char* cost = strstr_P(poll_return,PSTR("Cost"));
		poll_data.tariff0_cost = get_unsigned_pseudo_float_3(cost+5);
	}
	else if (strncmp_P(poll_return,PSTR("Tariff1 :"),9)==0){
		poll_data.tariff1_consumed = get_unsigned_pseudo_float_3(poll_return+12);
		char* cost = strstr_P(poll_return,PSTR("Cost"));
		poll_data.tariff1_cost = get_unsigned_pseudo_float_3(cost+5);
	}
}


//Simulate the original ZigBee module.
static void ATInterpreterProcessCommand(char* command)
{
	// HACK: dummy commands we need to accept
	if ((strcmp_P(command, PSTR("ATS01=31f4")) == 0) ||
			(strcmp_P(command, PSTR("ATS00=0001")) == 0) ||
			(strcmp_P(command, PSTR("at+dassl")) == 0))
	{
		printf("%s\r\n", AT_RESPONSE_OK);
	}
	// we need to process the join command
	else if (strcmp_P(command, PSTR("at+jn")) == 0)
	{
		printf_P(PSTR("JPAN:11,31F4\r\n"));
		printf("%s\r\n", AT_RESPONSE_OK);
	}
	// check if the command starts with "AT". if not, we return ERROR
	else if (strlen(command) < strlen(AT_COMMAND_PREFIX) ||
			strncmp(command, AT_COMMAND_PREFIX, strlen(AT_COMMAND_PREFIX)) != 0)
	{
		printf("%s\r\n", AT_RESPONSE_ERROR);
	}
	// check if we have an simple "AT" command
	else if (strlen(command) == strlen(AT_COMMAND_PREFIX) &&
			strncmp(command, AT_COMMAND_PREFIX, strlen(AT_COMMAND_PREFIX)) == 0)
	{
		printf("%s\r\n", AT_RESPONSE_OK);
	}
	// check if we have an simple "ATI" command
	else if (strcmp(command, AT_COMMAND_INFO) == 0)
	{
		printf_P(PSTR("I am an AVR Raven Jackdaw pretending to be an ETRX2 Zigbee module.\r\n%s\r\n"), AT_RESPONSE_OK);
	}
	// check if we have an simple "ATZ" command
	else if (strncmp(command, AT_COMMAND_RESET, strlen(command)) == 0)
	{
		printf_P(PSTR("Resetting...\r\n%s\r\n"), AT_RESPONSE_OK);
	}
	// check if we have a unicast command
	else if (strncmp(command, AT_COMMAND_UNICAST, strlen(AT_COMMAND_UNICAST)) == 0)
	{
		// we need to parse the remaining part (payload points  after AT+UCAST:0021ED000004699D,)
		char* address = &command[strlen(AT_COMMAND_UNICAST)];
		char* payload = address;
		while ( *payload != ',' && *payload != '\0' )
		{
			payload++;
		}
		if ( *payload == ',' )
		{
			*payload = '\0';
			payload++;
		}
		// trim CR/LF at the end of the payload
		int len = strlen(payload);
		if (len > 0 && (payload[len-1] == '\r' || payload[len-1] == '\n'))
		{
			payload[len-1] = '\0';
			if (len > 1 && (payload[len-2] == '\r' || payload[len-2] == '\n'))
			{
				payload[len-2] = '\0';
			}
		}
		// The plogg uses ~~ to tell us there should be a newline
		// This code segment constructs complete lines and then calls the parsing function.
		uint8_t length = strlen(poll_return);
		strcpy(poll_return+length,payload);
		while (strstr(poll_return,"~~") != NULL){
			char * start= strstr(poll_return,"~~");
			start[0]='\0';
			parse_Poll();
			uint8_t rest_length = strlen(start+2);
			memmove(poll_return,start+2,rest_length+1);
		}
		printf_P(PSTR("+UCAST:00\r\n%s\r\n"), AT_RESPONSE_OK);
		// HOST is waiting for "ACK:00" or NACK
		printf_P(PSTR("ACK:00\r\n"));
	}
	else
	{
	  // default: we return ERROR
	  printf("%s\r\n", AT_RESPONSE_ERROR);
	}
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(plogg_process, ev, data)
{
  static struct etimer etimer;
  int rx;
  unsigned int buf_pos;
  char buf[128];

  PROCESS_BEGIN();
  
	ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
  rs232_set_input(RS232_PORT_0, uart_get_char);

  etimer_set(&etimer, CLOCK_SECOND * 15);

  while (1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_TIMER) {
			//send the nessecary commands to switch modes with a delay (simple method)
			switch (mode_switch_number){
				// manual mode
				// Turns off the timer and sets all timers to 0000-0000
				case 0:
					printf_P(PSTR("UCAST:0021ED000004699D=SE 0\r\n"));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;

				case 1:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 0 0000-0000\r\n"));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;
				case 2:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 1 0000-0000\r\n"));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;
				case 3:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 2 0000-0000\r\n"));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;
				case 4:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 3 0000-0000\r\n"));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND * 3);
					break;

				// Auto Mode
				// Restores all timers and activates timer
				case 128:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 0 %04u-%04u\r\n"),eeprom_read_word(&ee_start_time0),eeprom_read_word(&ee_end_time0));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;
				case 129:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 1 %04u-%04u\r\n"),eeprom_read_word(&ee_start_time1),eeprom_read_word(&ee_end_time1));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;
				case 130:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 2 %04u-%04u\r\n"),eeprom_read_word(&ee_start_time2),eeprom_read_word(&ee_end_time2));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;
				case 131:
					printf_P(PSTR("UCAST:0021ED000004699D=SO 3 %04u-%04u\r\n"),eeprom_read_word(&ee_start_time3),eeprom_read_word(&ee_end_time3));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND *1);
					break;
				case 132:
					printf_P(PSTR("UCAST:0021ED000004699D=SE 1\r\n"));
					mode_switch_number++;
					etimer_set(&etimer, CLOCK_SECOND * 3);
					break;	

				default:
					etimer_set(&etimer, CLOCK_SECOND * 3);
					switch (poll_number){
	    		 	// This makes sure the costs are polled every minute.
						case 15:
	    		 	case 35:
						case 55:
	      			printf_P(PSTR("UCAST:0021ED000004699D=SC\r\n"));
							break;
	    		 	// This makes sure the max values are polled every minute.
						case 5:
						case 25:
						case 45:
			      	printf_P(PSTR("UCAST:0021ED000004699D=SM\r\n"));
							break;
						// This polls the new set tariff time.
						// Only called if the time changes.
						case 68:
	    		  	printf_P(PSTR("UCAST:0021ED000004699D=ST\r\n"));
							break;
						// This polls the new set tariff rate.
						// Only called if the tariff changes.
						case 78:
			      	printf_P(PSTR("UCAST:0021ED000004699D=SS\r\n"));
							break;
						//This polls the new current values every 30 seconds.
						default:
							if (!(poll_number%10)){
								printf_P(PSTR("UCAST:0021ED000004699D=SV\r\n"));
							}
					}
					poll_number = (poll_number+1) % 60;
			}
    } else if (ev == PROCESS_EVENT_MSG) {
      buf_pos = 0;
      while ((rx=ringbuf_get(&uart_buf))!=-1) {
        if (buf_pos<126 && (char)rx=='\r') {
          rx = ringbuf_get(&uart_buf);
          if ((char)rx=='\n') {
            buf[buf_pos] = '\0';
            ATInterpreterProcessCommand(buf);
            buf_pos = 0;
            continue;
          } else {
            buf[buf_pos++] = '\r';
            buf[buf_pos++] = (char)rx;
          }
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

/*_______________________________COAP________________________________________*/
/*---------------------------------------------------------------------------*/

/****************************** Reset ****************************************/ 
RESOURCE(reset, METHOD_POST, "reset", "Reset");

void
reset_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	int index=0;
	char temp[REST_MAX_CHUNK_SIZE];
	const uint8_t * string=NULL;
	bool success=true;

	int len = coap_get_payload(request, &string);
	if(len == 0){ 
		success = false;
	}
	else{
		if (strncmp_P((char*)string, PSTR("cost"),MAX(len,4))==0){
			printf_P(PSTR("UCAST:0021ED000004699D=SC 1\r\n"));
			poll_number = 15;
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Reset successful\n"));
		}
		else if(strncmp_P((char*)string, PSTR("max"),MAX(len,3))==0){
			printf_P(PSTR("UCAST:0021ED000004699D=SM 1\r\n"));
			poll_number = 25;
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Reset successful\n"));
		}

		else if(strncmp_P((char*)string, PSTR("acc"),MAX(len,3))==0){
			printf_P(PSTR("UCAST:0021ED000004699D=SR\r\n"));
			poll_number = 10;
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Reset successful\n"));
		}
		else{
			success=false;
		}
	}
  if(!success){
		index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Payload: {acc,cost,max}\n"));
 	 	REST.set_response_status(response, REST.status.BAD_REQUEST);
	}
 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 	REST.set_response_payload(response, (uint8_t *) temp , index);
}


/**************************** Max Values *************************************/
RESOURCE(max, METHOD_GET, "max", "Max Values");

void
max_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){

	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;
	const char * query = NULL;
	bool success = true;

	int len = REST.get_query(request, &query);

	if (strncmp_P(query, PSTR("voltage"),MAX(len,7))==0){
		if (poll_data.voltage_max_value != 0){
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ldV at %u %s %02u %02u:%02u:%02u\n"),poll_data.voltage_max_value/1000, (poll_data.voltage_max_value <0 ) ? ((poll_data.voltage_max_value % 1000)*-1) : (poll_data.voltage_max_value %1000), poll_data.voltage_max_date_y, poll_data.voltage_max_date_m, poll_data.voltage_max_date_d,poll_data.voltage_max_time_h, poll_data.voltage_max_time_m, poll_data.voltage_max_time_s);
		}
		else{
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("No highest voltage was recorded\n"));
		}
	}
	else if(strncmp_P(query, PSTR("current"),MAX(len,7))==0){
		if (poll_data.current_max_value !=0){
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ldA at %u %s %02u %02u:%02u:%02u\n"),poll_data.current_max_value/1000, (poll_data.current_max_value <0 ) ? ((poll_data.current_max_value % 1000)*-1) : (poll_data.current_max_value %1000), poll_data.current_max_date_y, poll_data.current_max_date_m, poll_data.current_max_date_d,poll_data.current_max_time_h, poll_data.current_max_time_m, poll_data.current_max_time_s);
		}
		else{
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("No highest current was recorded\n"));
		}
	}
	else if(strncmp_P(query, PSTR("wattage"),MAX(len,7))==0){
		if (poll_data.watts_max_value !=0){
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ldW at %u %s %02u %02u:%02u:%02u\n"),poll_data.watts_max_value/1000, (poll_data.watts_max_value <0 ) ? ((poll_data.watts_max_value % 1000)*-1) : (poll_data.watts_max_value %1000), poll_data.watts_max_date_y, poll_data.watts_max_date_m, poll_data.watts_max_date_d,poll_data.watts_max_time_h, poll_data.watts_max_time_m, poll_data.watts_max_time_s);
		}
		else{
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("No highest wattage was recorded\n"));
		}
	}
	else{
		success = false;
	}
	if (!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		index += snprintf_P(temp, REST_MAX_CHUNK_SIZE, PSTR("Add a get parameter to select a peak. available peaks {voltage,current,wattage}\neg.: /max?voltage to get the voltage's peak\n"));
	}
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t *)temp , index);
}


/***************************** Date & Time ***********************************/
RESOURCE(time, METHOD_GET | METHOD_POST, "time", "Time");

void
time_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	int index=0;
	char temp[REST_MAX_CHUNK_SIZE];
	bool success = true;
	const uint8_t * string=NULL;
	int hour, min,sec;

	if (REST.get_method_type(request) == METHOD_GET){
		index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("%02u:%02u:%02u\n"), poll_data.time_h,poll_data.time_m,poll_data.time_s);
	}
	else{
		int len = coap_get_payload(request, &string);
		if (len == 5 || len ==8){
				hour = atoi((char*)&string[0]);
				min = atoi((char*)&string[3]);
			 	sec=(len==5)?0:atoi((char*)&string[6]);
				if (len==8 && !(isdigit(string[6]) && isdigit(string[7]) && string[5]==':') ){
					success = false;
				}
				else if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]))){
					success = false;
				}
				else if ( string[2]!=':' ){
					success = false;
				}
				else if (!( 0<= hour && hour<=23 && 0<=min && min <=59 && 0<=sec && sec<=59)){
					success = false; 
				}
		}
		else{
			success = false;
		}
	 	if (success){
			printf_P(PSTR("UCAST:0021ED000004699D=rtt%02d.%02d.%02d\r\n"),hour,min,sec);
			poll_number = 10;
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Time set to %02d:%02d:%02d\n"),hour,min,sec);
		}
		else{
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Payload: hh:mm[:ss]\n"));
 		 	REST.set_response_status(response, REST.status.BAD_REQUEST);
		}
	}
 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 	REST.set_response_payload(response, (uint8_t *)temp , index);
}


RESOURCE(date, METHOD_GET | METHOD_POST, "date", "Date");

void
date_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	int index=0;
	char temp[REST_MAX_CHUNK_SIZE];
	bool success = true;
	const uint8_t * string = NULL;
	int month, day, year;

	if (REST.get_method_type(request) == METHOD_GET){
		index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("%02u %s %u\n"),poll_data.date_d,poll_data.date_m,poll_data.date_y);
	}
	else{
		int len = coap_get_payload(request, &string);
		if (len==8){
				day = atoi((char*)&string[0]);
				month = atoi((char*)&string[3]);
				year = atoi((char*)&string[6]);
				if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]) && isdigit(string[6]) && isdigit(string[7]))){
					success=false;
				} 
				else if ( string[2]!='.' || string[5]!='.' ){
					success=false;
				}
				else if (!(0<=year && year <=99 && 1<=month && month<=12 && 1<=day )){
					success=false;
				}
				else if( (month==4 || month ==6 || month==9 || month==11) && day>30){
					success=false;
				}
				else if( month==2 && !((year%4)==0) && day > 28) {
					success=false;
				}
				else if( month==2 && day>29){
					success=false;
				}
				else if( day > 31){
					success=false;
				}
		}
		else{
			success= false;	
		}
	 	if (success){
			printf_P(PSTR("UCAST:0021ED000004699D=rtd%02i.%02i.%02i\r\n"),year,month,day);
			poll_number = 10;
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Date set to %02i.%02i.%02i\n"),day,month,year);
		}
		else{
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Payload: dd.mm.yy\n"));
 		  REST.set_response_status(response, REST.status.BAD_REQUEST);
		}
	}
 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 	REST.set_response_payload(response, (uint8_t *)temp , index);
}


/**************************** Current Values **********************************/
RESOURCE(state, METHOD_GET, "state", "State");

void
state_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	char temp[REST_MAX_CHUNK_SIZE];
	const char* query = NULL;
	int index =0;

	int len = REST.get_query(request,&query);
	if (strncmp_P(query, PSTR("voltage"),MAX(len,7))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld V\n"),poll_data.voltage/1000, (poll_data.voltage <0 ) ? ((poll_data.voltage % 1000)*-1) : (poll_data.voltage %1000));
	
	}
	else if (strncmp_P(query, PSTR("current"),MAX(len,7))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld A\n"),poll_data.current/1000, (poll_data.current <0 ) ? ((poll_data.current % 1000)*-1) : (poll_data.current %1000));
	}
	else if (strncmp_P(query, PSTR("frequency"),MAX(len,9))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld Hz\n"),poll_data.frequency/1000, (poll_data.frequency <0 ) ? ((poll_data.frequency% 1000)*-1) : (poll_data.frequency %1000));
 
	}
	else if (strncmp_P(query, PSTR("active"),MAX(len,6))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld W\n"),poll_data.active_total/1000, (poll_data.active_total <0 ) ? ((poll_data.active_total % 1000)*-1) : (poll_data.active_total %1000));

	}
	else if (strncmp_P(query, PSTR("active_generated"),MAX(len,16))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kWh\n"),poll_data.active_gen/1000, poll_data.active_gen %1000);

	}
	else if (strncmp_P(query, PSTR("active_consumed"),MAX(len,15))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kWh\n"),poll_data.active_con/1000, poll_data.active_con %1000);

	}
	else if (strncmp_P(query, PSTR("phase"),MAX(len,5))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld Degrees\n"),poll_data.phase_angle/1000, (poll_data.phase_angle <0 ) ? ((poll_data.phase_angle % 1000)*-1) : (poll_data.phase_angle %1000));

	}
	else if (strncmp_P(query, PSTR("plogg_time"),MAX(len,10))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%u days %u:%02u:%02u \n"),poll_data.plogg_time_d, poll_data.plogg_time_h, poll_data.plogg_time_m, poll_data.plogg_time_s);
	
	}
	else if (strncmp_P(query, PSTR("equipment_time"),MAX(len,14))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%u days %u:%02u:%02u \n"),poll_data.equipment_time_d, poll_data.equipment_time_h, poll_data.equipment_time_m, poll_data.equipment_time_s);
	
	}
	else if (strncmp_P(query, PSTR("reactive"),MAX(len,8))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld VAR\n"),poll_data.reactive_total/1000, (poll_data.reactive_total <0 ) ? ((poll_data.reactive_total % 1000)*-1) : (poll_data.reactive_total %1000));
		
	}
	else if (strncmp_P(query, PSTR("reactive_generated"),MAX(len,18))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kVARh\n"),poll_data.reactive_gen/1000, poll_data.reactive_gen %1000);

	}
	else if (strncmp_P(query, PSTR("reactive_consumed"),MAX(len,17))==0){
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kVARh\n"),poll_data.reactive_co/1000, poll_data.reactive_co %1000);

	}
	else{
		size_t strpos = 0;
		strpos += snprintf_P((char*) buffer, REST_MAX_CHUNK_SIZE,PSTR("Add get variable to select a value. Possible choices are:\n -voltage\n -current\n -frequency\n -phase\n -plogg_time\n -equipment_time\n -active\n -active_consumed\n -active_generated\n -reactive\n -reactive_consumed\n -reactive_generated\neg.: /state?voltage\n")+*offset);
		REST.set_response_payload(response, buffer, strpos);
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 		REST.set_response_status(response, REST.status.BAD_REQUEST);
		*offset += REST_MAX_CHUNK_SIZE;
		if (*offset >= strpos){
			*offset = -1;
		}
		return;
	}
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t *)temp , index);
}


/***************************** Tariff Timer ************************************/
RESOURCE(tariff_timer,METHOD_GET | METHOD_POST, "tariff/timer", "Tariff Timer");

void
tariff_timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;
	const char* string=NULL;
	bool success= true;
	char minutes[3];

	if (REST.get_method_type(request) == METHOD_GET){
	  printf_P(PSTR("UCAST:0021ED000004699D=ST\r\n"));
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%02u:%02u-%02u:%02u\n"),poll_data.tariff0_start/100,poll_data.tariff0_start % 100,poll_data.tariff0_end/100,poll_data.tariff0_end % 100);
	}
	else{
		int len = REST.get_post_variable(request, "start", &string);
		int start_hour=0;
		int start_min=0;
		if (len == 5){
			strncpy(minutes, &string[3], 2);
			minutes[2]=0;
			start_hour = atoi(&string[0]);
			start_min = atoi(minutes);
			if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]))){
				success = false;
			}
			else if ( string[2]!=':' ){
				success = false;
			}
			else if (!( 0<= start_hour && start_hour<=23 && 0<=start_min && start_min <=59)){
				success = false; 
			}
		}
		else{
			success = false;
		}
		len = REST.get_post_variable(request, "end", &string);
		int end_hour=0;
		int end_min=0;
		if (len == 5){
			strncpy(minutes, &string[3], 2);
			minutes[2]=0;
			end_hour = atoi(&string[0]);
			end_min = atoi(minutes);
			if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]))){
				success = false;
			}
			else if ( string[2]!=':' ){
				success = false;
			}
			else if (!( 0<= end_hour && end_hour<=23 && 0<=end_min && end_min <=59)){
				success = false; 
			}
		}
		else{
			success = false;
		}
	 	if (success){
			uint16_t tariff_start=start_hour*100+start_min;
			uint16_t tariff_end=end_hour*100+end_min;
			printf_P(PSTR("UCAST:0021ED000004699D=ST %04u-%04u\r\n"),tariff_start,tariff_end);
			poll_number = 68;
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Tariff 1 active Time: %02u:%02u-%02u:%02u\n"),tariff_start/100,tariff_start % 100,tariff_end/100,tariff_end % 100);
		}
		else{
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Payload: start=hh:mm&end=hh:mm\n"));
 			REST.set_response_status(response, REST.status.BAD_REQUEST);
		}
	}
 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 	REST.set_response_payload(response, (uint8_t *)temp , index);

}


/***************************** Tariff Rate ************************************/
RESOURCE(tariff_rate,METHOD_GET | METHOD_POST, "tariff/rate", "Tariff Rate");

void
tariff_rate_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;
	const uint8_t * string=NULL;
	bool success= false;
	uint16_t rate=0;
	int tariff=0;
	uint8_t len;
	const char * query = NULL;

	if ((len = REST.get_query(request, &query))){
		if(isdigit(query[0]) && len == 1){
			char c[2];
			c[0]=query[0];
			c[1]='\0';
			tariff = atoi(c);
			if(tariff <= 2 && tariff >= 0){
				//humans start counting from 1 not 0
				tariff--;
				success = true;
			}
		}
	}
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		index += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Add a get parameter [0;2] that specifies the tariff, eg.: /tariff/rate?1 to interact with tariff 1.\nO is an overview"));
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, index);
	 	printf_P(PSTR("UCAST:0021ED000004699D=SS\r\n"));
		return;
	}
	if (tariff==-1){
	 	printf_P(PSTR("UCAST:0021ED000004699D=SS\r\n"));
		if (REST.get_method_type(request)==METHOD_POST){
			REST.set_response_status(response, REST.status.METHOD_NOT_ALLOWED);
			index += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Overview not allowed with POST"));
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, index);
		}
		else{
			int i;
			for (i=0; i<2; i++){
				switch (i){
					case 0: rate=poll_data.tariff0_rate; break;
					case 1: rate=poll_data.tariff1_rate; break;
				}
				index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("Tariff %d: %u pence/kWh\n"),(i+1),rate);
			}
			REST.set_response_payload(response, (uint8_t *)temp , index);
		}	
		return;
	}

	if (REST.get_method_type(request) == METHOD_GET){
	 	printf_P(PSTR("UCAST:0021ED000004699D=SS\r\n"));
		switch (tariff){
			case 0: rate=poll_data.tariff0_rate; break;
			case 1: rate=poll_data.tariff1_rate; break;
		}
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%u pence/kWh\n"),rate);
	}
	else{
		int len = coap_get_payload(request, &string);
		if (len==0){
			success=false;
		}
		else{
			rate = atoi((char*)&string[0]);
			if (!(isdigit(string[0]))){
				success = false;
			}
			else if (!( 0<= rate && rate < 1000)){
				success = false; 
			}
		}
	 	if (success){
			printf_P(PSTR("UCAST:0021ED000004699D=SS %u %u\r\n"),tariff,rate);
			poll_number = 78;
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("Tariff %u is now %u pence/kWh\n"),tariff+1,rate);
		}
		else{
			index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE, PSTR("Payload: ppp\n"));
 			REST.set_response_status(response, REST.status.BAD_REQUEST);
		}
	}
 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
 	REST.set_response_payload(response, (uint8_t *)temp , index);
}


/****************************** Costs ************************************/
RESOURCE(tariff_cost,METHOD_GET, "tariff/cost", "Tarrif Costs");

void
tariff_cost_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;
	unsigned long cost=0;
	int tariff= 0;
	bool success = false;
	uint8_t len;
	const char * query = NULL;

	if ((len = REST.get_query(request, &query))){
		if(isdigit(query[0]) && len == 1){
			char c[2];
			c[0]=query[0];
			c[1]='\0';
			tariff = atoi(c);
			if(tariff <= 2 && tariff >= 0){
				//humans start counting from 1 not 0
				tariff--;
				success = true;
			}
		}
	}
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		index += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Add a get parameter [0;2] that specifies the tariff.\n0 is the total cost\n"));
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, index);
		return;
	}
	switch (tariff){
		case 0: cost=poll_data.tariff0_cost; break;
		case 1: cost=poll_data.tariff1_cost; break;
		case -1: cost=poll_data.tariff0_cost+poll_data.tariff1_cost; break;
	}
	index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%lu.%02lu\n"), cost / 1000, (cost %1000)/10);
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t *)temp , index);
}


/****************************** Tariff Consumed ************************************/
RESOURCE(tariff_consumed,METHOD_GET, "tariff/consumed", "Tariff Consumation in kWh");

void
tariff_consumed_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;
	unsigned long consumed=0;
	int tariff= 0;
	bool success = false;
	uint8_t len;
	const char * query = NULL;

	if ((len = REST.get_query(request, &query))){
		if(isdigit(query[0]) && len == 1){
			char c[2];
			c[0]=query[0];
			c[1]='\0';
			tariff = atoi(c);
			if(tariff <= 2 && tariff >= 0){
				//humans start counting from 1 not 0
				tariff--;
				success = true;
			}
		}
	}
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		index += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Add a get parameter [0;2] that specifies the tariff.\n0 is the total consumation\n"));
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, index);
		return;
	}
	switch (tariff){
		case 0: consumed=poll_data.tariff0_consumed; break;
		case 1: consumed=poll_data.tariff1_consumed; break;
		case -1: consumed=poll_data.tariff0_consumed+poll_data.tariff1_consumed; break;
	}
	index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kWh\n"), consumed / 1000, (consumed %1000));
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t *)temp , index);
}


/********************************** Timers *************************************/

RESOURCE(timer,METHOD_GET|METHOD_POST, "timer", "Timers");

void
timer_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;
	const char* string=NULL;
	uint16_t start_time=0;
	uint16_t end_time=0;
	int timer= 0;
	bool success = false;
	uint8_t len;
	const char * query = NULL;
	char minutes[3];

	if (!(poll_data.mode == AUTO)){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Change to auto mode first\n"));
	 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, (uint8_t *)temp , index);
		return;
	}
	if ((len = REST.get_query(request, &query))){
		if(isdigit(query[0]) && len == 1){
			char c[2];
			c[0]=query[0];
			c[1]='\0';
			timer = atoi(c);
			if(timer <= 4 && timer >= 0){
				//humans start counting from 1 not 0
				timer--;
				success = true;
			}
		}
	}
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		index += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Add a get parameter [0;4] that specifies the timer, eg.: /timer?2 to interact with timer 2\nOr 0 eg.: timer?0 for an overview\n"));
		REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, buffer, index);
		return;
	}
	if(timer==-1){
		if (REST.get_method_type(request)==METHOD_POST){
			REST.set_response_status(response, REST.status.METHOD_NOT_ALLOWED);
			index += snprintf_P((char*)buffer, REST_MAX_CHUNK_SIZE, PSTR("Overview not allowed with POST"));
			REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
			REST.set_response_payload(response, buffer, index);
		}
		else{
			int i;
			for (i=0; i<4; i++){
				switch (i){
					case 0:
						start_time = eeprom_read_word(&ee_start_time0);
						end_time =  eeprom_read_word(&ee_end_time0);
						break;
					case 1:
						start_time = eeprom_read_word(&ee_start_time1);
						end_time =  eeprom_read_word(&ee_end_time1);
						break;	
						case 2:
						start_time = eeprom_read_word(&ee_start_time2);
						end_time =  eeprom_read_word(&ee_end_time2);
						break;	
					case 3:
						start_time = eeprom_read_word(&ee_start_time3);
						end_time =  eeprom_read_word(&ee_end_time3);
						break;	
				}
				index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("Timer %d: %02u:%02u-%02u:%02u\n"),i+1,start_time/100,start_time % 100,end_time/100,end_time % 100);
			}
			REST.set_response_payload(response, (uint8_t *)temp , index);
		}	
		return;
	}
	if (REST.get_method_type(request) == METHOD_GET){
		switch (timer){
			case 0:
				start_time = eeprom_read_word(&ee_start_time0);
				end_time =  eeprom_read_word(&ee_end_time0);
				break;
			case 1:
				start_time = eeprom_read_word(&ee_start_time1);
				end_time =  eeprom_read_word(&ee_end_time1);
				break;	
			case 2:
				start_time = eeprom_read_word(&ee_start_time2);
				end_time =  eeprom_read_word(&ee_end_time2);
				break;	
			case 3:
				start_time = eeprom_read_word(&ee_start_time3);
				end_time =  eeprom_read_word(&ee_end_time3);
				break;	
		}
		index += snprintf_P(temp+index,REST_MAX_CHUNK_SIZE,PSTR("%02u:%02u-%02u:%02u\n"),start_time/100,start_time % 100,end_time/100,end_time % 100);
	}
	else{
		int len = REST.get_post_variable(request, "start", &string);
		int start_hour=0;
		int start_min=0;
		if (len == 5){
			strncpy(minutes, &string[3], 2);
			minutes[2]=0;
			start_hour = atoi(&string[0]);
			start_min = atoi(minutes);
			if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]))){
				success = false;
			}
			else if ( string[2]!=':' ){
				success = false;
			}
			else if (!( 0<= start_hour && start_hour<=23 && 0<=start_min && start_min <=59)){
				success = false; 
			}
		}
		else{
			success = false;
		}
		len = REST.get_post_variable(request, "end", &string);
		int end_hour=0;
		int end_min=0;
		if (len == 5){
			strncpy(minutes, &string[3], 2);
			minutes[2]=0;
			end_hour = atoi(&string[0]);
			end_min = atoi(minutes);
			if (!(isdigit(string[0]) &&  isdigit(string[1]) && isdigit(string[3]) && isdigit(string[4]))){
				success = false;
			}
			else if ( string[2]!=':' ){
				success = false;
			}
			else if (!( 0<= end_hour && end_hour<=23 && 0<=end_min && end_min <=59)){
				success = false; 
			}
		}
		else{
			success = false;
		}
	 	if (success){
			start_time=start_hour*100+start_min;
			end_time=end_hour*100+end_min;
			switch (timer){
				case 0:
					eeprom_write_word(&ee_start_time0, start_time);
					eeprom_write_word(&ee_end_time0, end_time);
					break;
				case 1:
					eeprom_write_word(&ee_start_time1, start_time);
					eeprom_write_word(&ee_end_time1, end_time);
					break;	
				case 2:
					eeprom_write_word(&ee_start_time2, start_time);
					eeprom_write_word(&ee_end_time2, end_time);
					break;	
				case 3:
					eeprom_write_word(&ee_start_time3, start_time);
					eeprom_write_word(&ee_end_time3, end_time);
					break;	
			}
			printf_P(PSTR("UCAST:0021ED000004699D=SO %u %04u-%04u\r\n"),timer,start_time,end_time);
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Timer %u: %02u:%02u-%02u:%02u\n"),timer+1,start_time/100,start_time%100,end_time/100,end_time%100);
		}
		else{
			index+=snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Payload: start=hh:mm&end=hh:mm\n"));
 			REST.set_response_status(response, REST.status.BAD_REQUEST);
		}
	}
 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t *)temp , index);
}


/****************************** Modes ******************************************/
RESOURCE(mode, METHOD_GET | METHOD_POST, "mode", "Mode auto/manual");

void
mode_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  
	const uint8_t * string=NULL;
  bool success = true;
	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;

	if (REST.get_method_type(request) == METHOD_GET){
		switch (poll_data.mode){
			case MANUAL:
				index+=snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("manual\n"));
				break;
			case AUTO:
	  		index +=snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("auto\n"));
				break;
			default:	
	  		index+=snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("undefined\n"));
				break;
		}
	}
	else{
		int len = coap_get_payload(request, &string);
		if(len == 0){
			success = false;
		}
		else{
			if(strncmp_P((char*) string,PSTR("manual"),MAX(len,6))==0){
				//set all timers to 0000-0000
				// Needs to be done by interrupts, can't send all commands at once
				mode_switch_number=0;
				poll_data.mode = MANUAL;
				index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("New mode is: manual\n"));
				poll_data.powered = true;
			}
			else if(strncmp_P((char*) string, PSTR("auto"),MAX(len,4))==0){
				//set all timers
				// Needs to be done by interrupts, can't send all commands at once
				mode_switch_number=128;
				poll_data.mode = AUTO;
				index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("New mode is: auto\n"));
			}
 			else{
	 	   	success = false;
			}
		}
 	 	if (!success){
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Payload: {manual,auto}"));
 	 	  REST.set_response_status(response, REST.status.BAD_REQUEST);
 	 	}
	}
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, (uint8_t *) temp , index);
}


/****************************** Power On/Off ***********************************/
RESOURCE(power, METHOD_GET | METHOD_POST, "power", "Power On/Off");

void
power_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  
	const uint8_t * string=NULL;
	bool success = true;
	char temp[REST_MAX_CHUNK_SIZE];
	int index=0;
	
	if(!(poll_data.mode == MANUAL)){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
		index += snprintf_P(temp,REST_MAX_CHUNK_SIZE, PSTR("Change to manual mode first\n"));
	 	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
		REST.set_response_payload(response, (uint8_t *)temp , index);
		return;
	}
	if (REST.get_method_type(request) == METHOD_GET){
		if (poll_data.powered){
	  	index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("On\n"));
		}
		else{
	  	index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Off\n"));
		}
	}
	else{
		int len = coap_get_payload(request, &string);
		if(len == 0){
			success = false;
		}
		else{
			if(strncmp_P((char*)string,PSTR("on"),MAX(len,2))==0){
				printf_P(PSTR("UCAST:0021ED000004699D=SE 0\r\n"));
	  		index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Power on\n"));
				poll_data.powered=true;
			}
			else if(strncmp_P((char*)string, PSTR("off"),MAX(len,2))==0){
				printf_P(PSTR("UCAST:0021ED000004699D=SE 1\r\n"));
	  		index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Power off\n"));
				poll_data.powered=false;
			}
 			else{
	 	   	success = false;
			}
		}
 	 	if (!success){
			index += snprintf_P(temp,REST_MAX_CHUNK_SIZE,PSTR("Payload: {on,off}"));
 	 	  REST.set_response_status(response, REST.status.BAD_REQUEST);
 	 	}
	}
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t *) temp , index);
}


/****************************** Coap Process ***********************************/
PROCESS_THREAD(coap_process, ev, data)
{
  PROCESS_BEGIN();

  rest_init_framework();
  
  rest_activate_resource(&resource_time);
  rest_activate_resource(&resource_date);
	
  rest_activate_resource(&resource_max);
  
	rest_activate_resource(&resource_reset);
  
	rest_activate_resource(&resource_state);

	rest_activate_resource(&resource_tariff_timer);
	rest_activate_resource(&resource_tariff_rate);
	rest_activate_resource(&resource_tariff_cost);
	rest_activate_resource(&resource_tariff_consumed);

	rest_activate_resource(&resource_timer);
	rest_activate_resource(&resource_power);
	rest_activate_resource(&resource_mode);

	memset(&poll_data, 0, sizeof(poll_data));	
	poll_data.powered=true;
	poll_data.mode=255;

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&coap_process, &plogg_process);
/*---------------------------------------------------------------------------*/
