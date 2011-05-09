#ifndef __COAP_CLIENT_H__
#define __COAP_CLIENT_H__

#include "contiki.h"
#include "contiki-net.h"
#include "rest.h"
#include "coap-03.h"

struct request_state_t {
    struct pt pt;
};
typedef void (coap_request_callback)(uint16_t, uint8_t *, uint16_t);

PT_THREAD(blocking_rest_request(struct request_state_t *request_state,
		process_event_t ev,
		uip_ipaddr_t *remote_ipaddr, uint16_t remote_port,
		coap_packet_t *request,
		restful_response_handler request_callback)
);

#endif /* __COAP_CLIENT_H__ */
