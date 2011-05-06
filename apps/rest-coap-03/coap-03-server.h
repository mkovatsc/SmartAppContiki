#ifndef COAP_SERVER_H_
#define COAP_SERVER_H_

#if !defined(WITH_COAP) || WITH_COAP!=3
#error "WITH_COAP must be set to 3"
#endif

#include "rest.h"
#include "coap-03.h"

/* Declare server process */
PROCESS_NAME(coap_server);

#define SERVER_LISTEN_PORT      UIP_HTONS(61616)

typedef coap_packet_t rest_request_t;
typedef coap_packet_t rest_response_t;

extern const struct rest_implementation coap_rest_implementation;

#endif /* COAP_SERVER_H_ */
