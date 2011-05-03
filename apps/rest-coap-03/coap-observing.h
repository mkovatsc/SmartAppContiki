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

#ifndef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS      2
#endif /* COAP_MAX_OBSERVERS */

/* container for transactions with message buffer and retransmission info */
typedef struct coap_observer {
  struct coap_observer *next; /* for LIST */

  const char *url;
  uip_ipaddr_t addr;
  uint16_t port;
  uint16_t token;
} coap_observer_t;

list_t coap_get_observers(void);
coap_observer_t *coap_add_observer(const char *url, uip_ipaddr_t *addr, uint16_t port, uint16_t token);
void coap_remove_observer(coap_observer_t *o);
void coap_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port);
void coap_notify_observers(const char *url, uint32_t observe, uint8_t *payload, uint16_t payload_len);

#endif /* COAP_OBSERVING_H_ */
