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

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

static char temp[103]; // ./well-known/core has longest payload 102 + 1 string delimiter

/* Resources are defined by RESOURCE macro, signature: resource name, the http methods it handles and its url*/
RESOURCE(helloworld, METHOD_GET, "hello");

/* For each resource defined, there corresponds an handler method which should be defined too.
 * Name of the handler method should be [resource name]_handler
 * */
void
helloworld_handler(REQUEST* request, RESPONSE* response)
{
  char len[4];
  int length = 12;
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

  rest_set_header_content_type(response, TEXT_PLAIN);
  rest_set_response_payload(response, temp, strlen(temp));
}

/* Resources are defined by RESOURCE macro, signature: resource name, the http methods it handles and its url*/
RESOURCE(mirror, METHOD_GET, "dbg");

/* For each resource defined, there corresponds an handler method which should be defined too.
 * Name of the handler method should be [resource name]_handler
 * */
void
mirror_handler(REQUEST* request, RESPONSE* response)
{
  int index = 0;

  content_type_t content = rest_get_header_content_type(request);
  /* the other header options  */
  uint32_t max_age = 0;
  const uint8_t *etag = NULL;
  int len = 0;
  const char *host = "";
  const char *location = "";
  uint32_t observe = 0;
  uint16_t token = 0;
  uint32_t block_num = 0;
  uint8_t block_more = 0;
  uint16_t block_size = 0;
  const char *query = "";

  index += sprintf(temp, "CT %u\n", content);
  if (rest_get_header_max_age(request, &max_age))
    index += sprintf(temp+index, "MA %lu\n", max_age);
  if ((len = rest_get_header_etag(request, &etag))) {
    int i = 0;
    /* readable print, use strncmp() */
    index += sprintf(temp+index, "ET 0x");
    while (i<len){
      index += sprintf(temp+index, "%02X", etag[i++]);
    }
    index += sprintf(temp+index, "\n");
  }
  if ((len = rest_get_header_host(request, &host)))
    index += sprintf(temp+index, "UH %.*s\n", len, host);
  if ((len = rest_get_header_location(request, &location)))
    index += sprintf(temp+index, "Lo %.*s\n", len, location);
  if (rest_get_header_observe(request, &observe))
    index += sprintf(temp+index, "Ob %lu\n", observe);
  if (rest_get_header_token(request, &token))
    index += sprintf(temp+index, "To 0x%X\n", token);
  if (rest_get_header_block(request, &block_num, &block_more, &block_size))
    index += sprintf(temp+index, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  if ((len = rest_get_query(request, &query)))
    index += sprintf(temp+index, "Qu %.*s", len, query);

  /* setting header options for response */
  uint8_t etag_res[] = {0xCB, 0xCD, 0xEF, 0x00}; // is copied
  static char host_res[] =  {'c','o','n','t','i','k','i',0}; // strings are not copied and should be static or in .text
  char fake[] = {'/','f','a','k','e',0}; // See what happens...

  rest_set_header_content_type(response, TEXT_PLAIN);
  rest_set_header_max_age(response, 10); // For HTTP the page will not be re-requested for 10 seconds
  rest_set_header_etag(response, etag_res);
  rest_set_header_host(response, host_res); // ensure the terminating 0 for strings
  rest_set_header_location(response, fake);
  rest_set_path(response, "/path/to/res"); // the leading / MUST be omitted and will be cropped by the setter function
  rest_set_header_observe(response, 10);
  rest_set_header_token(response, 0x0180);
  rest_set_header_block(response, 42, 0, 64);
  rest_set_query(response, "?l=1"); // the leading ? MUST be omitted and will be cropped by the setter function

  rest_set_response_payload(response, temp, strlen(temp));
}

RESOURCE(discover, METHOD_GET, ".well-known/core");
void
discover_handler(REQUEST* request, RESPONSE* response)
{
  /* </hello>;rt="Text",</dbg>;rt="Mirror",</led>;rt="Control",</light>;rt="LightSensor",</toggle>;rt="Led" */
  int index = 0;
  index += sprintf(temp + index, "%s", "</hello>;rt=\"Text\"");
  index += sprintf(temp + index, ",%s", "</dbg>;rt=\"Mirror\"");
#if defined (CONTIKI_TARGET_SKY)
  index += sprintf(temp + index, ",%s", "</led>;rt=\"Control\"");
  index += sprintf(temp + index, ",%s", "</light>;rt=\"LightSensor\"");
  index += sprintf(temp + index, ",%s", "</toggle>;rt=\"Led\"");
#endif /*defined (CONTIKI_TARGET_SKY)*/

  rest_set_response_payload(response, temp, strlen(temp));
  rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);
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

  if (!success) {
    rest_set_response_status(response, BAD_REQUEST_400);
  }
}

/*A simple getter example. Returns the reading from light sensor with a simple etag*/
RESOURCE(light, METHOD_GET, "light");
void
light_handler(REQUEST* request, RESPONSE* response)
{
  static uint8_t etag[] = {0xAB, 0xCD, 0};

  uint16_t light_photosynthetic;
  uint16_t light_solar;

  light_photosynthetic = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
  light_solar = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);

  if (rest_get_header_content_type(request)==TEXT_PLAIN || rest_get_header_content_type(request)==TEXT_HTML) {
    rest_set_header_content_type(response, TEXT_PLAIN);
    sprintf(temp,"%u;%u", light_photosynthetic, light_solar);

    rest_set_header_etag(response, etag);
    rest_set_response_payload(response, temp, strlen(temp));
  } else if (rest_get_header_content_type(request)==APPLICATION_JSON) {
    rest_set_header_content_type(response, APPLICATION_JSON);
    sprintf(temp,"{'light':{'photosynthetic':%u,'solar':%u}}", light_photosynthetic, light_solar);

    rest_set_header_etag(response, etag);
    rest_set_response_payload(response, temp, strlen(temp));
  } else {
    char *info = "Supporting content-types text/plain, text/html, and application/json";
    rest_set_response_status(response, UNSUPPORTED_MADIA_TYPE_415);
    rest_set_response_payload(response, info, strlen(info));
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
  rest_activate_resource(&resource_discover);

#if defined (CONTIKI_TARGET_SKY)
  SENSORS_ACTIVATE(light_sensor);
  rest_activate_resource(&resource_led);
  rest_activate_resource(&resource_light);
  rest_activate_resource(&resource_toggle);
#endif /*defined (CONTIKI_TARGET_SKY)*/

  PROCESS_END();
}
