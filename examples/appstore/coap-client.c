#include "contiki.h"
#include "pt.h"
#include "coap-03.h"
#include "coap-client.h"
#include "coap-03-transactions.h"

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

void blocking_request_callback(coap_transaction_t *transaction, void *response) {
	coap_packet_t *r = (coap_packet_t*)response;
	process_poll(transaction->process);
}

PT_THREAD(blocking_rest_request(struct request_state_t *state, process_event_t ev,
		uip_ipaddr_t *remote_ipaddr, uint16_t remote_port,
		coap_packet_t *request,
		restful_response_handler request_callback)) {

	PT_BEGIN(&state->pt);

	static coap_transaction_t *transaction;

	transaction = coap_new_transaction(request->tid, remote_ipaddr, remote_port);
	if(transaction == NULL) {
		PT_EXIT(&state->pt);
	}

	request->header = transaction->packet;
	transaction->packet_len = coap_serialize_message(request);
	transaction->process = PROCESS_CURRENT();
	transaction->callback = blocking_request_callback;

	PRINTF("Sending to /%.*s\n", request->uri_path_len, request->uri_path);
	PRINTF("  %.*s\n", request->payload_len, request->payload);
	coap_send_transaction(transaction);

PRINTF("Before yield\n");
	PT_YIELD_UNTIL(&state->pt, ev == PROCESS_EVENT_POLL);
PRINTF("After yield \n");
request_callback(transaction->response);

//    do { /* transmit the packet periodically until a response is received */
//      /* send CoAP request */
//
//      /* Add block header only if it is necessary*/
//      if (state->block.size) {
//        PRINTF("CoAP client request, chunk: %d\n", state->block.number + 1);
//        coap_set_header_block(&coap_packet, state->block.number + 1, 1, state->block.size);
//      }
//      coap_packet.tid = xact_id;
//      PRINTF("Request TID %d\n", coap_packet.tid);
//      request_size = serialize_packet(&coap_packet, buf);
//      uip_udp_packet_send(conn, buf, request_size);
//      etimer_set(&state->et, 2*CLOCK_SECOND);
//      PT_WAIT_UNTIL(&state->pt, uip_newdata() || etimer_expired(&state->et));
//      if(uip_newdata()) {
//        /* receive CoAP response */
//        parse_message(&coap_packet, uip_appdata, uip_datalen());
//        PRINTF("Reply TID %d\n", coap_packet.tid);
//        if(coap_packet.tid == xact_id) {
//          resp_received = 1;
//          state->block.more = 0;
//          if (coap_get_header_block(&coap_packet, &state->block)) {
//            PRINTF("CoAP client reply, chunk: %u %u %u\n", (uint16_t)state->block.number, state->block.more, state->block.size);
//          }
//          if(request_callback) request_callback(state->block.number, coap_packet.payload, coap_packet.payload_len);
//        }
//      }
//    } while(!resp_received);
//    etimer_stop(&state->et);
//  }

  PT_END(&state->pt);
}
