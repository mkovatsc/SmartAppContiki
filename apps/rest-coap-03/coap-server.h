#ifndef COAP_SERVER_H_
#define COAP_SERVER_H_

#if !defined(WITH_COAP) || WITH_COAP!=3
#error "### WITH_COAP MUST BE DEFINED: 3 ###"
#endif

#include "contiki.h"
#include "contiki-lib.h"
#include "coap-03.h"
#include "coap-transactions.h"
#include "rest.h" /*added for periodic_resource*/

/*Declare process*/
PROCESS_NAME(coap_server);

#define SERVER_LISTEN_PORT 61616

/*Type definition of the service callback*/
typedef int (*service_callback) (coap_packet_t* request, coap_packet_t* response);

/*
 *Setter of the service callback, this callback will be called in case of HTTP request.
 */
void set_service_callback(service_callback callback);

/*
 *Add resource to the list of restful_periodic_services
 */
void coap_activate_periodic_resource(struct periodic_resource_s *periodic_resource);

void coap_default_block_handler(coap_packet_t* request, coap_packet_t* response);
//void resource_changed(struct periodic_resource_t* resource);

#endif /* COAP_SERVER_H_ */
