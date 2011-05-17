#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

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

/*
 * Resources are defined by the RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash).
 */
RESOURCE(helloworld, METHOD_GET, "hello", "title=\"Hello world (set length with ?len query)\";rt=\"Text\"");

/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
void
helloworld_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t *payload;
  int length = 12; /* ------->| */
  char *message = "Hello World!";

  length = REST.get_request_payload(request, &payload);
  payload[length] = '\0';
  length = atoi(payload);

  if (length<0) length = 0;
  if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
//  memcpy(buffer, message, length);
  memset(buffer, 2, length);

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
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
