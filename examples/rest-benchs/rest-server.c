#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
#include "static-routing.h"
#endif

#include "dev/serial-line.h"
#include "dev/uart1.h"
#include "serial-shell.h"
#include "shell-sky.h"
#include "shell-powertrace.h"

#include "powertrace.h"

#include "rest-engine.h"

#if defined (CONTIKI_TARGET_SKY) /* Any other targets will be added here (&& defined (OTHER))*/
#include "dev/light-sensor.h"
#include "dev/button-sensor.h"
#include "dev/battery-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/leds.h"
#endif /*defined (CONTIKI_TARGET_SKY)*/

#if WITH_COAP == 3
#include "coap-03.h"
#elif WITH_COAP == 6
#include "coap-06.h"
#include "coap-06-observing.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/* Payload bench resource */
RESOURCE(helloworld, METHOD_GET | METHOD_POST, "hello", "title=\"Hello world (set length with ?len query)\";rt=\"Text\"");

void
helloworld_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t *payload;
  int length = 0;

  length = REST.get_request_payload(request, &payload);
  payload[length] = '\0';
  length = atoi(payload);

  if (length<0) length = 0;
  if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;

  memset(buffer, '-', length);

  REST.set_response_payload(response, buffer, length);
}

/* Separate bench resource */
PERIODIC_RESOURCE(separate, METHOD_GET | METHOD_POST, "separate", "title=\"Post processing time\"", 0);

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

struct process *process_separate_hack = NULL;

static uip_ipaddr_t sep_addr;
static uint16_t sep_port = 0;
static size_t sep_token_len = 0;
static uint8_t sep_token[8] ={0};
static uint16_t sep_tid = 0;
static coap_message_type_t sep_type;

void
separate_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t *payload;
  int length = 0;
  const char *query;

  static uint8_t ack_buffer[32];

  if (sep_port==0)
  {

    length = REST.get_request_payload(request, &payload);
    payload[length] = '\0';
    periodic_resource_separate.period = atoi(payload) * CLOCK_SECOND / 10;

    PRINTF("Separate after %lu\n", periodic_resource_separate.period);

    // start timeout
    // FIXME Hack, maybe there is a better way, but which is still lighter than posting everything to the process
    struct process *process_actual = PROCESS_CURRENT();
    process_current = process_separate_hack;
    PRINTF("Timer process: %p\n", process_current);
    etimer_set(&periodic_resource_separate.periodic_timer, periodic_resource_separate.period);
    process_current = process_actual;

    PRINTF("Handling process for %s: %p\n", resource_separate.url, process_current);

    // save client as temporary observer
    uip_ipaddr_copy(&sep_addr, &UIP_IP_BUF->srcipaddr);
    sep_port = UIP_UDP_BUF->srcport;
    sep_tid = ((coap_packet_t *)request)->tid;

    sep_token_len = ((coap_packet_t *)request)->token_len;
    memcpy(sep_token, ((coap_packet_t *)request)->token, sep_token_len);

    // abort response by setting coap_error_code
    coap_error_code = SKIP_RESPONSE;

    PRINTF("Waiting for token %u\n", sep_token_len);

    if (coap_get_query_variable(request, "sep", &query) && query[0]=='1')
    {

      PRINTF("WITH ACK\n");
      /* send separate ACK. */
      coap_packet_t ack[1];
      coap_init_message(ack, COAP_TYPE_ACK, 0, ((coap_packet_t *)request)->tid);
      coap_send_message(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, ack_buffer, coap_serialize_message(ack, ack_buffer));

      sep_type = COAP_TYPE_CON;
    }
    else
    {
      PRINTF("NO ACK\n");
      sep_type = COAP_TYPE_ACK;
    }

  }
  else
  {
    PRINTF("Dropping duplicate\n");
    coap_error_code = SKIP_RESPONSE;
  }
}

int
separate_periodic_handler(resource_t *r)
{
  PRINTF("Separate timeout as %u with token %u\n", sep_type, sep_token_len);

  static uint8_t sep_buffer[32];

  // send response
  coap_packet_t response[1];
  coap_init_message(response, sep_type, REST.status.OK, sep_type==COAP_TYPE_ACK ? sep_tid : coap_get_tid());
  if (sep_token_len) coap_set_header_token(response, sep_token, sep_token_len);
  coap_set_payload(response, (uint8_t *) "Separate", 8);

  if (sep_type==COAP_TYPE_ACK)
  {
    coap_send_message(&sep_addr, sep_port, sep_buffer, coap_serialize_message(response, sep_buffer));
  }
  else
  {
    coap_transaction_t *transaction = NULL;
    if ( (transaction = coap_new_transaction(response->tid, &sep_addr, sep_port)) )
    {
      if ((transaction->packet_len = coap_serialize_message(response, transaction->packet)))
      {
        coap_send_transaction(transaction);
      }
      else
      {
        coap_clear_transaction(transaction);
      }
    }
  }

  sep_port = 0;

  /* do not restart timer by returning 0 */
  return 0;
}

/*---------------------------------------------------------------------------*/
static void nothing() {}
/*---------------------------------------------------------------------------*/
const struct uip_fallback_interface rpl_interface = {
  nothing, nothing
};

PROCESS(rest_server_example, "Rest Server Example");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("Rest Example\n");

#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif

  PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  PRINTF("LL header: %u\n", UIP_LLH_LEN);
  PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
  PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);
#if WITH_COAP == 3
  PRINTF("CoAP max packet: %u\n", COAP_MAX_PACKET_SIZE);
  PRINTF("CoAP transactions: %u\n", COAP_MAX_OPEN_TRANSACTIONS);
#endif

#if !WITH_RPL
  configure_routing();
#endif

  /* Initialize the REST framework. */
  rest_init_framework();

  /* Activate the application-specific resources. */
  rest_activate_resource(&resource_helloworld);
  rest_activate_periodic_resource(&periodic_resource_separate);

//  powertrace_start(CLOCK_SECOND * 4);
//  powertrace_sniff(POWERTRACE_ON);

  uart1_set_input(serial_line_input_byte);
  serial_line_init();
  serial_shell_init();
//  shell_sky_init();
  shell_powertrace_init();

  PROCESS_WAIT_UNTIL(0);

  PROCESS_END();
}
