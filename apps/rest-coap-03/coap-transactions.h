/*
 * coap-03.h
 *
 *  Created on: 12 Apr 2011
 *      Author: Matthias Kovatsch
 */

#ifndef COAP_TRANSACTIONS_H_
#define COAP_TRANSACTIONS_H_

#include "coap-03.h"

#if !defined(WITH_COAP) || WITH_COAP!=3
#error "WITH_COAP must be set to 3"
#endif

/*
 * The number of concurrent messages that can be stored for retransmission in the transaction layer.
 */
#ifndef COAP_MAX_OPEN_TRANSACTIONS
#define COAP_MAX_OPEN_TRANSACTIONS  4
#endif /* COAP_MAX_OPEN_TRANSACTIONS */

#include "coap-03.h"

/* container for transactions with message buffer and retransmission info */
typedef struct coap_transaction {
  struct coap_transaction *next; /* for LIST */

  uint16_t tid;
  struct etimer retrans_timer;
  uint8_t retrans_counter;

  uip_ipaddr_t addr;
  uint16_t port;

  uint16_t packet_len;
  uint8_t packet[COAP_MAX_PACKET_SIZE+1]; /* +1 for the terminating '\0' to simply and savely use snprintf(buf, len+1, "", ...) in the resource handler. */
} coap_transaction_t;

void coap_register_as_transaction_handler();

coap_transaction_t *coap_new_transaction(uint16_t tid, uip_ipaddr_t *addr, uint16_t port);
void coap_send_transaction(coap_transaction_t *t);
void coap_cancel_transaction(coap_transaction_t *t);
int coap_cancel_transaction_by_tid(uint16_t tid);

void coap_check_transactions();

#endif /* COAP_TRANSACTIONS_H_ */
