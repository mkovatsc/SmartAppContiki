/*
 * coap-03.h
 *
 *  Created on: 12 Apr 2011
 *      Author: Matthias Kovatsch
 */

#include "contiki.h"
#include "contiki-net.h"

#include "coap-transactions.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif


MEMB(transactions_memb, coap_transaction_t, COAP_MAX_OPEN_TRANSACTIONS);
LIST(transactions_list);


coap_transaction_t *
coap_new_transaction(uint16_t tid, uip_ipaddr_t *addr, uint16_t port)
{
  coap_transaction_t *t = memb_alloc(&transactions_memb);

  if (t)
  {
    t->tid = tid;
    t->retrans_counter = 0;

    /* save client address */
    uip_ipaddr_copy(&t->addr, addr);
    t->port = port;
  }

  return t;
}

void
coap_send_transaction(coap_transaction_t *t)
{
  PRINTF("Sending transaction %u\n", t->tid);

  coap_send_message(&t->addr, t->port, t->packet, t->packet_len);

  if (COAP_TYPE_CON==((COAP_HEADER_TYPE_MASK & t->packet[0])>>COAP_HEADER_TYPE_POSITION))
  {
    if (t->retrans_counter<COAP_MAX_RETRANSMIT)
    {
      PRINTF("Keeping transaction %u\n", t->tid);
      etimer_set(&t->retrans_timer, CLOCK_SECOND * COAP_RESPONSE_TIMEOUT * 1<<(t->retrans_counter));
      list_add(transactions_list, t); /* list itself makes sure same element is not added twice */

      t = NULL;
    }
    else
    {
      /* timeout */

      /* handle observers */
      coap_remove_observer_by_client(&t->addr, t->port);

      coap_cancel_transaction(t);
    }
  }
  else
  {
    coap_cancel_transaction(t);
  }
}

void
coap_cancel_transaction(coap_transaction_t *t)
{
  list_remove(transactions_list, t);

  PRINTF("Freeing transaction %u\n", t->tid);
  etimer_stop(&t->retrans_timer);
  memb_free(&transactions_memb, t);
}

void
coap_cancel_transaction_by_tid(uint16_t tid)
{
  coap_transaction_t *t = NULL;

  for (t = (coap_transaction_t*)list_head(transactions_list); t; t = t->next)
  {
    if (t->tid==tid)
    {
      coap_cancel_transaction(t);
      return;
    }
  }
}

void
coap_check_transactions()
{
  coap_transaction_t *t = NULL;

  for (t = (coap_transaction_t*)list_head(transactions_list); t; t = t->next)
  {
    if (etimer_expired(&t->retrans_timer))
    {
      ++(t->retrans_counter);
      PRINTF("Retransmitting %u (%u)\n", t->tid, t->retrans_counter);
      coap_send_transaction(t);
    }
  }
}
