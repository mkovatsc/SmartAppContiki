/*
 * coap-03.h
 *
 *  Created on: 12 Apr 2011
 *      Author: Matthias Kovatsch, based on Dogan Yazar's work
 */

#ifndef COAP_TRANSACTIONS_H_
#define COAP_TRANSACTIONS_H_

#if !defined(WITH_COAP) || WITH_COAP!=3
#error "### WITH_COAP MUST BE DEFINED: 3 ###"
#endif

#include "contiki-lib.h"
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
  uint8_t packet[COAP_MAX_PACKET_SIZE];
} coap_transaction_t;


coap_transaction_t *coap_new_transaction(uint16_t tid, uip_ipaddr_t *addr, uint16_t port);
void coap_send_transaction(coap_transaction_t *t);

void coap_check_transactions();

#endif /* COAP_TRANSACTIONS_H_ */
