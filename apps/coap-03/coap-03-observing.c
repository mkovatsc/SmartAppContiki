/*
 * coap-observing.c
 *
 *  Created on: 03 May 2011
 *      Author: Matthias Kovatsch
 */

#include <stdio.h>
#include <string.h>

#include "coap-03-transactions.h"
#include "coap-03-observing.h"

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

/*-----------------------------------------------------------------------------------*/
coap_observer_t *
coap_add_observer(const char *url, uip_ipaddr_t *addr, uint16_t port, const uint8_t *token, size_t token_len)
{
  coap_observer_t *o = memb_alloc(&observers_memb);

  if (o)
  {
    o->url = url;
    uip_ipaddr_copy(&o->addr, addr);
    o->port = port;
    o->token_len = token_len;
    memcpy(o->token, token, token_len);

    stimer_set(&o->refresh_timer, COAP_OBSERVING_REFRESH_INTERVAL);

    PRINTF("Adding observer for /%s [0x%02X%02X]\n", o->url, o->token[0], o->token[1]);
    list_add(observers_list, o);
  }

  return o;
}
/*-----------------------------------------------------------------------------------*/
void
coap_remove_observer(coap_observer_t *o)
{
  PRINTF("Removing observer for /%s [0x%02X%02X]\n", o->url, o->token[0], o->token[1]);

  memb_free(&observers_memb, o);
  list_remove(observers_list, o);
}

int
coap_remove_observer_by_client(uip_ipaddr_t *addr, uint16_t port)
{
  int removed = 0;
  coap_observer_t* obs = NULL;
  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
      PRINTF("Remove check Port %u\n", port);
    if (uip_ipaddr_cmp(&obs->addr, addr) && obs->port==port)
    {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
int
coap_remove_observer_by_token(uip_ipaddr_t *addr, uint16_t port, uint8_t *token, size_t token_len)
{
  int removed = 0;
  coap_observer_t* obs = NULL;
  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    PRINTF("Remove check Token 0x%02X%02X\n", token[0], token[1]);
    if (uip_ipaddr_cmp(&obs->addr, addr) && obs->port==port && memcmp(obs->token, token, token_len)==0)
    {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*-----------------------------------------------------------------------------------*/
void
coap_notify_observers(const char *url, int type, uint32_t observe, uint8_t *payload, size_t payload_len)
{
  coap_observer_t* obs = NULL;
  for (obs = (coap_observer_t*)list_head(observers_list); obs; obs = obs->next)
  {
    if (obs->url==url) /* using RESOURCE url string as handle */
    {
      coap_transaction_t *transaction = NULL;

      // FIXME CON vs NON, implement special transaction for CON, sharing the same buffer, updating retransmissions with newer notifications, etc.
      //       Maybe even less max retransmissions.
      if ( (transaction = coap_new_transaction(coap_get_tid(), &obs->addr, obs->port)) )
      {
        /* Use CON to check whether client is still there/interested after COAP_OBSERVING_REFRESH_INTERVAL. */
        if (stimer_expired(&obs->refresh_timer))
        {
          PRINTF("Observing: Refresh client with CON\n");
          type = COAP_TYPE_CON;
          stimer_restart(&obs->refresh_timer);
        }

        /* prepare response */
        coap_packet_t push[1]; /* This way the packet can be treated as pointer as usual. */
        coap_init_message(push, (coap_message_type_t)type, OK_200, transaction->tid );
        coap_set_header_observe(push, observe);
        coap_set_header_token(push, obs->token, obs->token_len);
        coap_set_payload(push, payload, payload_len);
        transaction->packet_len = coap_serialize_message(push, transaction->packet);

        PRINTF("Observing: Notify from /%s for ", url);
        PRINT6ADDR(&obs->addr);
        PRINTF(":%u\n", obs->port);
        PRINTF("  %.*s\n", payload_len, payload);

        coap_send_transaction(transaction);
      }
    }
  }
}
/*-----------------------------------------------------------------------------------*/
void
coap_observe_handler(void *request, void *response)
{
  static char content[26];

  if (response && ((coap_packet_t *)response)->code<128) /* response without error code */
  {
    if (IS_OPTION((coap_packet_t *)request, COAP_OPTION_OBSERVE))
    {
      if (IS_OPTION((coap_packet_t *)request, COAP_OPTION_TOKEN))
      {
        const char *url;
        REST.get_url(request, &url); /* request url was set to RESOURCE url string as handle. */
        if (coap_add_observer(url, &((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])->srcipaddr, ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])->srcport, ((coap_packet_t *)request)->token, ((coap_packet_t *)request)->token_len))
        {
          coap_set_header_observe(response, 0);
          coap_set_payload(response, (uint8_t *)content, snprintf(content, sizeof(content), "Added as observer %u/%u", list_length(observers_list), COAP_MAX_OBSERVERS));
        }
        else
        {
          ((coap_packet_t *)response)->code = SERVICE_UNAVAILABLE_503;
          coap_set_payload(response, (uint8_t *)"Too many observers", 18);
        } /* if (added observer) */
      }
      else /* if (token) */
      {
        ((coap_packet_t *)response)->code = TOKEN_OPTION_REQUIRED;
        coap_set_payload(response, (uint8_t *)"Observing requires token", 24);
      } /* if (token) */
    }
    else /* if (observe) */
    {
      /* Remove client if it is currently observing. */
      coap_remove_observer_by_client(&((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])->srcipaddr, ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])->srcport);
    } /* if (observe) */
  }
}
