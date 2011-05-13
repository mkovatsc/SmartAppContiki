#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <ctype.h>
#include "contiki-raven.h"
#include "rs232.h"
#include "ringbuf.h"
#include "rest.h"
/*
#include "UsefulMicropendousDefines.h"
// set up external SRAM prior to anything else to make sure malloc() has access to it
void EnableExternalSRAM (void) __attribute__ ((naked)) __attribute__ ((section (".init3")));
void EnableExternalSRAM(void)
{
  PORTE_EXT_SRAM_SETUP;  // set up control port
  ENABLE_EXT_SRAM;       // enable the external SRAM
  XMCRA = ((1 << SRE));  // enable XMEM interface with 0 wait states
  XMCRB = 0;
  SELECT_EXT_SRAM_BANK0; // select Bank 0
}
*/

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
static uint8_t poll_number = 0;

static char poll_return[128];
uint8_t poll_return_index;

static struct {
	char activated;
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

	long watts_total;
	unsigned long watts_con;
	unsigned long watts_gen;
	long frequency;
	long current;
	long voltage;
	long power_total;
	unsigned long power_gen;
	unsigned long power_con;
	long phase_angle;

	uint16_t tariff_zone;
	uint16_t tariff0_start;
	uint16_t tariff0_end;
	
	long tariff0;
	long tariff1;
	unsigned long tariff0_consumed;
	unsigned long tariff0_cost;
	unsigned long tariff1_consumed;
	unsigned long tariff1_cost;

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

static long get_signed_pseudo_float_3(char* string){
	long number=0;
	char deci = '0';
	char centi = '0';
	char mili = '0';
	sscanf_P(string,PSTR("%ld%*c%c%c%c"),&number, &deci, &centi, &mili);
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

static unsigned long get_unsigned_pseudo_float_3(char* string){
	unsigned long number=0;
	char deci = '0';
	char centi = '0';
	char mili = '0';
	sscanf_P(string,PSTR("%lu%*c%c%c%c"),&number, &deci, &centi, &mili);
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

static void parse_Poll(){

	//printf("%s\r\n",poll_return);

	if( strncmp_P(poll_return,PSTR("Time entry"),10) == 0) {
		sscanf_P(poll_return+27,PSTR("%u %3s %u %u:%u:%u"),&poll_data.date_y,&poll_data.date_m,&poll_data.date_d,&poll_data.time_h,&poll_data.time_m,&poll_data.time_s);
		poll_data.date_m[3]='\0';
		//printf("%u %s %02u\r\n",poll_data.date_y, poll_data.date_m, poll_data.date_d);
		//printf("%02u:%02u:%02u\r\n",poll_data.time_h,poll_data.time_m,poll_data.time_s);
	}
	else if (strncmp_P(poll_return,PSTR("Watts (-Gen +Con)"),17) == 0) {
		poll_data.watts_total = get_signed_pseudo_float_3(poll_return+27);
	//	printf("Total Watts: %ld.%03ld\r\n",poll_data.watts_total/1000, (poll_data.watts_total <0 ) ? ((poll_data.watts_total % 1000)*-1) : (poll_data.watts_total %1000) );
	}
	
	else if (strncmp_P(poll_return,PSTR("Cumulative Watts (Gen)"),22) == 0) {
		poll_data.watts_gen = get_unsigned_pseudo_float_3(poll_return+27);
	//	printf("Cumulative Watts (gen): %lu.%03lu\r\n",poll_data.watts_gen/1000, poll_data.watts_gen % 1000 );
	}
	else if (strncmp_P(poll_return,PSTR("Cumulative Watts (Con)"),22) == 0) {
		poll_data.watts_con = get_unsigned_pseudo_float_3(poll_return+27);
	//	printf("Cumulative Watts (con): %lu.%03lu\r\n",poll_data.watts_con/1000, poll_data.watts_con % 1000 );
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
		poll_data.power_total = get_signed_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Acc Reactive Pwr (Gen)"),22) == 0){
		poll_data.power_gen = get_unsigned_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Acc Reactive Pwr (Con)"),22) == 0){
		poll_data.power_con = get_unsigned_pseudo_float_3(poll_return+27);
	}
	else if (strncmp_P(poll_return,PSTR("Phase Angle (V/I)"),17) == 0){
		poll_data.phase_angle = get_signed_pseudo_float_3(poll_return+27);
		//printf("Phase: %ld.%03ld\r\n",poll_data.phase_angle/1000, (poll_data.phase_angle <0 ) ? ((poll_data.phase_angle % 1000)*-1) : (poll_data.phase_angle %1000) );
	}
	else if (strncmp_P(poll_return,PSTR("Plogg on time"),13) == 0){
		sscanf_P(poll_return+27,PSTR("%u %*s %u:%u:%u"),&poll_data.plogg_time_d,&poll_data.plogg_time_h,&poll_data.plogg_time_m,&poll_data.plogg_time_s);
		//printf("%u days %02u:%02u:%02u\r\n",poll_data.plogg_time_d,poll_data.plogg_time_h,poll_data.plogg_time_m,poll_data.plogg_time_s);
	}
	else if (strncmp_P(poll_return,PSTR("Equipment on time"),17) == 0){
		sscanf_P(poll_return+27,PSTR("%u %*s %u:%u:%u"),&poll_data.equipment_time_d,&poll_data.equipment_time_h,&poll_data.equipment_time_m,&poll_data.equipment_time_s);
		//printf("%u days %02u:%02u:%02u\r\n",poll_data.equipment_time_d,poll_data.equipment_time_h,poll_data.equipment_time_m,poll_data.equipment_time_s);
	}
	else if (strncmp_P(poll_return,PSTR("Highest RMS voltage"),19) == 0){
		poll_data.voltage_max_value = get_signed_pseudo_float_3(poll_return+24);
		sscanf_P(poll_return+24,PSTR("%*s %*c %*s %u %3s %u %u:%u:%u"),&poll_data.voltage_max_date_y,&poll_data.voltage_max_date_m,&poll_data.voltage_max_date_d,&poll_data.voltage_max_time_h,&poll_data.voltage_max_time_m,&poll_data.voltage_max_time_s);
		poll_data.voltage_max_date_m[3]='\0';
	//	printf("Max Voltage: %ld.%03ld at %u %s %02u %02u:%02u:%02u\r\n",poll_data.voltage_max_value/1000, (poll_data.voltage_max_value <0 ) ? ((poll_data.voltage_max_value % 1000)*-1) : (poll_data.voltage_max_value %1000), poll_data.voltage_max_date_y, poll_data.voltage_max_date_m, poll_data.voltage_max_date_d,poll_data.voltage_max_time_h, poll_data.voltage_max_time_m, poll_data.voltage_max_time_s);

	}
	else if (strncmp_P(poll_return,PSTR("Highest RMS current"),19) == 0){
		poll_data.current_max_value = get_signed_pseudo_float_3(poll_return+24);

		sscanf_P(poll_return+24,PSTR("%*s %*c %*s %u %3s %u %u:%u:%u"),&poll_data.current_max_date_y,&poll_data.current_max_date_m,&poll_data.current_max_date_d,&poll_data.current_max_time_h,&poll_data.current_max_time_m,&poll_data.current_max_time_s);
		poll_data.current_max_date_m[3]='\0';
//		printf("Max Current: %ld.%03ld at %u %s %02u %02u:%02u:%02u\r\n",poll_data.current_max_value/1000, (poll_data.current_max_value <0 ) ? ((poll_data.current_max_value % 1000)*-1) : (poll_data.current_max_value %1000), poll_data.current_max_date_y, poll_data.current_max_date_m, poll_data.current_max_date_d,poll_data.current_max_time_h, poll_data.current_max_time_m, poll_data.current_max_time_s);

	}
	else if (strncmp_P(poll_return,PSTR("Highest wattage"),15) == 0){
		poll_data.watts_max_value = get_signed_pseudo_float_3(poll_return+20);
		sscanf_P(poll_return+20,PSTR("%*s %*c %*s %u %3s %u %u:%u:%u"),&poll_data.watts_max_date_y,&poll_data.watts_max_date_m,&poll_data.watts_max_date_d,&poll_data.watts_max_time_h,&poll_data.watts_max_time_m,&poll_data.watts_max_time_s);
		poll_data.watts_max_date_m[3]='\0';
//		printf("Max Wattage: %ld.%03ld at %u %s %02u %02u:%02u:%02u\r\n",poll_data.watts_max_value/1000, (poll_data.watts_max_value <0 ) ? ((poll_data.watts_max_value % 1000)*-1) : (poll_data.watts_max_value %1000), poll_data.watts_max_date_y, poll_data.watts_max_date_m, poll_data.watts_max_date_d,poll_data.watts_max_time_h, poll_data.watts_max_time_m, poll_data.watts_max_time_s);

	}
	else if (strncmp_P(poll_return,PSTR("Timers are currently enabled"),29) == 0){
		poll_data.activated = 1;
	}
	else if (strncmp_P(poll_return,PSTR("Timers are currently disabled"),30) == 0){
		poll_data.activated = 0;
	}

	else if (strncmp_P(poll_return,PSTR("Tarrif 0 Cost"),13) == 0){ //Tarrif is not a typo. Plogg returns Tarrif in this case
		poll_data.tariff0 = get_signed_pseudo_float_3(poll_return+16);
	}

	else if (strncmp_P(poll_return,PSTR("Tarrif 1 Cost"),13) == 0){//Tarrif is not a typo. Plogg returns Tarrif in this case
		poll_data.tariff1 = get_signed_pseudo_float_3(poll_return+16);
	}
	else if (strncmp_P(poll_return,PSTR("Current tarrif zone"),19)==0){
		sscanf_P(poll_return+22,PSTR("%d"),&poll_data.tariff_zone);
	}
	else if (strncmp_P(poll_return,PSTR("Tariff 0 from"),13) ==0){
		sscanf_P(poll_return+22,PSTR("%d%*c%d"),&poll_data.tariff0_start,&poll_data.tariff0_end);
	}
	else if (strncmp_P(poll_return,PSTR("Tariff0 :"),7)==0){
		poll_data.tariff0_consumed = get_unsigned_pseudo_float_3(poll_return+12);
		char* cost = strstr_P(poll_return,PSTR("Cost"));
		poll_data.tariff0_cost = get_unsigned_pseudo_float_3(cost+5);
	//	printf("Tariff 0: %lu.%03lukWh Cost: %lu.%02lu\r\n",poll_data.tariff0_consumed/1000, poll_data.tariff0_consumed % 1000,poll_data.tariff0_cost/1000, (poll_data.tariff0_cost % 1000) / 10 );

	}
	else if (strncmp_P(poll_return,PSTR("Tariff1 :"),7)==0){
		poll_data.tariff1_consumed = get_unsigned_pseudo_float_3(poll_return+12);
		char* cost = strstr_P(poll_return,PSTR("Cost"));
		poll_data.tariff1_cost = get_unsigned_pseudo_float_3(cost+5);
	}

}




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

		uint8_t length = strlen(poll_return);
		strcpy(poll_return+length,payload);
		while (strstr(poll_return,"~~") != NULL){
			char * start= strstr(poll_return,"~~");
			start[0]='\0';
			parse_Poll();
			uint8_t rest_length = strlen(start+2);
			memmove(poll_return,start+2,rest_length+1);
		}


	/*	// replace = ~~ with \r\n
		int idx = 0;
		while (payload[idx] != '\0')
		{
			if (payload[idx] == '~')
			{
				if (idx > 0 && payload[idx-1] == '\r')
				{
					payload[idx] = '\n';
				}
				else
				{
					payload[idx] = '\r';
				}
			}
			idx++;
		}
*/
		printf_P(PSTR("+UCAST:00\r\n%s\r\n"), AT_RESPONSE_OK);

//		telnet( payload );
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
  Led1_on(); // red

  etimer_set(&etimer, CLOCK_SECOND * 10);

  while (1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_TIMER) {
      etimer_reset(&etimer);
			if (poll_number == 0){
	      printf_P(PSTR("UCAST:0021ED000004699D=SV\r\n"));
			}
			else if (poll_number == 1){
	      printf_P(PSTR("UCAST:0021ED000004699D=SS\r\n"));
			}
			else if (poll_number == 2){
	      printf_P(PSTR("UCAST:0021ED000004699D=ST\r\n"));
			}
			else if (poll_number == 3){
	      printf_P(PSTR("UCAST:0021ED000004699D=SC\r\n"));
			}
			else if (poll_number == 4){
	      printf_P(PSTR("UCAST:0021ED000004699D=SM\r\n"));
			}
			else if (poll_number == 5){
	      printf_P(PSTR("UCAST:0021ED000004699D=SE\r\n"));
			}
			poll_number = (poll_number+1) % 6;

    } else if (ev == PROCESS_EVENT_MSG) {
      buf_pos = 0;
      while ((rx=ringbuf_get(&uart_buf))!=-1) {
        if (buf_pos<126 && (char)rx=='\r') {
          rx = ringbuf_get(&uart_buf);
          if ((char)rx=='\n') {
            buf[buf_pos] = '\0';
            //printf("%s\r\n", buf);
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
//          telnet("ERROR: RX buffer overflow\r\n");

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
RESOURCE(reset, METHOD_POST, "reset");

void
reset_handler(REQUEST* request, RESPONSE* response){

	char temp[50];
	char section[10];
	char success=1;

	if (rest_get_post_variable(request, "section", section, 10)){
		if (!strcmp_P(section, PSTR("cost"))){
			printf_P(PSTR("UCAST:0021ED000004699D=SC 1\r\n"));
		}
		else if(!strcmp_P(section, PSTR("max"))){
			printf_P(PSTR("UCAST:0021ED000004699D=SM 1\r\n"));
		}

		else if(!strcmp_P(section, PSTR("acc"))){
			printf_P(PSTR("UCAST:0021ED000004699D=SR\r\n"));
		}
		else if(!strcmp_P(section, PSTR("log"))){
			printf_P(PSTR("UCAST:0021ED000004699D=SX\r\n"));
		}
		else{
			success=0;
		}
	}
	else{
		success=0;
	}
  if(!success){
		sprintf_P(temp,PSTR("Section is missing\nTry section=[max,cost,acc]"));
  	rest_set_header_content_type(response, TEXT_PLAIN);
  	rest_set_response_payload(response, temp , strlen(temp));
 	 	rest_set_response_status(response, BAD_REQUEST_400);
	}
}

/**************************** Max Values *************************************/
RESOURCE(max, METHOD_GET, "max");
void
max_handler(REQUEST* request, RESPONSE* response)
{
	char type[10];
	char reset[5];
	char temp[100];
	char success=1;
	
	if (rest_get_query_variable(request, "type", type, 10)){
		if (!strcmp_P(type, PSTR("current"))){
			sprintf_P(temp,PSTR("Max Current: %ld.%03ld at %u %s %02u %02u:%02u:%02u\n"),poll_data.current_max_value/1000, (poll_data.current_max_value <0 ) ? ((poll_data.current_max_value % 1000)*-1) : (poll_data.current_max_value %1000), poll_data.current_max_date_y, poll_data.current_max_date_m, poll_data.current_max_date_d,poll_data.current_max_time_h, poll_data.current_max_time_m, poll_data.current_max_time_s);

		}
		else if (!strcmp_P(type, PSTR("voltage"))){
			sprintf_P(temp,PSTR("Max Voltage: %ld.%03ld at %u %s %02u %02u:%02u:%02u\n"),poll_data.voltage_max_value/1000, (poll_data.voltage_max_value <0 ) ? ((poll_data.voltage_max_value % 1000)*-1) : (poll_data.voltage_max_value %1000), poll_data.voltage_max_date_y, poll_data.voltage_max_date_m, poll_data.voltage_max_date_d,poll_data.voltage_max_time_h, poll_data.voltage_max_time_m, poll_data.voltage_max_time_s);
		}
		else if (!strcmp_P(type, PSTR("wattage"))){
			sprintf_P(temp,PSTR("Max Wattage: %ld.%03ld at %u %s %02u %02u:%02u:%02u\n"),poll_data.watts_max_value/1000, (poll_data.watts_max_value <0 ) ? ((poll_data.watts_max_value % 1000)*-1) : (poll_data.watts_max_value %1000), poll_data.watts_max_date_y, poll_data.watts_max_date_m, poll_data.watts_max_date_d,poll_data.watts_max_time_h, poll_data.watts_max_time_m, poll_data.watts_max_time_s);
		}
		else{
			success=0;
		}
	}
	else{
		success=0;
	}
	if(success){
  	rest_set_header_content_type(response, TEXT_PLAIN);
		rest_set_response_payload(response, temp , strlen(temp));
	}
	else{
		sprintf_P(temp,PSTR("Query variable missing or not known\nAppend ?type=[current,voltage,wattage] to url"));
  	rest_set_header_content_type(response, TEXT_PLAIN);
  	rest_set_response_payload(response, temp , strlen(temp));
 	 	rest_set_response_status(response, BAD_REQUEST_400);
	}
}

/************************* Enable/disable Timers ******************************/
RESOURCE(activate, METHOD_GET | METHOD_POST, "activate");
void
activate_handler(REQUEST* request, RESPONSE* response)
{
  char state[5];
  char success = 1;
	char temp[50];
	if (rest_get_method_type(request) == METHOD_GET){
		if (poll_data.activated){
	  	sprintf_P(temp,PSTR("Timers are enabled\n"));
		}
		else{
	  	sprintf_P(temp,PSTR("Timers are disabled\n"));
		}
  	rest_set_header_content_type(response, TEXT_PLAIN);
 		rest_set_response_payload(response, temp, strlen(temp));

	}
	else{
	  if (rest_get_post_variable(request, "state", state, 5)){
			if(!strcmp(state,"on")){
	 	    Led2_on();
				printf_P(PSTR("UCAST:0021ED000004699D=SE 1\r\n"));
			}
			else if(!strcmp(state, "off")){
	 	    Led2_off();
				printf_P(PSTR("UCAST:0021ED000004699D=SE 0\r\n"));
			}
 			else{
	 	   	success = 0;
			}
		}
		else{
			success=0;
 	 	}
 	 	if (!success){
 	 	  rest_set_response_status(response, BAD_REQUEST_400);
 	 	}
	}
}

/***************************** Date & Time ***********************************/
RESOURCE(clock, METHOD_GET, "clock");
void
clock_handler(REQUEST* request, RESPONSE* response){
	char temp[100];
	int index = 0;
  index += sprintf_P(temp + index, PSTR("</clock/time>,"));
  index += sprintf_P(temp + index, PSTR("</clock/date>"));
	
  rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
  rest_set_response_payload(response, temp , strlen(temp));
//	rest_set_response_status(response, CREATED_201);
}

RESOURCE(time, METHOD_GET | METHOD_POST, "clock/time");
void
time_handler(REQUEST* request, RESPONSE* response){

	char temp[100];
	char success = 1;
	char time[10];
	int hour, min;

	if (rest_get_method_type(request) == METHOD_GET){
		sprintf_P(temp,PSTR("Time: %02d:%02d:%02d\n"), poll_data.time_h,poll_data.time_m,poll_data.time_s);
  	rest_set_header_content_type(response, TEXT_PLAIN);
 		rest_set_response_payload(response, temp , strlen(temp));
	}
	else{
	  if (rest_get_post_variable(request, "value", time, 6)){
				hour = atoi(&time[0]);
				min = atoi(&time[3]);
				if (!(isdigit(time[0]) &&  isdigit(time[1]) && isdigit(time[3]) && isdigit(time[4]))){
					success = 0;
				}
				else if (!( 0<= hour && hour<=23 && 0<=min && min <=59)){
					success=0; 
				}
		}
		else{
				success=0;
		}

	 	if (success){
			printf_P(PSTR("UCAST:0021ED000004699D=rtt%02i.%02i.00\r\n"),hour,min);
		}
		else{
			sprintf_P(temp, PSTR("Excpected Format: value=hh:mm\n"));
  		rest_set_header_content_type(response, TEXT_PLAIN);
 			rest_set_response_payload(response, temp , strlen(temp));
 		  rest_set_response_status(response, BAD_REQUEST_400);
		}
	}
}


RESOURCE(date, METHOD_GET | METHOD_POST, "clock/date");
void
date_handler(REQUEST* request, RESPONSE* response){
		
	char temp[100];
	char success = 1;
	char date[10];
	int month, day, year;

	if (rest_get_method_type(request) == METHOD_GET){
		sprintf_P(temp, PSTR("Date: %02d %s %d\n"),poll_data.date_d,poll_data.date_m,poll_data.date_y);
  	rest_set_header_content_type(response, TEXT_PLAIN);
 		rest_set_response_payload(response, temp , strlen(temp));
	}
	else{
	  if (rest_get_post_variable(request, "value", date, 9)){
				day = atoi(&date[0]);
				month = atoi(&date[3]);
				year = atoi(&date[6]);
				if (!(isdigit(date[0]) &&  isdigit(date[1]) && isdigit(date[3]) && isdigit(date[4]) && isdigit(date[6]) && isdigit(date[7]))){
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
		}
		else{
				success=0;
		}

	 	if (success){
			printf_P(PSTR("UCAST:0021ED000004699D=rtd%02i.%02i.%02i\r\n"),year,month,day);
		}
		else{
			sprintf_P(temp, PSTR("Excpected Format: value=dd.mm.yy\n"));
  		rest_set_header_content_type(response, TEXT_PLAIN);
 			rest_set_response_payload(response, temp , strlen(temp));
 		  rest_set_response_status(response, BAD_REQUEST_400);
		}
	}
}



RESOURCE(discover, METHOD_GET, ".well-known/core");
void
discover_handler(REQUEST* request, RESPONSE* response){
	char temp[100];
  int index = 0;
  index += sprintf_P(temp + index, PSTR("</max>,"));
  index += sprintf_P(temp + index, PSTR("</activate>,"));
  index += sprintf_P(temp + index, PSTR("</clock>,"));
  index += sprintf_P(temp + index, PSTR("</reset>"));
  rest_set_response_payload(response, temp, strlen(temp));
  rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);

}

PROCESS_THREAD(coap_process, ev, data)
{
  PROCESS_BEGIN();

  rest_init();
  
  rest_activate_resource(&resource_clock);
  rest_activate_resource(&resource_time);
  rest_activate_resource(&resource_date);
	rest_activate_resource(&resource_activate);
  rest_activate_resource(&resource_discover);
  rest_activate_resource(&resource_max);
  rest_activate_resource(&resource_reset);

	strcpy_P(poll_return,PSTR("\0"));
	poll_data.activated=0;
 	poll_data.date_y=0;
	strcpy_P(poll_data.date_m,PSTR("\0"));
 	poll_data.date_d=0;
	poll_data.time_h=0;
	poll_data.time_m=0;
  poll_data.time_s=0;
	poll_data.watts_total=0;
	poll_data.watts_con=0;
	poll_data.watts_gen=0;
	poll_data.frequency=0;
	poll_data.current=0;
	poll_data.voltage=0;
	poll_data.power_total=0;
	poll_data.power_gen=0;
	poll_data.power_con=0;
	poll_data.phase_angle=0;
	poll_data.plogg_time_d=0;
	poll_data.plogg_time_h=0;
	poll_data.plogg_time_m=0;
	poll_data.plogg_time_s=0;
	poll_data.equipment_time_d=0;
	poll_data.equipment_time_h=0;
	poll_data.equipment_time_m=0;
	poll_data.equipment_time_s=0;

	poll_data.current_max_value=0;
	poll_data.current_max_date_y=0;
	strcpy_P(poll_data.current_max_date_m,PSTR("\0"));
 	poll_data.current_max_date_d=0;
	poll_data.current_max_time_h=0;
	poll_data.current_max_time_m=0;
	poll_data.current_max_time_s=0;

	poll_data.voltage_max_value=0;
 	poll_data.voltage_max_date_y=0;
	strcpy_P(poll_data.voltage_max_date_m,PSTR("\0"));
 	poll_data.voltage_max_date_d=0;
	poll_data.voltage_max_time_h=0;
	poll_data.voltage_max_time_m=0;
  poll_data.voltage_max_time_s=0;
	
	poll_data.watts_max_value=0;
 	poll_data.watts_max_date_y=0;
	strcpy_P(poll_data.watts_max_date_m,PSTR("\0"));
 	poll_data.watts_max_date_d=0;
	poll_data.watts_max_time_h=0;
	poll_data.watts_max_time_m=0;
  poll_data.watts_max_time_s=0;


  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&coap_process, &plogg_process);
/*---------------------------------------------------------------------------*/
