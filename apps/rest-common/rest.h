#ifndef REST_H_
#define REST_H_

/*includes*/
#include "contiki.h"
#include "contiki-lib.h"

struct resource_s;
struct periodic_resource_s;

#ifndef REST_MAX_CHUNK_SIZE
#define REST_MAX_CHUNK_SIZE     128
#endif

#ifdef WITH_COAP
  #include "coap-server.h"
  #define REQUEST coap_packet_t
  #define RESPONSE coap_packet_t
  #define SERVER_PROCESS (&coap_server)

  #include "coap-rest-mapping.h"

#else /*WITH_COAP*/
  /*WITH_HTTP*/
  #include "http-common.h"
  #include "http-server.h"
  #define REQUEST http_request_t
  #define RESPONSE http_response_t
  #define SERVER_PROCESS (&http_server)

  char *rest_to_http_max_age(uint32_t age);
  char *rest_to_http_etag(uint8_t *etag, uint8_t etag_len);

  #include "http-rest-mapping.h"

#endif /*WITH_COAP*/

/*REST method types*/
typedef enum {
  METHOD_GET = (1 << 0),
  METHOD_POST = (1 << 1),
  METHOD_PUT = (1 << 2),
  METHOD_DELETE = (1 << 3)
} method_t;


/*Signature of handler functions*/
typedef void (*restful_handler) (REQUEST* request, RESPONSE* response, int32_t *offset, uint8_t *buffer, uint16_t buffer_size);
typedef int (*restful_pre_handler) (REQUEST* request, RESPONSE* response);
typedef void (*restful_post_handler) (REQUEST* request, RESPONSE* response);

typedef int (*restful_periodic_handler) (struct resource_s* resource);

/*
 * Data structure representing a resource in REST.
 */
struct resource_s {
  struct resource_s *next; /*for LIST, points to next resource defined*/
  method_t methods_to_handle; /*handled HTTP methods*/
  const char* url; /*handled URL*/
  restful_handler handler; /*handler function*/
  restful_pre_handler pre_handler; /*to be called before handler, may perform initializations*/
  restful_post_handler post_handler; /*to be called after handler, may perform finalizations (cleanup, etc)*/
  void* user_data; /*pointer to user specific data*/
};
typedef struct resource_s resource_t;

struct periodic_resource_s {
  struct periodic_resource_s *next; /*for LIST, points to next resource defined*/
  resource_t *resource;
  uint32_t period;
  struct etimer periodic_timer;
  restful_periodic_handler periodic_handler;
};
typedef struct periodic_resource_s periodic_resource_t;

/*
 * Macro to define a Resource
 * Resources are statically defined for the sake of efficiency and better memory management.
 */
#define RESOURCE(name, methods_to_handle, url) \
void name##_handler(REQUEST*, RESPONSE*, int32_t*, uint8_t*, uint16_t); \
resource_t resource_##name = {NULL, methods_to_handle, url, name##_handler, NULL, NULL, NULL}

/*
 * Macro to define a Periodic Resource
 * The corresponding _periodic_handler() function will be called every period.
 * There one can define what to do, e.g., poll a sensor and publish a changed value to observing clients.
 */
#define PERIODIC_RESOURCE(name, methods_to_handle, url, period) \
RESOURCE(name, methods_to_handle, url); \
int name##_periodic_handler(resource_t*); \
periodic_resource_t periodic_resource_##name = {NULL, &resource_##name, period, {{0}}, name##_periodic_handler}

/*
 * Initializes REST framework and starts HTTP or COAP process
 */
void rest_init(void);

/*
 * Resources wanted to be accessible should be activated with the following code.
 */
void rest_activate_resource(resource_t* resource);

void rest_activate_periodic_resource(periodic_resource_t* periodic_resource);

/*
 * To be called by HTTP/COAP server as a callback function when a new service request appears.
 * This function dispatches the corresponding RESTful service.
 */
int rest_invoke_restful_service(REQUEST* request, RESPONSE* response, int32_t *offset, uint8_t *buffer, uint16_t buffer_size);

/*
 * Returns the resource list
 */
list_t rest_get_resources(void);

/*
 * Getter method for user specific data.
 */
void* rest_get_user_data(resource_t* resource);

/*
 * Setter method for user specific data.
 */
void rest_set_user_data(resource_t* resource, void* user_data);

/*
 * Sets the pre handler function of the Resource.
 * If set, this function will be called just before the original handler function.
 * Can be used to setup work before resource handling.
 */
void rest_set_pre_handler(resource_t* resource, restful_pre_handler pre_handler);

/*
 * Sets the post handler function of the Resource.
 * If set, this function will be called just after the original handler function.
 * Can be used to do cleanup (deallocate memory, etc) after resource handling.
 */
void rest_set_post_handler(resource_t* resource, restful_post_handler post_handler);

#endif /*REST_H_*/
