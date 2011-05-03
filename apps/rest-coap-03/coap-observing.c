/*
 * coap-observing.h
 *
 *  Created on: 03 May 2011
 *      Author: Matthias Kovatsch
 */

#include "coap-observing.h"

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


MEMB(observers_memb, coap_observer_t, COAP_MAX_OBSERVERS);
LIST(observers_list);

coap_observer_t *
coap_add_observer(const char *url, uip_ipaddr_t *addr, uint16_t port, uint16_t token)
{
  coap_observer_t *o = memb_alloc(&observers_memb);

  if (o)
  {
    PRINTF("Adding observer for /%s\n", url);

    o->url = url;
    uip_ipaddr_copy(&o->addr, addr);
    o->port = port;
    o->token = token;

    list_add(observers_list, o);
  }

  return o;
}

void
coap_remove_observer(coap_observer_t *o)
{
  PRINTF("Removing observer for /%s\n", o->url);

  memb_free(&observers_memb, o);
  list_remove(observers_list, o);
}

void
coap_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port)
{
  coap_observer_t* obs = NULL;
  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    if (uip_ipaddr_cmp(&obs->addr, addr) && obs->port==port)
    {
      coap_remove_observer(obs);
    }
  }
}

// FIXME CON vs NON, implement special transaction for CON, sharing the same buffer
void
coap_notify_observers(const char *url, uint32_t observe, uint8_t *payload, uint16_t payload_len)
{
  coap_observer_t* obs = NULL;
  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
    {
      if (obs->url==url)
      {
        uint8_t temp[COAP_MAX_PACKET_SIZE+1] = {0};
        coap_packet_t push[1];
        coap_init_message(push, temp, COAP_TYPE_CON, OK_200, random_rand());
        coap_set_header_observe(push, observe);
        coap_set_header_token(push, obs->token);
        coap_set_payload(push, payload, payload_len);

        PRINTF("Observing: Notify from %s for ", url);
        PRINT6ADDR(&obs->addr);
        PRINTF(":%u\n", obs->port);
        coap_send_message(&obs->addr, obs->port, push->header, coap_serialize_message(push));
      }
    }
}
