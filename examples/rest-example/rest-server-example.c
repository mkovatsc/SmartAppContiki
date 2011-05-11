#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#include "rest.h"

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
  /* response might be NULL for non-confirmable CoAP requests. */
  if (response)
  {
    const char *len = NULL;
    int length = 12; /* ------->| */
    char *message = "Hello World! ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789?!at 86 now+2+4at 99 now100..105..110..115..120..125..130..135..140..145..150..155..160";

    /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
    if (REST.get_query_variable(request, "len", &len)) {
      length = atoi(len);
      if (length<0) length = 0;
      if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
      memcpy(buffer, message, length);
    } else {
      memcpy(buffer, message, length);
    }

    REST.set_header_content_type(response, REST.type.TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
    REST.set_header_etag(response, (uint8_t *) &length, 1);
    REST.set_response_payload(response, buffer, length);
  }
}

/* This resource mirrors the incoming request. It shows how to access the options and how to set them for the response. */
RESOURCE(mirror, METHOD_GET | METHOD_POST | METHOD_PUT | METHOD_DELETE, "mirror", "title=\"Returns your decoded message\";rt=\"Debug\"");

void
mirror_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  if (response)
  {
    /* The ETag and Token is copied to the header. */
    uint8_t opaque[] = {0xCB, 0xCD, 0xEF};

    /* Strings are not copied and should be static or in program memory (char *str = "string in .text";).
     * They must be '\0'-terminated as the setters use strlen(). */
    static char location[] = {'/','f','a','k','e','?','q','=','0', 0};

    /* Getter for the header option Content-Type. If the option is not set, text/plain is returned by default. */
    unsigned int content_type = REST.get_header_content_type(request);

    /* The other getters copy the value (or string/array pointer) to the given pointers and return 1 for success or the length of strings/arrays. */
    uint32_t max_age = 0;
    const char *host = "";
    uint32_t observe = 0;
    const uint8_t *token = NULL;
    uint32_t block_num = 0;
    uint8_t block_more = 0;
    uint16_t block_size = 0;
    const char *query = "";
    int len = 0;

    /* Mirror the received header options in the response payload. Unsupported getters (e.g., rest_get_header_observe() with HTTP) will return 0. */

    int strpos = 0;
    /* snprintf() counts the terminating '\0' to the size parameter.
     * Add +1 to fill the complete buffer.
     * The additional byte is taken care of by allocating REST_MAX_CHUNK_SIZE+1 bytes in the REST framework. */
    strpos += snprintf((char *)buffer, REST_MAX_CHUNK_SIZE+1, "CT %u\n", content_type);

    /* Some getters such as for ETag or Location are omitted, as these options should not appear in a request.
     * Max-Age might appear in HTTP requests or used for special purposes in CoAP. */
    if (REST.get_header_max_age(request, &max_age))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "MA %lu\n", max_age);
    }
    if ((len = REST.get_header_host(request, &host)))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UH %.*s\n", len, host);
    }
#if WITH_COAP > 1
    if (coap_get_header_observe(request, &observe))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Ob %lu\n", observe);
    }
    if ((len = coap_get_header_token(request, &token)))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "To 0x");
      int index = 0;
      for (index = 0; index<len; ++index) {
          strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", token[index]);
      }
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
    }
#if WITH_COAP == 3
    if (coap_get_header_block(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
    }
#elif WITH_COAP == 6
    if (coap_get_header_block2(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
    }
    // TODO Block1
#endif

#endif
    if ((len = REST.get_query(request, &query)))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Qu %.*s\n", len, query);
    }
    if ((len = REST.get_request_payload(request, &token)))
    {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%.*s", len, token);
    }
    REST.set_response_payload(response, buffer, strpos);

    PRINTF("/mirror options received: %s\n", buffer);

    /* Set dummy header options for response. Like getters, some setters are not implemented for HTTP and have no effect. */
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_header_max_age(response, 10); /* For HTTP, browsers will not re-request the page for 10 seconds. CoAP action depends on the client. */
    REST.set_header_etag(response, opaque, 3);
    REST.set_header_location(response, location); /* Initial slash is omitted by framework */
#if WITH_COAP > 1
    coap_set_header_observe(response, 10);
    opaque[0] = 0x01;
    opaque[1] = 0xCC;
    coap_set_header_token(response, opaque, 2); /* If this function is not called, the Token is copied from the request by default. */
#if WITH_COAP == 3
    coap_set_header_block(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
#elif WITH_COAP == 6
    coap_set_header_block2(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
    // TODO Block1
#endif
#endif
  }
}

/*
 * For data larger than REST_MAX_CHUNK_SIZE (e.g., stored in flash) resources must be aware of the buffer limitation
 * and split their responses by themselves. To transfer the complete resource through a TCP stream or CoAP's blockwise transfer,
 * the byte offset where to continue is provided to the handler as int32_t pointer.
 * These chunk-wise resources must set the offset value to its new position or -1 of the end is reached.
 * (The offset for CoAP's blockwise transfer can go up to 2'147'481'600 = ~2047 M for block size 2048 (reduced to 1024 in observe-03.)
 */
RESOURCE(chunks, METHOD_GET, "chunks", "title=\"Blockwise demo\";rt=\"Data\"");

void
chunks_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
#define CHUNKS_TOTAL    1030

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    REST.set_response_payload(response, (uint8_t*)"Block out of scope", 18);
    return;
  }

  if (response)
  {
    int32_t strpos = 0;

    /* Generate data until reaching CHUNKS_TOTAL. */
    while (strpos<REST_MAX_CHUNK_SIZE) {
      strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "|%ld|", *offset);
    }
    /* Truncate if above. */
    if (*offset+strpos > CHUNKS_TOTAL)
    {
      strpos = CHUNKS_TOTAL - *offset;
    }

    REST.set_response_payload(response, buffer, strpos);

    /* Signal chunk awareness of resource to framework. */
    *offset += strpos;

    /* Signal end of resource. */
    if (*offset>=CHUNKS_TOTAL)
    {
      *offset = -1;
    }
  } /* if (response) */
}

/*
 * Example for a periodic resource.
 * It takes an additional period parameter, which defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions by managing a list of subscribers to notify.
 */
PERIODIC_RESOURCE(polling, METHOD_GET, "poll", "title=\"Periodic demo\";rt=\"Observable\"", 5*CLOCK_SECOND);

void
polling_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, (uint8_t *)"It's periodic!", 14);
  }

  /* A post_handler that handles subscriptions will be called for periodic resources by the REST framework. */
}

/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
int
polling_periodic_handler(resource_t *r)
{
  static uint32_t periodic_i = 0;
  static char content[16];

  PRINTF("TICK /%s\n", r->url);
  periodic_i = periodic_i + 1;

  // FIXME provide a rest_notify_subscribers call; how to manage specific options such as COAP_TYPE?
  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r->url, 1, periodic_i, (uint8_t *)content, snprintf(content, sizeof(content), "TICK %lu", periodic_i));

  return 1;
}

#if defined (CONTIKI_TARGET_SKY)
/*
 * Example for an event resource.
 * Additionally takes a period parameter that defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions and manages a list of subscribers to notify.
 */
EVENT_RESOURCE(event, METHOD_GET, "event", "title=\"Event demo\";rt=\"Observable\"");

void
event_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, (uint8_t *)"It's eventful!", 14);
  }

  /* A post_handler that handles subscriptions/observing will be called for periodic resources by the framework. */
}

/* Additionally, a handler function named [resource name]_event_handler must be implemented for each PERIODIC_RESOURCE defined.
 * It will be called by the REST manager process with the defined period. */
int
event_event_handler(resource_t *r)
{
  static uint32_t event_i = 0;
  static char content[10];

  PRINTF("EVENT /%s\n", r->url);
  ++event_i;

  /* Notify registered observers with the given message type, observe option, and payload.
   * The token will be set automatically. */

  // FIXME provide a rest_notify_subscribers call; how to manage specific options such as COAP_TYPE?
  REST.notify_subscribers(r->url, 0, event_i, content, snprintf(content, sizeof(content), "EVENT %lu", event_i));
  return 1;
}

/*A simple actuator example, depending on the color query parameter and post variable mode, corresponding led is activated or deactivated*/
RESOURCE(led, METHOD_POST | METHOD_PUT , "leds", "title=\"Led control (use ?color=red|green|blue and POST/PUT mode=on|off)\";rt=\"Control\"");

void
led_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *color = NULL;
  const char *mode = NULL;
  uint8_t led = 0;
  int success = 1;

  if ((len=REST.get_query_variable(request, "color", &color))) {
    PRINTF("color %.*s\n", len, color);

    if (strncmp(color, "red", len)==0) {
      led = LEDS_RED;
    } else if(strncmp(color,"green", len)==0) {
      led = LEDS_GREEN;
    } else if (strncmp(color,"blue", len)==0) {
      led = LEDS_BLUE;
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (success && (len=REST.get_post_variable(request, "mode", &mode))) {
    PRINTF("mode %s\n", mode);

    if (strncmp(mode, "on", len)==0) {
      leds_on(led);
    } else if (strncmp(mode, "off", len)==0) {
      leds_off(led);
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (!success && response) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}

/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(light, METHOD_GET, "light", "title=\"Photosynthetic and solar light (supports JSON)\";rt=\"LightSensor\"");
void
light_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  static uint8_t etag[] = {0xAB, 0xCD};

  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    uint16_t light_photosynthetic = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
    uint16_t light_solar = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);

    if (REST.get_header_content_type(request)==REST.type.TEXT_PLAIN || REST.get_header_content_type(request)==REST.type.TEXT_HTML) {
      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      snprintf(buffer, REST_MAX_CHUNK_SIZE, "%u;%u", light_photosynthetic, light_solar);

      REST.set_header_etag(response, etag, 2);
      REST.set_response_payload(response, (uint8_t *)buffer, strlen(buffer));
    } else if (REST.get_header_content_type(request)==REST.type.APPLICATION_JSON) {
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      snprintf(buffer, REST_MAX_CHUNK_SIZE, "{'light':{'photosynthetic':%u,'solar':%u}}", light_photosynthetic, light_solar);

      REST.set_header_etag(response, etag, 2);
      REST.set_response_payload(response, buffer, strlen(buffer));
    } else {
      char *info = "Supporting content-types text/plain, text/html, and application/json";
      REST.set_response_status(response, REST.status.UNSUPPORTED_MADIA_TYPE);
      REST.set_response_payload(response, (uint8_t *)info, strlen(info));
    }
  }
}

/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(battery, METHOD_GET, "battery", "title=\"Battery status\";rt=\"Battery\"");
void
battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  static uint8_t etag[] = {0xAB, 0xCD};

  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    int battery = battery_sensor.value(0);

    if (REST.get_header_content_type(request)==REST.type.TEXT_PLAIN || REST.get_header_content_type(request)==REST.type.TEXT_HTML) {
      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      snprintf(buffer, REST_MAX_CHUNK_SIZE, "%d", battery);

      REST.set_header_etag(response, etag, 2);
      REST.set_response_payload(response, (uint8_t *)buffer, strlen(buffer));
    } else if (REST.get_header_content_type(request)==REST.type.APPLICATION_JSON) {
      REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
      snprintf(buffer, REST_MAX_CHUNK_SIZE, "{'battery':%d}", battery);

      REST.set_header_etag(response, etag, 2);
      REST.set_response_payload(response, buffer, strlen(buffer));
    } else {
      char *info = "Supporting content-types text/plain, text/html, and application/json";
      REST.set_response_status(response, REST.status.UNSUPPORTED_MADIA_TYPE);
      REST.set_response_payload(response, (uint8_t *)info, strlen(info));
    }
  }
}

/* A simple actuator example. Toggles the red led */
RESOURCE(toggle, METHOD_GET | METHOD_PUT | METHOD_POST, "toggle", "title=\"Red LED\";rt=\"Control\"");
void
toggle_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  leds_toggle(LEDS_RED);
}
#endif /*defined (CONTIKI_TARGET_SKY)*/


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

  /* Initialize the REST framework. */
  rest_init_framework();

  /* Activate the application-specific resources. */
  rest_activate_resource(&resource_helloworld);
  rest_activate_resource(&resource_mirror);
  rest_activate_resource(&resource_chunks);
  rest_activate_periodic_resource(&periodic_resource_polling);

#if defined (CONTIKI_TARGET_SKY)
  SENSORS_ACTIVATE(light_sensor);
  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(battery_sensor);

  rest_activate_resource(&resource_event);
  rest_activate_resource(&resource_led);
  rest_activate_resource(&resource_light);
  rest_activate_resource(&resource_battery);
  rest_activate_resource(&resource_toggle);
#endif /*defined (CONTIKI_TARGET_SKY)*/

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
#if defined (CONTIKI_TARGET_SKY)
    if (ev == sensors_event && data == &button_sensor) {
      PRINTF("BUTTON\n");
      /* Call the event_handler for this application-specific event. */
      event_event_handler(&resource_event);
    }
#endif
  } /* while (1) */

  PROCESS_END();
}
