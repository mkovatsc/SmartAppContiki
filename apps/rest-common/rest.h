#ifndef REST_H_
#define REST_H_

/*includes*/
#include "contiki.h"
#include "contiki-lib.h"

/*
 * The maximum buffer size that is provided for resource responses and must be respected due to the limited IP buffer.
 * Larger data must be handled by the resource and will be sent chunk-wise through a TCP stream or CoAP blocks.
 */
#ifndef REST_MAX_CHUNK_SIZE
#define REST_MAX_CHUNK_SIZE     128
#endif

/*REST method types*/
typedef enum {
  METHOD_GET = (1 << 0),
  METHOD_POST = (1 << 1),
  METHOD_PUT = (1 << 2),
  METHOD_DELETE = (1 << 3)
} rest_method_t;




#ifdef WITH_COAP
#include "coap-03.h"
#define REQUEST         coap_packet_t
#define RESPONSE        coap_packet_t
#else
#define REQUEST         http_request_t
#define RESPONSE        http_response_t
#endif

typedef int (* service_callback_t)(REQUEST *request, RESPONSE *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/**
 * The structure of a MAC protocol driver in Contiki.
 */
struct rest_implementation {
  char *name;

  /** Initialize the REST implementation. */
  void (* init)(void);

  /** Register the RESTful service callback at implementation */
  void (* set_service_callback)(service_callback_t callback);

  /** Get the method of a request. */
  rest_method_t (* get_method_type)(REQUEST *request);

  /** Set the status code of a response. */
  void (* set_response_status)(RESPONSE *response, rest_status_t code);

  /** Get the content-type of a request. */
  rest_content_type_t (* get_header_content_type)(REQUEST *request);

  /** Set the content-type of a response. */
  int (* set_header_content_type)(RESPONSE *response, rest_content_type_t);

  /** Get the Max-Age option of a request. */
  int (* get_header_max_age)(REQUEST *request, uint32_t *age);

  /** Set the Max-Age option of a response. */
  int (* set_header_max_age)(RESPONSE *response, uint32_t age);

  /** Set the ETag option of a response. */
  int (* set_header_etag)(RESPONSE *response, uint8_t *etag, int length);

  /** Get the Host option of a request. */
  int (* get_header_host)(REQUEST *request, const char **host);

  /** Set the location option of a response. */
  int (* set_header_location)(RESPONSE *response, char *location);

  /** Get the payload option of a request. */
  int (* get_request_payload)(REQUEST *request, uint8_t **payload);

  /** Set the payload option of a response. */
  int (* set_response_payload)(RESPONSE *response, uint8_t *payload, int length);

  /** Get the query string of a request. */
  int (* get_query)(REQUEST *request, const char **value);

  /** Get the value of a request query key-value pair. */
  int (* get_query_variable)(REQUEST *request, const char *name, const char **value);

  /** Get the value of a request POST key-value pair. */
  int (* get_post_variable)(REQUEST *request, const char *name, const char **value);

  /** Send the payload to all subscribers of the resource at url. */
  void (* notify_subscribers)(const char *url, int, uint32_t observe, uint8_t *payload, uint16_t payload_len);

  /** The handler for resource subscriptions. */
  void (* subscription_handler)(REQUEST *request, RESPONSE *response);
};

struct resource_s;
struct periodic_resource_s;



#ifndef MIN
#define MIN(a, b) ((a) < (b)? (a) : (b))
#endif /* MIN */





/*Signature of handler functions*/
typedef void (*restful_handler) (REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
typedef int (*restful_pre_handler) (REQUEST* request, RESPONSE* response);
typedef void (*restful_post_handler) (REQUEST* request, RESPONSE* response);

typedef int (*restful_periodic_handler) (struct resource_s* resource);

/*
 * Data structure representing a resource in REST.
 */
struct resource_s {
  struct resource_s *next; /* for LIST, points to next resource defined */
  rest_method_t methods_to_handle; /* handled RESTful methods */
  const char* url; /*handled URL*/
  const char* attributes; /* link-format attributes; can be omitted for HTTP */
  restful_handler handler; /* handler function */
  restful_pre_handler pre_handler; /* to be called before handler, may perform initializations */
  restful_post_handler post_handler; /* to be called after handler, may perform finalizations (cleanup, etc) */
  void* user_data; /* pointer to user specific data */
};
typedef struct resource_s resource_t;

struct periodic_resource_s {
  struct periodic_resource_s *next; /* for LIST, points to next resource defined */
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
#define RESOURCE(name, methods_to_handle, url, attributes) \
void name##_handler(REQUEST *, RESPONSE *, uint8_t *, uint16_t, int32_t *); \
resource_t resource_##name = {NULL, methods_to_handle, url, attributes, name##_handler, NULL, NULL, NULL}

/*
 * Macro to define a periodic resource
 * The corresponding [name]_periodic_handler() function will be called every period.
 * For instance polling a sensor and publishing a changed value to subscribed clients would be done there.
 * The subscriber list will be maintained by the post_handler rest_subscription_handler() (see rest-mapping header file).
 */
#define PERIODIC_RESOURCE(name, methods_to_handle, url, attributes, period) \
void name##_handler(REQUEST *, RESPONSE *, uint8_t *, uint16_t, int32_t *); \
resource_t resource_##name = {NULL, methods_to_handle, url, attributes, name##_handler, NULL, NULL, NULL}; \
int name##_periodic_handler(resource_t*); \
periodic_resource_t periodic_resource_##name = {NULL, &resource_##name, period, {{0}}, name##_periodic_handler}

/*
 * Macro to define an event resource
 * Like periodic resources, event resources have a post_handler that manages a subscriber list.
 * Instead of a periodic_handler, an event_callback must be provided.
 */
#define EVENT_RESOURCE(name, methods_to_handle, url, attributes) \
void name##_handler(REQUEST *, RESPONSE *, uint8_t *, uint16_t, int32_t *); \
resource_t resource_##name = {NULL, methods_to_handle, url, attributes, name##_handler, NULL, NULL, NULL}; \
int name##_event_handler(resource_t*)



extern const struct rest_implementation coap_rest_implementation;

/*
 * Initializes REST framework and starts HTTP or COAP process
 */
void rest_init_framework(void);

/*
 * Resources wanted to be accessible should be activated with the following code.
 */
void rest_activate_resource(resource_t* resource);

void rest_activate_periodic_resource(periodic_resource_t* periodic_resource);

/*
 * To be called by HTTP/COAP server as a callback function when a new service request appears.
 * This function dispatches the corresponding RESTful service.
 */
int rest_invoke_restful_service(REQUEST* request, RESPONSE* response, uint8_t *buffer, uint16_t buffer_size, int32_t *offset);

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
