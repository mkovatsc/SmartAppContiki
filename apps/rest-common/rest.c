#include "contiki.h"
#include <string.h> /*for string operations in match_addresses*/
#include "rest.h"
#include "buffer.h"

#define DEBUG 0
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

/*FIXME it is possible to define some of the rest functions as MACROs rather than functions full of ifdefs.*/

LIST(restful_services);

void
rest_init(void)
{
  list_init(restful_services);

  set_service_callback(rest_invoke_restful_service);

  /*Start rest server process*/
  process_start(SERVER_PROCESS, NULL);
}

void
rest_activate_resource(resource_t* resource)
{
  /*add it to the restful web service link list*/
  list_add(restful_services, resource);
}

void
rest_activate_periodic_resource(periodic_resource_t* periodic_resource)
{
#ifdef WITH_COAP
  coap_activate_periodic_resource(periodic_resource);
#endif /*WITH_COAP*/
  rest_activate_resource(periodic_resource->resource);
}

void
rest_set_user_data(resource_t* resource, void* user_data)
{
  resource->user_data = user_data;
}

void*
rest_get_user_data(resource_t* resource)
{
  return resource->user_data;
}

void
rest_set_pre_handler(resource_t* resource, restful_pre_handler pre_handler)
{
  resource->pre_handler = pre_handler;
}

void
rest_set_post_handler(resource_t* resource, restful_post_handler post_handler)
{
  resource->post_handler = post_handler;
}

list_t
rest_get_resources(void)
{
  return restful_services;
}

void
rest_set_response_status(RESPONSE* response, status_code_t status)
{
#ifdef WITH_COAP
  coap_set_code(response, status);
#else /*WITH_COAP*/
  http_set_status(response, status);
#endif /*WITH_COAP*/
}

#ifdef WITH_COAP
static method_t coap_to_rest_method(coap_method_t method)
{
  return (method_t)(1 << (method - 1));
}

static coap_method_t rest_to_coap_method(method_t method)
{
  coap_method_t coap_method = COAP_GET;
  switch (method) {
  case METHOD_GET:
    coap_method = COAP_GET;
    break;
  case METHOD_POST:
    coap_method = COAP_POST;
      break;
  case METHOD_PUT:
    coap_method = COAP_PUT;
      break;
  case METHOD_DELETE:
    coap_method = COAP_DELETE;
      break;
  default:
    break;
  }
  return coap_method;
}
#endif /*WITH_COAP*/

method_t
rest_get_method_type(REQUEST* request)
{
#ifdef WITH_COAP
  return coap_to_rest_method(coap_get_method(request));
#else
  return (method_t)(request->request_type);
#endif
}

/*Only defined for COAP for now.*/
#ifdef WITH_COAP
void
rest_set_method_type(REQUEST* request, method_t method)
{
  coap_set_method(request, rest_to_coap_method(method));
}
#endif /*WITH_COAP*/

void
rest_set_response_payload(RESPONSE* response, uint8_t* payload, uint16_t size)
{
#ifdef WITH_COAP
  coap_set_payload(response, payload, size);
#else
  http_set_res_payload(response, payload, size);
#endif /*WITH_COAP*/
}

/*Only defined for COAP for now.*/
#ifdef WITH_COAP
void
rest_set_request_payload(REQUEST* request, uint8_t* payload, uint16_t size)
{
  coap_set_payload(request, payload, size);
}
#endif /*WITH_COAP*/

int
rest_get_query_variable(REQUEST* request, const char *name, char* output, uint16_t output_size)
{
#ifdef WITH_COAP
  return coap_get_query_variable(request, name, output, output_size);
#else
  return http_get_query_variable(request, name, output, output_size);
#endif /*WITH_COAP*/
}

int
rest_get_post_variable(REQUEST* request, const char *name, char* output, uint16_t output_size)
{
#ifdef WITH_COAP
  return coap_get_post_variable(request, name, output, output_size);
#else
  return http_get_post_variable(request, name, output, output_size);
#endif /*WITH_COAP*/
}

content_type_t
rest_get_header_content_type(REQUEST* request)
{
#ifdef WITH_COAP
  return coap_get_header_content_type(request);
#else
  return http_get_header_content_type(request);
#endif /*WITH_COAP*/
}

int
rest_set_header_content_type(RESPONSE* response, content_type_t content_type)
{
#ifdef WITH_COAP
  return coap_set_header_content_type(response, content_type);
#else
  return http_set_res_header(response, HTTP_HEADER_NAME_CONTENT_TYPE, http_get_content_type_string(content_type), 1);
#endif /*WITH_COAP*/

}

int
rest_get_header_etag(RESPONSE* response, uint32_t *etag)
{
#ifdef WITH_COAP
  return coap_get_header_etag(response, etag);
#else
  return 0; // FIXME
#endif /*WITH_COAP*/
}
int
rest_set_header_etag(RESPONSE* response, uint32_t etag)
{
#ifdef WITH_COAP
  return coap_set_header_etag(response, etag);
#else
  /* restricted to comply with 32-bit size for CoAP */
  char temp_etag[5];
  sprintf(temp_etag, "%04x", etag);
  return http_set_res_header(response, HTTP_HEADER_NAME_ETAG, temp_etag, 1);
#endif /*WITH_COAP*/
}

int
rest_get_header_max_age(RESPONSE* response, uint32_t *age)
{
#ifdef WITH_COAP
  return coap_get_header_max_age(response, age);
#else
  return 0; // FIXME
#endif /*WITH_COAP*/
}
int
rest_set_header_max_age(RESPONSE* response, uint32_t age)
{
#ifdef WITH_COAP
  return coap_set_header_max_age(response, age);
#else
  /* Cache-Control: max-age=age for HTTP */
  char temp_age[19];
  sprintf(temp_age, "max-age=%lu", age);
  return http_set_res_header(response, HTTP_HEADER_CACHE_CONTROL, temp_age, 1);
#endif /*WITH_COAP*/
}

//int rest_set_header_uri(RESPONSE* response, char *uri, uint16_t len); // use tokens

int
rest_get_header_location(RESPONSE* response, char **uri)
{
#ifdef WITH_COAP
  return coap_get_header_location(response, uri);
#else
  return 0; // FIXME http_set_res_header(response, HTTP_HEADER_NAME_LOCATION, uri, 1);
#endif /*WITH_COAP*/
}
int
rest_set_header_location(RESPONSE* response, char *uri)
{
#ifdef WITH_COAP
  return coap_set_header_location(response, uri);
#else
  return http_set_res_header(response, HTTP_HEADER_NAME_LOCATION, uri, 1);
#endif /*WITH_COAP*/
}

int
rest_get_header_observe(RESPONSE* response, uint32_t *observe)
{
#ifdef WITH_COAP
  return coap_get_header_observe(response, observe);
#else
  return 0;
#endif /*WITH_COAP*/
}
int
rest_set_header_observe(RESPONSE* response, uint32_t observe)
{
#ifdef WITH_COAP
  return coap_set_header_observe(response, observe);
#else
  return 0;
#endif /*WITH_COAP*/
}

int
rest_get_header_token(RESPONSE* response, uint16_t *token)
{
#ifdef WITH_COAP
  return coap_get_header_token(response, token);
#else
  return 0;
#endif /*WITH_COAP*/
}
int
rest_set_header_token(RESPONSE* response, uint16_t token)
{
#ifdef WITH_COAP
  return coap_set_header_token(response, token);
#else
  return 0;
#endif /*WITH_COAP*/
}

int
rest_get_header_block(RESPONSE* response, uint32_t *num, uint8_t *more, uint16_t *size)
{
#ifdef WITH_COAP
  return coap_get_header_block(response, num, more, size);
#else
  return 0;
#endif /*WITH_COAP*/
}
int
rest_set_header_block(RESPONSE* response, uint32_t num, uint8_t more, uint16_t size)
{
#ifdef WITH_COAP
  return coap_set_header_block(response, num, more, size);
#else
  return 0;
#endif /*WITH_COAP*/
}



int
rest_invoke_restful_service(REQUEST* request, RESPONSE* response)
{
  int found = 0;
  const char* url = request->url;
  uint16_t url_len = request->url_len;

  PRINTF("rest_invoke_restful_service url %s url_len %d -->\n", url, url_len);

  resource_t* resource = NULL;

  for (resource = (resource_t*)list_head(restful_services); resource; resource = resource->next) {
    /*if the web service handles that kind of requests and urls matches*/
    if (url && strlen(resource->url) == url_len && strncmp(resource->url, url, url_len) == 0){
      found = 1;
      method_t method = rest_get_method_type(request);

      PRINTF("method %u, resource->methods_to_handle %u\n", (uint16_t)method, resource->methods_to_handle);

      if (resource->methods_to_handle & method) {

        /*call pre handler if it exists*/
        if (!resource->pre_handler || resource->pre_handler(request, response)) {
          /* call handler function*/
          resource->handler(request, response);

          /*call post handler if it exists*/
          if (resource->post_handler) {
            resource->post_handler(request, response);
          }
        }
      } else {
        rest_set_response_status(response, METHOD_NOT_ALLOWED_405);
      }
      break;
    }
  }

  if (!found) {
    rest_set_response_status(response, NOT_FOUND_404);
  }

  return found;
}
