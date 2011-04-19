/*
 * coap-03.h
 *
 *  Created on: 12 Apr 2011
 *      Author: Matthias Kovatsch, based on Dogan Yazar's work
 */

#ifdef CONTIKI_TARGET_NETSIM
  #include <stdio.h>
  #include <iostream>
  #include <cstring>
  #include <cstdlib>
  #include <unistd.h>
  #include <errno.h>
  #include <string.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#else
  #include "contiki.h"
  #include "contiki-net.h"
  #include <string.h>
  #include <stdio.h>
#endif

#include "coap-transactions.h"

#define DEBUG 1
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

static struct uip_udp_conn *udp_conn = NULL;
static uip_ipaddr_t udp_addr = {{0}};
static uint16_t udp_port = 0;

void
coap_init_transactions(struct uip_udp_conn *conn)
{
  udp_conn = conn;
  uip_ipaddr_copy(&udp_addr, &conn->ripaddr);
  udp_port = conn->rport;

  list_init(transactions_list);
}

coap_transaction_t *
coap_new_transaction(uint16_t tid, uip_ipaddr_t *addr, uint16_t port)
{
  coap_transaction_t *t = memb_alloc(&transactions_memb);

  t->tid = tid;
  t->retrans_counter = 0;

  /* save client address */
  uip_ipaddr_copy(&t->addr, addr);
  t->port = port;

  return t;
}

void
coap_send_transaction(coap_transaction_t *t)
{
  coap_message_type_t type = (COAP_HEADER_TYPE_MASK & t->packet[0])>>COAP_HEADER_TYPE_POSITION;

  /*configure connection to reply to client*/
  uip_ipaddr_copy(&udp_conn->ripaddr, &t->addr);
  udp_conn->rport = t->port;

  uip_udp_packet_send(udp_conn, t->packet, t->packet_len);
  PRINTF("-sent UDP datagram------\n Length: %u\n -----------------------\n", t->packet_len);

  /* Restore server connection to allow data from any node */
  uip_ipaddr_copy(&udp_conn->ripaddr, &udp_addr);
  udp_conn->rport = udp_port;


  if (type==COAP_TYPE_CON)
  {
    if (t->retrans_counter<COAP_MAX_RETRANSMIT)
    {
      etimer_set(t->retrans_timer, CLOCK_SECOND * COAP_RESPONSE_TIMEOUT);
      list_add(transactions_list, t);

      t = NULL;
    }
    else
    {

    }
  }

  if (t)
  {
    memb_free(&transactions_memb, t);
  }
}
