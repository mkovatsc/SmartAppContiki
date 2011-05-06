#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "contiki-net.h"

#ifdef CONTIKI_TARGET_SKY /* Any other targets will be added here (&& defined (OTHER))*/
#include "dev/button-sensor.h"
#include "dev/battery-sensor.h"
#endif

#include "coap-03.h"
#include "coap-transactions.h"

#define TOGGLE_INTERVAL 10


#define DEBUG 1
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

#define SERVER_NODE(ipaddr)   uip_ip6addr(ipaddr, 0xaaaa, 0, 0, 0, 0x0212, 0x7400, 0x0da0, 0xd748)
#define LOCAL_PORT      UIP_HTONS(61617)
#define REMOTE_PORT     UIP_HTONS(61616)

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

int current_tid = 0;
static uip_ipaddr_t server_ipaddr;
static struct etimer et;

#define NUMBER_OF_URLS 4
char* service_urls[NUMBER_OF_URLS] = {".well-known/core", "toggle", "battery", "poll"};

static void
send_data(void)
{
  coap_transaction_t *transaction = NULL;

  if ( (transaction = coap_new_transaction(current_tid++, &server_ipaddr, REMOTE_PORT)) )
  {

    /* prepare response */
    coap_packet_t request[1]; /* This way the packet can be treated as pointer as usual. */
    coap_init_message(request, transaction->packet, COAP_TYPE_CON, COAP_GET, transaction->tid );
    coap_set_header_uri_path(request, service_urls[1]);
    coap_set_payload(request, (uint8_t *)"Toggling...", 11);
    transaction->packet_len = coap_serialize_message(request);

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
        coap_cancel_transaction_by_tid(response->tid);
      }
      else if (response->type==COAP_TYPE_RST)
      {
        PRINTF("Received RST %u\n", response->tid);
        coap_cancel_transaction_by_tid(response->tid);
      }
      else if (response->type==COAP_TYPE_CON)
      {
        /* reuse input buffer */
        coap_init_message(response, response->header, COAP_TYPE_ACK, 0, response->tid);
        coap_send_message(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, response->header, coap_serialize_message(response));
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

  current_tid = random_rand();

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
      send_data();
#endif
    }
  }

  PROCESS_END();
}
