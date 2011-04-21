#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest.h"

#if defined (CONTIKI_TARGET_SKY) /* Any other targets will be added here (&& defined (OTHER))*/
#include "dev/light-sensor.h"
#include "dev/battery-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/leds.h"
#endif /*defined (CONTIKI_TARGET_SKY)*/

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

static char temp[103]; // ./well-known/core has longest payload 102 + 1 string delimiter


/* Resources are defined by RESOURCE macro. Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash) */
RESOURCE(mirror, METHOD_GET, "dbg");

/* To each defined resource corresponds a handler function named [resource name]_handler. */
void
mirror_handler(REQUEST* request, RESPONSE* response)
{
  int strpos = 0;

  /* Getter for the header option Content-Type. If the option is not set, text/plain is returned by default. */
  content_type_t content_type = rest_get_header_content_type(request);

  /* The other getters copy the value to the given pointers and return 1 for success or the length of strings/arrays. */
  uint32_t max_age = 0;
  const char *host = "";
  uint32_t observe = 0;
  uint16_t token = 0;
  uint32_t block_num = 0;
  uint8_t block_more = 0;
  uint16_t block_size = 0;
  const char *query = "";
  int len = 0;

  /* Mirror the received header options in the response payload. Unsupported getters (e.g., observe with HTTP) will return 0. */
  strpos += sprintf(temp, "CT %u\n", content_type);
  if (rest_get_header_max_age(request, &max_age))
    strpos += sprintf(temp+strpos, "MA %lu\n", max_age);
  if ((len = rest_get_header_host(request, &host)))
    strpos += sprintf(temp+strpos, "UH %.*s\n", len, host);
  if (rest_get_header_observe(request, &observe))
    strpos += sprintf(temp+strpos, "Ob %lu\n", observe);
  if (rest_get_header_token(request, &token))
    strpos += sprintf(temp+strpos, "To 0x%X\n", token);
  if (rest_get_header_block(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
    strpos += sprintf(temp+strpos, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  if ((len = rest_get_query(request, &query)))
    strpos += sprintf(temp+strpos, "Qu %.*s", len, query);

  PRINTF("/dbg options received: %s\n", temp);

  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    uint8_t etag_res[] = {0xCB, 0xCD, 0xEF}; /* The ETag is copied */
    static char fake[] = {'/','f','a','k','e', 0}; /* Strings are not copied and should be static or in program memory (char *str = "string in .text";). */

    /* setting header options for response */
    rest_set_header_content_type(response, TEXT_PLAIN);
    rest_set_header_max_age(response, 10); /* For HTTP, browsers will not re-request the page for 10 seconds. */
    rest_set_header_etag(response, etag_res, 3);
    rest_set_header_location(response, fake);
    rest_set_header_observe(response, 10);
    rest_set_header_token(response, 0x0180); /* If this function is not called, the Token is copied from the request by default. */
    rest_set_header_block(response, 42, 0, 64); /* See BLOCKWISE_RESOURCE. */

    rest_set_response_payload(response, (uint8_t *)temp, strpos);
  }
}

/* When using CoAP, BLOCKWISE_RESOURCEs will call coap_default_block_handler()
 * after the [resource name]_handler() function and crop the payload into the requested block. */
BLOCKWISE_RESOURCE(helloworld, METHOD_GET, "hello");

/* For each resource defined, there corresponds an handler function named [resource name]_handler. */
void
helloworld_handler(REQUEST* request, RESPONSE* response)
{
  char len[4];
  int length = 12; /* ------->| */
  char *message = "Hello World! ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789?!at 86 now+2+4at 99 now";

  if (rest_get_query_variable(request, "len", len, 4)) {
    length = atoi(len);
    if (length<0) length = 0;
    if (length>99) length = 99;
    memcpy(temp, message, length);
    temp[length] = '\0';
  } else {
    memcpy(temp, message, length);
    temp[length] = '\0';
  }

  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    rest_set_header_etag(response, (uint8_t *) &length, 1);
    rest_set_header_content_type(response, TEXT_PLAIN);
    rest_set_response_payload(response, (uint8_t *)temp, length);
  }
}

PERIODIC_RESOURCE(polling, METHOD_GET, "poll", 5*CLOCK_SECOND);

static uint16_t periodic_token = 0;
static uip_ipaddr_t periodic_addr = {{0}};
static uint16_t periodic_port = 0;
static uint32_t periodic_i = 0;

void
polling_handler(REQUEST* request, RESPONSE* response)
{
  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    rest_set_header_content_type(response, TEXT_PLAIN);
    rest_set_response_payload(response, (uint8_t *)"It's periodic!", 14);

    uint32_t observe = 0;
    if (rest_get_header_observe(request, &observe))
    {
      if (rest_get_header_token(request, &periodic_token))
      {
        uip_ipaddr_copy(&periodic_addr, &((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])->srcipaddr);
        periodic_port = ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])->srcport;

        periodic_i = 0;
        coap_set_header_observe(response, periodic_i);
      }
      else /* if (token) */
      {
        rest_set_response_status(response, TOKEN_OPTION_REQUIRED);
        rest_set_response_payload(response, (uint8_t *)"Observing requires token", 24);

        periodic_token = 0;
      } /* if (token) */
    }
    else /* if (observe) */
    {
      periodic_token = 0;
    } /* if (observe) */
  }
}

int
polling_periodic_handler(resource_t *r)
{
  PRINTF("TICK %s\n", r->url);

  if (periodic_token)
  {
    coap_packet_t push[1];
    coap_init_message(push, (uint8_t *)temp, COAP_TYPE_CON, OK_200, random_rand());
    coap_set_header_observe(push, ++periodic_i);
    coap_set_header_token(push, periodic_token);
    coap_set_payload(push, (uint8_t *)"TICK", 4);

    PRINTF("Sending TICK %u to ", periodic_i);
    PRINT6ADDR(&periodic_addr);
    PRINTF(":%u\n", periodic_port);
    coap_send_message(&periodic_addr, periodic_port, push->header, coap_serialize_message(push));
  }

  return 1;
}


/* The discover resource should be included when using CoAP. */
RESOURCE(discover, METHOD_GET, ".well-known/core");
void
discover_handler(REQUEST* request, RESPONSE* response)
{
  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    /* </hello>;rt="Text",</dbg>;rt="Mirror",</led>;rt="Control",</light>;rt="LightSensor",</toggle>;rt="Led" */
    int strpos = 0;

    strpos += sprintf(temp + strpos, "%s", "</hello>;rt=\"Text\"");
    strpos += sprintf(temp + strpos, ",%s", "</dbg>;rt=\"Mirror\"");
  #if defined (CONTIKI_TARGET_SKY)
    strpos += sprintf(temp + strpos, ",%s", "</led>;rt=\"Control\"");
    strpos += sprintf(temp + strpos, ",%s", "</light>;rt=\"LightSensor\"");
    strpos += sprintf(temp + strpos, ",%s", "</toggle>;rt=\"Led\"");
  #endif /*defined (CONTIKI_TARGET_SKY)*/

    rest_set_response_payload(response, (uint8_t *)temp, strpos);
    rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
  }
}

#if defined (CONTIKI_TARGET_SKY)
/*A simple actuator example, depending on the color query parameter and post variable mode, corresponding led is activated or deactivated*/
RESOURCE(led, METHOD_POST | METHOD_PUT , "led");

void
led_handler(REQUEST* request, RESPONSE* response)
{
  char color[10];
  char mode[10];
  uint8_t led = 0;
  int success = 1;

  if (rest_get_query_variable(request, "color", color, 10)) {
    PRINTF("color %s\n", color);

    if (!strcmp(color,"red")) {
      led = LEDS_RED;
    } else if(!strcmp(color,"green")) {
      led = LEDS_GREEN;
    } else if ( !strcmp(color,"blue") ) {
      led = LEDS_BLUE;
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (success && rest_get_post_variable(request, "mode", mode, 10)) {
    PRINTF("mode %s\n", mode);

    if (!strcmp(mode, "on")) {
      leds_on(led);
    } else if (!strcmp(mode, "off")) {
      leds_off(led);
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (!success && response) {
    rest_set_response_status(response, BAD_REQUEST_400);
  }
}

/*A simple getter example. Returns the reading from light sensor with a simple etag*/
RESOURCE(light, METHOD_GET, "light");
void
light_handler(REQUEST* request, RESPONSE* response)
{
  static uint8_t etag[] = {0xAB, 0xCD};

  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {

    uint16_t light_photosynthetic;
    uint16_t light_solar;

    light_photosynthetic = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
    light_solar = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);

    if (rest_get_header_content_type(request)==TEXT_PLAIN || rest_get_header_content_type(request)==TEXT_HTML) {
      rest_set_header_content_type(response, TEXT_PLAIN);
      sprintf(temp,"%u;%u", light_photosynthetic, light_solar);

      rest_set_header_etag(response, etag, 2);
      rest_set_response_payload(response, (uint8_t *)temp, strlen(temp));
    } else if (rest_get_header_content_type(request)==APPLICATION_JSON) {
      rest_set_header_content_type(response, APPLICATION_JSON);
      sprintf(temp,"{'light':{'photosynthetic':%u,'solar':%u}}", light_photosynthetic, light_solar);

      rest_set_header_etag(response, etag, 2);
      rest_set_response_payload(response, (uint8_t *)temp, strlen(temp));
    } else {
      char *info = "Supporting content-types text/plain, text/html, and application/json";
      rest_set_response_status(response, UNSUPPORTED_MADIA_TYPE_415);
      rest_set_response_payload(response, (uint8_t *)info, strlen(info));
    }
  }
}

/*A simple actuator example. Toggles the red led*/
RESOURCE(toggle, METHOD_GET | METHOD_PUT | METHOD_POST, "toggle");
void
toggle_handler(REQUEST* request, RESPONSE* response)
{
  leds_toggle(LEDS_RED);
}
#endif /*defined (CONTIKI_TARGET_SKY)*/


PROCESS(rest_server_example, "Rest Server Example");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("Rest Server Example\n");

#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef DIEEE802154_CONF_PANID
  PRINTF("PAN ID: 0x%04X\n", DIEEE802154_CONF_PANID);
#endif

  PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  PRINTF("LL header: %u\n", UIP_LLH_LEN);
  PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
#if WITH_COAP == 3
  PRINTF("CoAP transactions: %u\n", COAP_MAX_OPEN_TRANSACTIONS);
  PRINTF("CoAP max packet: %u\n", COAP_MAX_PACKET_SIZE);
  PRINTF("CoAP max payload: %u\n", COAP_MAX_PAYLOAD_SIZE);
#endif

  rest_init();

  rest_activate_resource(&resource_helloworld);
  rest_activate_resource(&resource_mirror);
  rest_activate_periodic_resource(&periodic_resource_polling);
  rest_activate_resource(&resource_discover);

#if defined (CONTIKI_TARGET_SKY)
  SENSORS_ACTIVATE(light_sensor);
  rest_activate_resource(&resource_led);
  rest_activate_resource(&resource_light);
  rest_activate_resource(&resource_toggle);
#endif /*defined (CONTIKI_TARGET_SKY)*/

  PROCESS_END();
}
