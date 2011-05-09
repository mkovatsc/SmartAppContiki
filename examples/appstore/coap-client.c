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

void blocking_request_callback(void *data, void *response) {
	struct request_state_t *state = (struct request_state_t *)data;
	state->response = (coap_packet_t*)response;
	process_poll(state->process);
}

PT_THREAD(blocking_rest_request(struct request_state_t *state, process_event_t ev,
	uip_ipaddr_t *remote_ipaddr, uint16_t remote_port,
	coap_packet_t *request,
	blocking_response_handler request_callback)) {

	PT_BEGIN(&state->pt);

	state->block_num = 0;
	state->response = NULL;
	state->process = PROCESS_CURRENT();

	uint8_t more;
	do {
		extern char elf_filename[];
		request->option_count = 0;
	    request->tid = coap_get_tid();
		state->transaction = coap_new_transaction(request->tid, remote_ipaddr, remote_port);
		if(state->transaction == NULL) {
			PRINTF("Could not allocate transaction");
			PT_EXIT(&state->pt);
		}
		request->header = state->transaction->packet;
		state->transaction->callback = blocking_request_callback;
		state->transaction->data = state;
		coap_set_header_block(request, state->block_num, 1, REST_MAX_CHUNK_SIZE);
		state->transaction->packet_len = coap_serialize_message(request);
		PRINTF("Sending #%lu (%u bytes)\n", state->block_num, state->transaction->packet_len);
		coap_send_transaction(state->transaction);
		PT_YIELD_UNTIL(&state->pt, ev == PROCESS_EVENT_POLL);
		request_callback(state->response);
		coap_get_header_block(state->response, NULL, &more, NULL, NULL);
		state->block_num++;
	} while(more != 0);

  PT_END(&state->pt);
}
