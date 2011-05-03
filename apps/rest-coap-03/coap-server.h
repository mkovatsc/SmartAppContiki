#ifndef COAP_SERVER_H_
#define COAP_SERVER_H_

#if !defined(WITH_COAP) || WITH_COAP!=3
#error "WITH_COAP must be set to 3"
#endif

#include "contiki.h"
#include "contiki-lib.h"
#include "coap-03.h"
#include "coap-transactions.h"
#include "coap-observing.h"
#include "rest.h" /*added for periodic_resource*/

/*Declare process*/
PROCESS_NAME(coap_server);

#define SERVER_LISTEN_PORT 61616

/*Type definition of the service callback*/
typedef int (*service_callback) (coap_packet_t *request, coap_packet_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/*
 *Setter of the service callback, this callback will be called in case of HTTP request.
 */
void set_service_callback(service_callback callback);

#endif /* COAP_SERVER_H_ */
