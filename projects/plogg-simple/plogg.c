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
#include "rs232.h"
#include "ringbuf.h"

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

#ifndef MAX 
#define MAX(a,b) ((a)<(b)?(b):(a))
#endif /* MAX */

#ifndef MIN
#define MIN(a, b) ((a) < (b)? (a) : (b))
#endif /* MIN */

#define TRUE 1
#define FALSE 0

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
	uint8_t powered;

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
			if (isdigit(centi)){
				number -= (centi - '0') *10;
				if (isdigit(mili)){
					number-= (mili - '0');
				}
			}
		}
	}
	else {
		number *= 1000;
		if (isdigit(deci)){
			number += (deci -'0') * 100;
			if (isdigit(centi)){
				number += (centi - '0')*10;
				if (isdigit(mili)){
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
		if (isdigit(centi)){
			number += (centi - '0') * 10;
			if (isdigit(mili)){
				number+= (mili - '0');
			}
		}
	}
	return number;
}


// Parses the responses from the Plogg and stores the values in the caching struct
// Make sure the strings matches the responses of the plogg
static void parse_Poll(){

	if (strncmp_P(poll_return,PSTR("Watts (-Gen +Con)"),17) == 0) {
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

  // Turns off the timer and sets all timers to 0000-0000
  printf_P(PSTR("UCAST:0021ED000004699D=SE 0\r\n"));
  etimer_set(&etimer, CLOCK_SECOND * 1);
  PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

  printf_P(PSTR("UCAST:0021ED000004699D=SO 0 0000-0000\r\n"));
  etimer_set(&etimer, CLOCK_SECOND * 1);
  PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

  printf_P(PSTR("UCAST:0021ED000004699D=SO 1 0000-0000\r\n"));
  etimer_set(&etimer, CLOCK_SECOND * 1);
  PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

  printf_P(PSTR("UCAST:0021ED000004699D=SO 2 0000-0000\r\n"));
  etimer_set(&etimer, CLOCK_SECOND * 1);
  PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

  printf_P(PSTR("UCAST:0021ED000004699D=SO 3 0000-0000\r\n"));
  etimer_set(&etimer, CLOCK_SECOND * 1);
  PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

  etimer_set(&etimer, CLOCK_SECOND * 15);

  while (1)
  {
    PROCESS_WAIT_EVENT();

    if (ev == PROCESS_EVENT_TIMER) {

      if (poll_number==16)
      {
        etimer_set(&etimer, CLOCK_SECOND * 1);
        PROCESS_WAIT_UNTIL(etimer_expired(&etimer));

        printf_P(PSTR("UCAST:0021ED000004699D=SR\r\n"));
        etimer_set(&etimer, CLOCK_SECOND * 3);
        poll_number = 1;
      }
      else
      {
        etimer_set(&etimer, CLOCK_SECOND * 5);

        if (poll_number==0)
        {
          // max values
          printf_P(PSTR("UCAST:0021ED000004699D=SM\r\n"));
        }
        else
        {
          printf_P(PSTR("UCAST:0021ED000004699D=SV\r\n"));
        }

        poll_number = (poll_number+1) % 12;
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
reset_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  poll_number = 16;
  printf_P(PSTR("UCAST:0021ED000004699D=SM 1\r\n"));
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE, PSTR("Reset successful\n"))
  );
}


/**************************** Max Values *************************************/
RESOURCE(max_power, METHOD_GET, "max/power", "Maximum power");
RESOURCE(max_current, METHOD_GET, "max/current", "Maximum current");
RESOURCE(max_voltage, METHOD_GET, "max/voltage", "Maximum voltage");

void
max_power_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ldW at %u %s %02u %02u:%02u:%02u\n"),poll_data.watts_max_value/1000, (poll_data.watts_max_value <0 ) ? ((poll_data.watts_max_value % 1000)*-1) : (poll_data.watts_max_value %1000), poll_data.watts_max_date_y, poll_data.watts_max_date_m, poll_data.watts_max_date_d,poll_data.watts_max_time_h, poll_data.watts_max_time_m, poll_data.watts_max_time_s)
  );
}
void
max_current_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer,REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ldA at %u %s %02u %02u:%02u:%02u\n"),poll_data.current_max_value/1000, (poll_data.current_max_value <0 ) ? ((poll_data.current_max_value % 1000)*-1) : (poll_data.current_max_value %1000), poll_data.current_max_date_y, poll_data.current_max_date_m, poll_data.current_max_date_d,poll_data.current_max_time_h, poll_data.current_max_time_m, poll_data.current_max_time_s)
  );
}
void
max_voltage_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ldV at %u %s %02u %02u:%02u:%02u\n"),poll_data.voltage_max_value/1000, (poll_data.voltage_max_value <0 ) ? ((poll_data.voltage_max_value % 1000)*-1) : (poll_data.voltage_max_value %1000), poll_data.voltage_max_date_y, poll_data.voltage_max_date_m, poll_data.voltage_max_date_d,poll_data.voltage_max_time_h, poll_data.voltage_max_time_m, poll_data.voltage_max_time_s)
  );
}


/**************************** Current Values **********************************/

RESOURCE(meter_power, METHOD_GET, "meter/power", "Power");
RESOURCE(meter_current, METHOD_GET, "meter/current", "Current");
RESOURCE(meter_voltage, METHOD_GET, "meter/voltage", "Voltage");
RESOURCE(meter_frequency, METHOD_GET, "meter/frequency", "Frequency");
RESOURCE(meter_phase, METHOD_GET, "meter/phase", "Phase");
RESOURCE(meter_reactive, METHOD_GET, "meter/reactive", "Reactive power");

void
meter_power_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld W\n"),poll_data.active_total/1000, (poll_data.active_total <0 ) ? ((poll_data.active_total % 1000)*-1) : (poll_data.active_total %1000))
  );
}
void
meter_current_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld A\n"),poll_data.current/1000, (poll_data.current <0 ) ? ((poll_data.current % 1000)*-1) : (poll_data.current %1000))
  );
}
void
meter_voltage_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld V\n"),poll_data.voltage/1000, (poll_data.voltage <0 ) ? ((poll_data.voltage % 1000)*-1) : (poll_data.voltage %1000))
  );
}
void
meter_frequency_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld Hz\n"),poll_data.frequency/1000, (poll_data.frequency <0 ) ? ((poll_data.frequency% 1000)*-1) : (poll_data.frequency %1000))
  );
}
void
meter_phase_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld Degrees\n"),poll_data.phase_angle/1000, (poll_data.phase_angle <0 ) ? ((poll_data.phase_angle % 1000)*-1) : (poll_data.phase_angle %1000))
  );
}
void
meter_reactive_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%ld.%03ld VAR\n"),poll_data.reactive_total/1000, (poll_data.reactive_total <0 ) ? ((poll_data.reactive_total % 1000)*-1) : (poll_data.reactive_total %1000))
  );
}

RESOURCE(consumption, METHOD_GET, "consumption", "Accumulated power");
RESOURCE(consumption_con, METHOD_GET, "consumption/consumed", "Accumulated consumption");
RESOURCE(consumption_gen, METHOD_GET, "consumption/generated", "Accumulated generation");

void
consumption_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kWh\n"),(poll_data.active_con-poll_data.active_gen)/1000, (poll_data.active_con-poll_data.active_gen) %1000)
  );
}
void
consumption_con_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kWh\n"),poll_data.active_con/1000, poll_data.active_con %1000)
  );
}
void
consumption_gen_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(
    response,
    (uint8_t *)buffer,
    snprintf_P((char *)buffer, REST_MAX_CHUNK_SIZE,PSTR("%lu.%03lu kWh\n"),poll_data.active_gen/1000, poll_data.active_gen %1000)
  );
}

/****************************** Power On/Off ***********************************/
RESOURCE(power, METHOD_GET | METHOD_PUT, "switch", "Power on/off/toggle");

void
power_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  
	const uint8_t * string = NULL;
	uint8_t success = TRUE;
	int index=0;
	
	if (REST.get_method_type(request) == METHOD_GET)
	{
          if (poll_data.powered)
          {
            index += snprintf_P((char *)buffer,REST_MAX_CHUNK_SIZE,PSTR("On\n"));
          }
          else
          {
            index += snprintf_P((char *)buffer,REST_MAX_CHUNK_SIZE,PSTR("Off\n"));
          }
	}
	else
	{
          int len = coap_get_payload(request, &string);
          if (len == 0)
          {
            success = FALSE;
          }
          else
          {
            if (strncmp_P((char *)string, PSTR("toggle"),MAX(len,6))==0)
            {
              if (poll_data.powered==FALSE)
              {
                strcpy_P((char *)string, PSTR("on"));
                len = 2;
              }
              else
              {
                strcpy_P((char *)string, PSTR("off"));
                len = 3;
              }
            }

            if (strncmp_P((char*)string,PSTR("on"),MAX(len,2))==0)
            {
              printf_P(PSTR("UCAST:0021ED000004699D=SE 0\r\n"));
              index += snprintf_P((char *)buffer,REST_MAX_CHUNK_SIZE,PSTR("Power on\n"));
              poll_data.powered=TRUE;
            }
            else if (strncmp_P((char*)string, PSTR("off"),MAX(len,3))==0)
            {
              printf_P(PSTR("UCAST:0021ED000004699D=SE 1\r\n"));
              index += snprintf_P((char *)buffer,REST_MAX_CHUNK_SIZE,PSTR("Power off\n"));
              poll_data.powered=FALSE;
            }
            else
            {
              success = FALSE;
            }
          }

          if (!success)
          {
            index += snprintf_P((char *)buffer,REST_MAX_CHUNK_SIZE,PSTR("Payload: {on,off,toggle}"));
            REST.set_response_status(response, REST.status.BAD_REQUEST);
          }
	}
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_response_payload(response, (uint8_t *) buffer , index);
}


/****************************** Coap Process ***********************************/
PROCESS_THREAD(coap_process, ev, data)
{
  PROCESS_BEGIN();

  rest_init_framework();
  
  rest_activate_resource(&resource_reset);

  rest_activate_resource(&resource_max_power);
  rest_activate_resource(&resource_max_current);
  rest_activate_resource(&resource_max_voltage);

  rest_activate_resource(&resource_meter_power);
  rest_activate_resource(&resource_meter_current);
  rest_activate_resource(&resource_meter_voltage);
  rest_activate_resource(&resource_meter_frequency);
  rest_activate_resource(&resource_meter_phase);
  rest_activate_resource(&resource_meter_reactive);

  rest_activate_resource(&resource_consumption);
  rest_activate_resource(&resource_consumption_con);
  rest_activate_resource(&resource_consumption_gen);

  rest_activate_resource(&resource_power);

  memset(&poll_data, 0, sizeof(poll_data));

  poll_data.mode = MANUAL;
  poll_data.powered = TRUE;

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&coap_process, &plogg_process);
/*---------------------------------------------------------------------------*/
