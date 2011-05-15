#ifndef COAP_SERVER_H_
#define COAP_SERVER_H_

#if !defined(REST)
#error "Define REST to \"coap_rest_implementation\""
#endif

#include "coap-03.h"
#include "coap-03-transactions.h"
#include "coap-03-observing.h"
#include "pt.h"

/* Declare server process */
PROCESS_NAME(coap_server);

#define SERVER_LISTEN_PORT      UIP_HTONS(COAP_DEFAULT_PORT)

typedef coap_packet_t rest_request_t;
typedef coap_packet_t rest_response_t;

struct request_state_t {
    struct pt pt;
    struct process *process;
    coap_transaction_t *transaction;
    coap_packet_t *response;
    uint32_t block_num;
};

typedef void (*blocking_response_handler) (void* response);

PT_THREAD(coap_blocking_request(struct request_state_t *request_state,
                                process_event_t ev,
                                uip_ipaddr_t *remote_ipaddr, uint16_t remote_port,
                                coap_packet_t *request,
                                blocking_response_handler request_callback)
);

extern const struct rest_implementation coap_rest_implementation;

#endif /* COAP_SERVER_H_ */
