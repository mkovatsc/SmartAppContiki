/*
 * coap-03.h
 *
 *  Created on: 12 Apr 2011
 *      Author: Matthias Kovatsch, based on Dogan Yazar's work
 */

#ifndef COAP_OBSERVING_H_
#define COAP_OBSERVING_H_

#if !defined(WITH_COAP) || WITH_COAP!=3
#error "WITH_COAP must be set to 3"
#endif

#include "coap-03.h"
#include "coap-transactions.h"

#ifndef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS      2
#endif /* COAP_MAX_OBSERVERS */

#define COAP_OBSERVING_REFRESH_INTERVAL  15

#if COAP_MAX_OPEN_TRANSACTIONS<COAP_MAX_OBSERVERS
#warning "COAP_MAX_OPEN_TRANSACTIONS smaller than COAP_MAX_OBSERVERS: cannot handle CON notifications"
#endif

/* container for transactions with message buffer and retransmission info */
typedef struct coap_observer {
  struct coap_observer *next; /* for LIST */

  const char *url;
  uip_ipaddr_t addr;
  uint16_t port;
  uint8_t token_len;
  uint8_t token[COAP_TOKEN_LEN];
  struct stimer refresh_timer;
} coap_observer_t;

list_t coap_get_observers(void);
coap_observer_t *coap_add_observer(const char *url, uip_ipaddr_t *addr, uint16_t port, const uint8_t *token, uint8_t token_len);
void coap_remove_observer(coap_observer_t *o);
int coap_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port);
int coap_remove_observer_by_token(uip_ipaddr_t *addr, uint16_t port, uint8_t *token, uint8_t token_len);
void coap_notify_observers(const char *url, coap_message_type_t type, uint32_t observe, uint8_t *payload, uint16_t payload_len);

void coap_observe_handler(coap_packet_t *request, coap_packet_t *response);

#endif /* COAP_OBSERVING_H_ */
