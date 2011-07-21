/*
 * Copyright (c) 2011, Matthias Kovatsch and other contributors.
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
 */

/**
 * \file
 *      CoAP client example
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"

#ifdef CONTIKI_TARGET_SKY /* Any other targets will be added here (&& defined (OTHER))*/
#include "dev/button-sensor.h"
#include "dev/battery-sensor.h"
#endif

#if WITH_COAP == 3
#include "coap-03.h"
#include "coap-03-transactions.h"
#elif WITH_COAP == 6
#include "coap-06.h"
#include "coap-06-transactions.h"
#elif WITH_COAP == 7
#include "coap-07.h"
#include "coap-07-transactions.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

#define TOGGLE_INTERVAL 10


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xfe80, 0, 0, 0, 0x0212, 0x7402, 0x0002, 0x0202) /* cooja2 */

#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT+1)
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)

static uip_ipaddr_t server_ipaddr;
static struct etimer et;

#define NUMBER_OF_URLS 6
char* service_urls[NUMBER_OF_URLS] = {".well-known/core", "/toggle", "battery/", "poll/me/not", "error/in//path", "//more/errors//"};

static int uri_switch = 0;

static void
send_data(void)
{
  coap_transaction_t *transaction = NULL;

  if ( (transaction = coap_new_transaction(coap_get_tid(), &server_ipaddr, REMOTE_PORT)) )
  {

    /* prepare response */
    coap_packet_t request[1]; /* This way the packet can be treated as pointer as usual. */
    coap_init_message(request, COAP_TYPE_CON, COAP_GET, transaction->tid );
    coap_set_header_uri_path(request, service_urls[uri_switch]);
    coap_set_payload(request, (uint8_t *)"GETting URL...", 14);
    transaction->packet_len = coap_serialize_message(request, transaction->packet);

    PRINTF("Sending to /%.*s\n", request->uri_path_len, request->uri_path);
    PRINTF("  %.*s\n", request->payload_len, request->payload);

    coap_send_transaction(transaction);
  }
}

static void
handle_incoming_data()
{
  int error = NO_ERROR;

  PRINTF("handle_incoming_data(): received uip_datalen=%u \n",(uint16_t)uip_datalen());

  uint8_t *data = uip_appdata + uip_ext_len;
  uint16_t data_len = uip_datalen() - uip_ext_len;

  if (uip_newdata()) {
    PRINTF("receiving UDP datagram from: ");
    PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
    PRINTF(":%u\n  Length: %u\n", uip_ntohs(UIP_UDP_BUF->srcport), data_len );

    coap_packet_t response[1] = {{0}};

    error = coap_parse_message(response, data, data_len);

    if (error==NO_ERROR)
    {
      if (response->type==COAP_TYPE_ACK)
      {
        PRINTF("Received ACK %u\n", response->tid);
        if (response->payload_len)
        {
          PRINTF("  %.*s\n", response->payload_len, response->payload);
        }
        coap_clear_transaction(coap_get_transaction_by_tid(response->tid));
      }
      else if (response->type==COAP_TYPE_RST)
      {
        PRINTF("Received RST %u\n", response->tid);
        coap_clear_transaction(coap_get_transaction_by_tid(response->tid));
      }
      else if (response->type==COAP_TYPE_CON)
      {
        /* reuse input buffer */
        coap_init_message(response, COAP_TYPE_ACK, 0, response->tid);
        coap_send_message(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, response->buffer, coap_serialize_message(response, response->buffer));
      }
    } /* if (parsed correctly) */
  }
}

PROCESS(coap_client_example, "COAP Client Example");
AUTOSTART_PROCESSES(&coap_client_example);

PROCESS_THREAD(coap_client_example, ev, data)
{
  PROCESS_BEGIN();

  SERVER_NODE(&server_ipaddr);

#ifdef CONTIKI_TARGET_SKY
  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(battery_sensor);
#endif

  /* retransmission timers will be set for this process. */
  coap_register_as_transaction_handler();

  coap_init_connection(LOCAL_PORT);

  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);

  while(1) {
    PROCESS_YIELD();
    if (etimer_expired(&et)) {
      send_data();
      etimer_reset(&et);
    } else if (ev == tcpip_event) {
      handle_incoming_data();
    } else if (ev == PROCESS_EVENT_TIMER) {
      /* retransmissions are handled here */
      coap_check_transactions();
#if defined (CONTIKI_TARGET_SKY)
    } else if (ev == sensors_event && data == &button_sensor) {
        uri_switch = (uri_switch+1) % NUMBER_OF_URLS;
      send_data();
#endif
    }
  }

  PROCESS_END();
}
