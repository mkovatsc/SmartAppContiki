#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#include "coap-06.h"
#include "rest-engine.h"

#if defined (CONTIKI_TARGET_SKY) /* Any other targets will be added here (&& defined (OTHER))*/
#include "dev/light-sensor.h"
#include "dev/button-sensor.h"
#include "dev/battery-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/leds.h"
#endif /*defined (CONTIKI_TARGET_SKY)*/


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
 * Example for a periodic resource.
 * It takes an additional period parameter, which defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions by managing a list of subscribers to notify.
 */
PERIODIC_RESOURCE(polling, METHOD_GET, "poll", "title=\"Periodic\";rt=\"Observable\"", 5*CLOCK_SECOND);

void
polling_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, (uint8_t *)"It's periodic!", 14);

  /* A post_handler that handles subscriptions will be called for periodic resources by the REST framework. */
}

/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
int
polling_periodic_handler(resource_t *r)
{
coap_stack_dump("polling_periodic_handler");
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
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, (uint8_t *)"It's eventful!", 14);

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
coap_stack_dump("led_handler");
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

  if (!success) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}

/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(light, METHOD_GET, "light", "title=\"Photosynthetic and solar light\";rt=\"LightSensor\"");
void
light_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
coap_stack_dump("light_handler");
  static uint8_t etag[] = {0xAB, 0xCD};

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

/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(battery, METHOD_GET, "battery", "title=\"Battery status\";rt=\"Battery\"");
void
battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  static uint8_t etag[] = {0xAB, 0xCD};

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
#endif /*defined (CONTIKI_TARGET_SKY)*/


PROCESS(rest_server_example, "Rest Server Example");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();
coap_stack_dump("example PROCESS");
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

/* if static routes are used rather than RPL */
#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
  set_global_address();
  configure_routing();
#endif

  /* Initialize the REST framework. */
  rest_init_framework();

  /* Activate the application-specific resources. */
  rest_activate_periodic_resource(&periodic_resource_polling);

#if defined (CONTIKI_TARGET_SKY)
  SENSORS_ACTIVATE(light_sensor);
  SENSORS_ACTIVATE(button_sensor);
  SENSORS_ACTIVATE(battery_sensor);

  rest_activate_event_resource(&resource_event);
  rest_activate_resource(&resource_led);
  rest_activate_resource(&resource_light);
  rest_activate_resource(&resource_battery);
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
