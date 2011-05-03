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

/* The REST framework automatically converts the data to the requested blocks for CoAP.
 * For large data, the byte offset is provided to the handler as int32_t pointer and must
 * and chunk-wise resources must set its value to the new value or -1 of the end is reached.
 * The offset for CoAP's blockwise transfer can go up to 2'147'481'600 = ~2047 M.
 */

/* Resources are defined by RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash). */
RESOURCE(helloworld, METHOD_GET, "hello", "title=\"Hello world (set length with ?len query)\";rt=\"Text\"");

/* A handler function named [resource name]_handler must be implemented for each RESOURCE defined. */
void
helloworld_handler(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  char len[4];
  int length = 12; /* ------->| */
  char *message = "Hello World! ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789?!at 86 now+2+4at 99 now100..105..110..115..120..125..130..135..140..145..150..155..160";

  if (rest_get_query_variable(request, "len", len, 4)) {
    length = atoi(len);
    if (length<0) length = 0;
    if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  }

  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
      /*
      if (block_offset > response->payload_len)
                    {
                      coap_set_code(response, BAD_REQUEST_400);
                      coap_set_payload(response, (uint8_t*)"Block out of scope", 18);
                    }
      */

    rest_set_header_content_type(response, TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
    rest_set_header_etag(response, (uint8_t *) &length, 1);
    rest_set_response_payload(response, buffer, length);
  }
}


RESOURCE(mirror, METHOD_GET, "mirror", "title=\"Returns your decoded message\";rt=\"Debug\"");

/* To each defined resource corresponds a handler function named [resource name]_handler. */
void
mirror_handler(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    uint8_t etag_res[] = {0xCB, 0xCD, 0xEF}; /* The ETag is copied to the header. */
    static char location[] = {'/','f','a','k','e', 0}; /* Strings are not copied and should be static or in program memory (char *str = "string in .text";). */

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

    /* Mirror the received header options in the response payload. Unsupported getters (e.g., rest_get_header_observe() with HTTP) will return 0. */

    /* snprintf() counts the terminating '\0' to the size parameter.
     * Add +1 to fill the complete buffer.
     * The additional byte is taken care of in coap_transaction_t. */
    strpos += snprintf((char *)buffer, REST_MAX_CHUNK_SIZE+1, "CT %u\n", content_type);

    if (rest_get_header_max_age(request, &max_age))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "MA %lu\n", max_age);
    }
    if ((len = rest_get_header_host(request, &host)))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UH %.*s\n", len, host);
    }
    if (rest_get_header_observe(request, &observe))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Ob %lu\n", observe);
    }
    if (rest_get_header_token(request, &token))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "To 0x%X\n", token);
    }
    if (rest_get_header_block(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
    }
    if ((len = rest_get_query(request, &query)))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Qu %.*s", len, query);
    }
    rest_set_response_payload(response, buffer, strpos);

    PRINTF("/dbg options received: %s\n", buffer);

    /* Setting dummy header options for response. */
    rest_set_header_content_type(response, TEXT_PLAIN);
    rest_set_header_max_age(response, 10); /* For HTTP, browsers will not re-request the page for 10 seconds. CoAP action depends on the client. */
    rest_set_header_etag(response, etag_res, 3);
    rest_set_header_location(response, location); /* Initial slash is omitted by framework */
    rest_set_header_observe(response, 10);
    rest_set_header_token(response, 0x0180); /* If this function is not called, the Token is copied from the request by default. */
    rest_set_header_block(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */

  }
}

/* The REST framework automatically converts the data to the requested blocks for CoAP.
 * For large data, the byte offset is provided to the handler as int32_t pointer and must
 * and chunk-wise resources must set its value to the new value or -1 of the end is reached.
 * The offset for CoAP's blockwise transfer can go up to 2'147'481'600 = ~2047 M.
 */
RESOURCE(chunks, METHOD_GET, "chunks", "title=\"Blockwise demo\";rt=\"Data\"");
#define CHUNKS_TOTAL    1030

void
chunks_handler(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

  /* check boundaries of this resource */
  if (*offset>=CHUNKS_TOTAL)
  {
    coap_set_code(response, BAD_REQUEST_400);
    coap_set_payload(response, (uint8_t*)"Block out of scope", 18);
    return;
  }

  if (response)
  {
    int32_t strpos = 0;

    /* Generate data until reaching CHUNKS_TOTAL. */
    while (strpos<REST_MAX_CHUNK_SIZE) {
      strpos += snprintf(buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "|%ld|", *offset);
    }
    /* Truncate if above. */
    if (*offset+strpos > CHUNKS_TOTAL)
    {
      strpos = CHUNKS_TOTAL - *offset;
    }

    rest_set_response_payload(response, buffer, strpos);

    /* Signal chunk awareness of resource. */
    *offset += strpos;

    /* Signal end of chunks. */
    if (*offset>=CHUNKS_TOTAL)
    {
      *offset = -1;
    }
  } /* if (response) */
}

PERIODIC_RESOURCE(polling, METHOD_GET, "poll", "title=\"Periodic demo\";rt=\"Observable\"", 5*CLOCK_SECOND);

static uint32_t periodic_i = 0;

void
polling_handler(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    uint32_t observe = 0;
    uint16_t token = 0;

    rest_set_header_content_type(response, TEXT_PLAIN);
    rest_set_response_payload(response, (uint8_t *)"It's periodic!", 14);

    if (rest_get_header_observe(request, &observe))
    {
      if (rest_get_header_token(request, &token))
      {
        if (coap_add_observer(request->url, &((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])->srcipaddr, ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])->srcport, token))
        {
          periodic_i = 0;
          coap_set_header_observe(response, periodic_i);
        }
        else
        {
          rest_set_response_status(response, SERVICE_UNAVAILABLE_503);
          rest_set_response_payload(response, (uint8_t *)"Too many observers", 18);
        }
      }
      else /* if (token) */
      {
        rest_set_response_status(response, TOKEN_OPTION_REQUIRED);
        rest_set_response_payload(response, (uint8_t *)"Observing requires token", 24);
      } /* if (token) */
    }
    else /* if (observe) */
    {
      /* Remove client */
      coap_remove_observer_by_client(&((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])->srcipaddr, ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])->srcport);
    } /* if (observe) */
  }
}

int
polling_periodic_handler(resource_t *r)
{
  PRINTF("TICK %s\n", r->url);
  coap_notify_observers(r->url, ++periodic_i, (uint8_t *)"TICK", 4);
  return 1;
}

#if defined (CONTIKI_TARGET_SKY)
/*A simple actuator example, depending on the color query parameter and post variable mode, corresponding led is activated or deactivated*/
RESOURCE(led, METHOD_POST | METHOD_PUT , "leds", "title=\"Led control (use ?color=red|green|blue and POST/PUT mode=on|off)\";rt=\"Control\"");

void
led_handler(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
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

/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(light, METHOD_GET, "light", "title=\"Photosynthetic and solar light (supports JSON)\";rt=\"LightSensor\"");
void
light_handler(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
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
      snprintf(buffer, REST_MAX_CHUNK_SIZE, "%u;%u", light_photosynthetic, light_solar);

      rest_set_header_etag(response, etag, 2);
      rest_set_response_payload(response, (uint8_t *)buffer, strlen(buffer));
    } else if (rest_get_header_content_type(request)==APPLICATION_JSON) {
      rest_set_header_content_type(response, APPLICATION_JSON);
      snprintf(buffer, REST_MAX_CHUNK_SIZE, "{'light':{'photosynthetic':%u,'solar':%u}}", light_photosynthetic, light_solar);

      rest_set_header_etag(response, etag, 2);
      rest_set_response_payload(response, buffer, strlen(buffer));
    } else {
      char *info = "Supporting content-types text/plain, text/html, and application/json";
      rest_set_response_status(response, UNSUPPORTED_MADIA_TYPE_415);
      rest_set_response_payload(response, (uint8_t *)info, strlen(info));
    }
  }
}

/* A simple actuator example. Toggles the red led */
RESOURCE(toggle, METHOD_GET | METHOD_PUT | METHOD_POST, "toggle", "title=\"Red LED\";rt=\"Control\"");
void
toggle_handler(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
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
  PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);
#if WITH_COAP == 3
  PRINTF("CoAP max packet: %u\n", COAP_MAX_PACKET_SIZE);
  PRINTF("CoAP transactions: %u\n", COAP_MAX_OPEN_TRANSACTIONS);
#endif

  rest_init();

  rest_activate_resource(&resource_helloworld);
  rest_activate_resource(&resource_mirror);
  rest_activate_resource(&resource_chunks);
  rest_activate_periodic_resource(&periodic_resource_polling);

#if defined (CONTIKI_TARGET_SKY)
  SENSORS_ACTIVATE(light_sensor);
  rest_activate_resource(&resource_led);
  rest_activate_resource(&resource_light);
  rest_activate_resource(&resource_toggle);
#endif /*defined (CONTIKI_TARGET_SKY)*/

  PROCESS_END();
}
