#ifndef COAPSERVER_H_
#define COAPSERVER_H_

#include "contiki.h"
#include "contiki-lib.h"
#include "coap-03.h"
#include "rest.h" /*added for periodic_resource*/

/*Declare process*/
PROCESS_NAME(coap_server);

#define SERVER_LISTEN_PORT 61616

typedef struct coap_transaction {
  uint16_t tid;

  uip_ipaddr_t addr;
  uint16_t port;

  char packet[COAP_MAX_PACKET_SIZE];
  uint16_t packet_len;
} coap_transaction_t;

/*Type definition of the service callback*/
typedef int (*service_callback) (coap_packet_t* request, coap_packet_t* response);

/*
 *Setter of the service callback, this callback will be called in case of HTTP request.
 */
void set_service_callback(service_callback callback);

/*
 *Add resource to the list of restful_periodic_services
 */
void coap_activate_periodic_resource(struct periodic_resource_t* periodic_resource);

//void resource_changed(struct periodic_resource_t* resource);

#endif /* COAPSERVER_H_ */
