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
#include <avr/eeprom.h>
#include "contiki-raven.h"
#include "rs232.h"
#include "ringbuf.h"

/*---------------------------------------------------------------------------*/
PROCESS(plogg_process, "Plogg comm");
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
#define TELNETD_CONF_LINELEN 40
#endif
#ifndef TELNETD_CONF_NUMLINES
#define TELNETD_CONF_NUMLINES 10
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
static char state = 0;

static void telnet(char *str);

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
		printf("I am an AVR Raven Jackdaw pretending to be an ETRX2 Zigbee module.\r\n%s\r\n", AT_RESPONSE_OK);
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

		// now we can send the payload to the client
		telnet( payload );

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
  int buf_pos;
  char buf[128];

  PROCESS_BEGIN();

  ringbuf_init(&uart_buf, uart_buf_data, sizeof(uart_buf_data));
  rs232_set_input(RS232_PORT_0, uart_get_char);
  Led1_on(); // red

  //etimer_set(&etimer, CLOCK_SECOND * 10);

  while (1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_TIMER) {
      etimer_reset(&etimer);
      telnet("INFO: polling...\r\nPLOGG> ");
      printf("UCAST:0021ED000004699D=SV\r\n");
    } else if (ev == PROCESS_EVENT_MSG) {
      buf_pos = 0;
      while ((rx=ringbuf_get(&uart_buf))!=-1) {
        if (buf_pos<126 && (char)rx=='\r') {
          rx = ringbuf_get(&uart_buf);
          if ((char)rx=='\n') {
            buf[buf_pos] = '\0';
            printf("%s\r\n", buf);
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
          telnet("ERROR: RX buffer overflow\r\n");
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
static void
buf_init(struct telnetd_buf *buf)
{
  buf->ptr = 0;
  buf->size = sizeof(buf->bufmem); // rows * cols
}
/*---------------------------------------------------------------------------*/
static int
buf_append(struct telnetd_buf *buf, const char *data, int len)
{
  int copylen;

  copylen = MIN(len, buf->size - buf->ptr);
  memcpy(&buf->bufmem[buf->ptr], data, copylen);
  buf->ptr += copylen;

  return copylen;
}
/*---------------------------------------------------------------------------*/
static void
buf_copyto(struct telnetd_buf *buf, char *to, int len)
{
  memcpy(to, &buf->bufmem[0], len);
}
/*---------------------------------------------------------------------------*/
static void
buf_pop(struct telnetd_buf *buf, int len)
{
  int poplen;

  poplen = MIN(len, buf->ptr);
  memcpy(&buf->bufmem[0], &buf->bufmem[poplen], buf->ptr - poplen);
  buf->ptr -= poplen;
}
/*---------------------------------------------------------------------------*/
static int
buf_len(struct telnetd_buf *buf)
{
  return buf->ptr;
}

/*---------------------------------------------------------------------------*/
static void
telnetd_get_char(u8_t c)
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
    if (s.continued == 0) {
      printf("UCAST:0021ED000004699D=");
    }
    printf("%s\r\n", s.buf);
    s.continued = 0;
    telnet("PLOGG> ");
    s.bufptr = 0;
  } else if (s.bufptr == sizeof(s.buf)-1) {
    s.buf[(int)s.bufptr] = 0;
    if (s.continued == 0) {
      printf("UCAST:0021ED000004699D=");
    }
    printf(s.buf);
    s.continued = 1;
    s.bufptr = 0;
  }
}
/*---------------------------------------------------------------------------*/
static void
sendopt(u8_t option, u8_t value)
{
  char line[4];
  line[0] = (char)TELNET_IAC;
  line[1] = option;
  line[2] = value;
  line[3] = 0;
  buf_append(&send_buf, line, 4);
}
/*---------------------------------------------------------------------------*/
static void
newdata(void)
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
static void
closed(void)
{
}
/*---------------------------------------------------------------------------*/
static void
acked(void)
{
  buf_pop(&send_buf, s.numsent);
}
/*---------------------------------------------------------------------------*/
static void
senddata(void)
{
  int len;
  len = MIN(buf_len(&send_buf), uip_mss());
  buf_copyto(&send_buf, uip_appdata, len);
  uip_send(uip_appdata, len);
  s.numsent = len;
}
/*---------------------------------------------------------------------------*/
void
telnetd_appcall(void *ts)
{
  if(uip_connected()) {
    tcp_markconn(uip_conn, &s);
    buf_init(&send_buf);
    s.bufptr = 0;
    s.continued = 0;
    s.state = STATE_NORMAL;
    telnet("PLOGG> ");
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
void
telnetd_quit(void)
{
  process_exit(&telnetd_process);
  LOADER_UNLOAD();
}
/*---------------------------------------------------------------------------*/
void
telnet(char *str)
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

/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&telnetd_process, &plogg_process);
/*---------------------------------------------------------------------------*/

