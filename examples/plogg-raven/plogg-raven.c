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

PROCESS(coap_process, "Coap Handler");
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

static struct {
	char activated;

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

static void parse_payload(char* payload)
{
	


}

static void ATInterpreterProcessCommand(char* command)
{
	// HACK: dummy commands we need to accept
	if ((strcmp(command, "ATS01=31f4") == 0) ||
			(strcmp(command, "ATS00=0001") == 0) ||
			(strcmp(command, "at+dassl") == 0))
	{
		printf("%s\r\n", AT_RESPONSE_OK);
	}
	// we need to process the join command
	else if (strcmp(command, "at+jn") == 0)
	{
		printf("JPAN:11,31F4\r\n");
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
		printf("Resetting...\r\n%s\r\n", AT_RESPONSE_OK);
	}
	// check if we have a unicast command
	else if (strncmp(command, AT_COMMAND_UNICAST, strlen(AT_COMMAND_UNICAST)) == 0)
	{
		// we need to parse the remaining part
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

		// replace = ~~ with \r\n
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

		printf("+UCAST:00\r\n%s\r\n", AT_RESPONSE_OK);

		// now we can parse payload and store it
//		telnet( payload );
		
		// HOST is waiting for "ACK:00" or NACK
		printf("ACK:00\r\n");
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
  char buf[255];

  PROCESS_BEGIN();

  ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
  rs232_set_input(RS232_PORT_0, uart_get_char);
  Led1_on(); // red

  etimer_set(&etimer, CLOCK_SECOND * 10);

  while (1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_TIMER) {
     // etimer_reset(&etimer);
      printf("UCAST:0021ED000004699D=SV\r\n");
    } else if (ev == PROCESS_EVENT_MSG) {
      buf_pos = 0;
      while ((rx=ringbuf_get(&uart_buf))!=-1) {
        if (buf_pos<254 && (char)rx=='\r') {
          rx = ringbuf_get(&uart_buf);
          if ((char)rx=='\n') {
            buf[buf_pos] = '\0';
       //     printf("%s\r\n", buf);
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
        if (buf_pos==255) {
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

char temp[100];

/* Resources are defined by RESOURCE macro, signature: resource name, the http methods it handles and its url*/
RESOURCE(hello, METHOD_GET, "hello");

/* For each resource defined, there corresponds an handler method which should be defined too.
 * Name of the handler method should be [resource name]_handler
 * */
void
hello_handler(REQUEST* request, RESPONSE* response)
{
  //sprintf(temp,"Two things are infinite: the universe and human stupidity; and I'm not sure about the the universe.\n");
  sprintf(temp,"Hello World\n");
	
  rest_set_header_content_type(response, TEXT_PLAIN);
  rest_set_response_payload(response, temp , strlen(temp));
}

/*A simple actuator example*/
RESOURCE(activate, METHOD_GET | METHOD_POST, "activate");
void
activate_handler(REQUEST* request, RESPONSE* response)
{
  char state[5];
  char success = 1;
	if (rest_get_method_type(request) == METHOD_GET){
		if (poll_data.activated){
	  	sprintf(temp,"Timers are enabled\n");
		}
		else{
	  	sprintf(temp,"Timers are disabled\n");
		}
  	rest_set_header_content_type(response, TEXT_PLAIN);
 		rest_set_response_payload(response, temp , strlen(temp));

	}
	else{
	  if (rest_get_post_variable(request, "state", state, 5)){
			if(!strcmp(state,"on")){
	 	    Led2_on();
				printf_P(PSTR("UCAST:0021ED000004699D=SE 1\r\n"));
				poll_data.activated=1;
			}
			else if(!strcmp(state, "off")){
	 	    Led2_off();
				printf_P(PSTR("UCAST:0021ED000004699D=SE 0\r\n"));
				poll_data.activated=0;
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

/****************Date & Time******************************/
RESOURCE(clock, METHOD_GET, "clock");
void
clock_handler(REQUEST* request, RESPONSE* response){
	int index = 0;
  index += sprintf(temp + index, "%s,", "</clock/time>;rt=\"Time\"");
  index += sprintf(temp + index, "%s", "</clock/date>;rt=\"Date\"");
	
  rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
  rest_set_response_payload(response, temp , strlen(temp));
//	rest_set_response_status(response, CREATED_201);
}

RESOURCE(time, METHOD_GET | METHOD_POST, "clock/time");
void
time_handler(REQUEST* request, RESPONSE* response){
	
	char success = 1;
	int index = 0;
	char time[10];
	int hour, min;

	if (rest_get_method_type(request) == METHOD_GET){
		success = 0;
		sprintf_P(temp + index, PSTR("Not yet implemented\n"));
  	rest_set_header_content_type(response, TEXT_PLAIN);
 		rest_set_response_payload(response, temp , strlen(temp));
 		rest_set_response_status(response, BAD_REQUEST_400);
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
			printf("UCAST:0021ED000004699D=rtt%02i.%02i.00\r\n",hour,min);
		}
		else{
			sprintf_P(temp + index, PSTR("Excpected Format: value=hh:mm\n"));
  		rest_set_header_content_type(response, TEXT_PLAIN);
 			rest_set_response_payload(response, temp , strlen(temp));
 		  rest_set_response_status(response, BAD_REQUEST_400);
		}
	}
}


RESOURCE(date, METHOD_GET | METHOD_POST, "clock/date");
void
date_handler(REQUEST* request, RESPONSE* response){
	
	char success = 1;
	int index = 0;
	char date[10];
	int month, day, year;

	if (rest_get_method_type(request) == METHOD_GET){
		success = 0;
		sprintf_P(temp + index, PSTR("Not yet implemented\n"));
  	rest_set_header_content_type(response, TEXT_PLAIN);
 		rest_set_response_payload(response, temp , strlen(temp));
 		rest_set_response_status(response, BAD_REQUEST_400);
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
			printf("UCAST:0021ED000004699D=rtd%02i.%02i.%02i\r\n",year,month,day);
		}
		else{
			sprintf_P(temp + index, PSTR("Excpected Format: value=dd.mm.yy\n"));
  		rest_set_header_content_type(response, TEXT_PLAIN);
 			rest_set_response_payload(response, temp , strlen(temp));
 		  rest_set_response_status(response, BAD_REQUEST_400);
		}
	}
}



RESOURCE(discover, METHOD_GET, ".well-known/core");
void
discover_handler(REQUEST* request, RESPONSE* response){
  int index = 0;
  index += sprintf(temp + index, "%s,", "</activate>;rt=\"Activate\"");
  index += sprintf(temp + index, "%s,", "</clock>;rt=\"Clock\"");
  index += sprintf(temp + index, "%s", "</dummy>;rt=\"Dummy\"");
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

	//initialize poll_data struct
	poll_data.activated=0;

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&coap_process, &plogg_process);
/*---------------------------------------------------------------------------*/
