/*
 * coap-03-observing.h
 *
 *  Created on: 03 May 2011
 *      Author: Matthias Kovatsch
 */

#ifndef COAP_OBSERVING_H_
#define COAP_OBSERVING_H_

#include "er-coap-03.h"
#include "er-coap-03-transactions.h"

#ifndef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS      4
#endif /* COAP_MAX_OBSERVERS */

/* Interval in seconds in which NON notifies are changed to CON notifies to check client. */
#define COAP_OBSERVING_REFRESH_INTERVAL  60

#if COAP_MAX_OPEN_TRANSACTIONS<COAP_MAX_OBSERVERS
#warning "COAP_MAX_OPEN_TRANSACTIONS smaller than COAP_MAX_OBSERVERS: cannot handle CON notifications"
#endif

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
coap_observer_t *coap_add_observer(const char *url, uip_ipaddr_t *addr, uint16_t port, const uint8_t *token, size_t token_len);
void coap_remove_observer(coap_observer_t *o);
int coap_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port);
int coap_remove_observer_by_token(uip_ipaddr_t *addr, uint16_t port, uint8_t *token, size_t token_len);
void coap_notify_observers(const char *url, int type, uint32_t observe, uint8_t *payload, size_t payload_len);

void coap_observe_handler(resource_t *resource, void *request, void *response);

#endif /* COAP_OBSERVING_H_ */
