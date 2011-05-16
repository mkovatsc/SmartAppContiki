/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: csma.c,v 1.24 2010/12/16 22:44:02 adamdunkels Exp $
 */

/**
 * \file
 *         A Carrier Sense Multiple Access (CSMA) MAC layer
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/* LOG
 * csma in order with one queue per neighbor
 * hack: slip_receiving: don't send now if there are incomming slip data
 */

#include "net/mac/csma.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"

#include "contiki.h"
#include "sys/ctimer.h"

#include "lib/random.h"

#include "net/netstack.h"

#include "lib/list.h"
#include "lib/memb.h"

#include <string.h>

#include <stdio.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else /* DEBUG */
#define PRINTF(...)
#endif /* DEBUG */

#ifndef CSMA_MAX_MAC_TRANSMISSIONS
#ifdef CSMA_CONF_MAX_MAC_TRANSMISSIONS
#define CSMA_MAX_MAC_TRANSMISSIONS CSMA_CONF_MAX_MAC_TRANSMISSIONS
#else
#define CSMA_MAX_MAC_TRANSMISSIONS 1
#endif /* CSMA_CONF_MAX_MAC_TRANSMISSIONS */
#endif /* CSMA_MAX_MAC_TRANSMISSIONS */

#if CSMA_MAX_MAC_TRANSMISSIONS < 1
#error CSMA_CONF_MAX_MAC_TRANSMISSIONS must be at least 1.
#error Change CSMA_CONF_MAX_MAC_TRANSMISSIONS in contiki-conf.h or in your Makefile.
#endif /* CSMA_CONF_MAX_MAC_TRANSMISSIONS < 1 */

struct neighbor_queue {
  struct neighbor_queue *next;
  uint8_t dest; /* last byte of the dest address */
  struct ctimer transmit_timer;
  uint8_t transmissions, max_transmissions, backofftx;
  uint8_t noacks, collisions, deferrals;
  void *cptr;
  mac_callback_t sent;
  LIST_STRUCT(queued_packet_list);
};

struct queued_packet {
  struct queued_packet *next;
  struct queuebuf *buf;
  //void *cptr;
//  mac_callback_t sent;
  //uint8_t dest; /* last byte of the dest address */
};

MEMB(neighbor_memb, struct neighbor_queue, MAX_NEIGHBORS);
MEMB(packet_memb, struct queued_packet, MAC_QUEUE_SIZE);
LIST(neighbor_list);

extern int slip_receiving;
extern int slip_cpt_queued;
int csma_cpt_queued = 0;
static uint16_t seqno = 0;

static void packet_sent(void *ptr, int status, int num_transmissions);

PROCESS(csma_process, "csma process");
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(csma_process, ev, data)
{
  PROCESS_BEGIN();
  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
    transmit_packet(data);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static struct neighbor_queue *neighbor_queue_from_dest(uint8_t dest) {
  struct neighbor_queue *n = list_head(neighbor_list);
  while(n != NULL) {
    if(n->dest == dest) {
      return n;
    }
    n = list_item_next(n);
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
static struct neighbor_queue *neighbor_queue_from_first_packet(struct queued_packet *q) {
  struct neighbor_queue *n = list_head(neighbor_list);
  /* first, only loop on list heads */
  while(n != NULL) {
    if(q == list_head(n->queued_packet_list)) {
      return n;
    }
    n = list_item_next(n);
  }
//  /* second, loop on all packets */
//  n = list_head(neighbor_list);
//  while(n != NULL) {
//    struct queued_packet *i = list_head(n->queued_packet_list);
//    while(i != NULL) {
//      if(i == q) {
//        return n;
//      }
//      i = list_item_next(i);
//    }
//    n = list_item_next(n);
//  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
static clock_time_t
default_timebase(void)
{
  clock_time_t time;
  /* The retransmission time must be proportional to the channel
     check interval of the underlying radio duty cycling layer. */
  time = NETSTACK_RDC.channel_check_interval();

  /* If the radio duty cycle has no channel check interval (i.e., it
     does not turn the radio off), we make the retransmission time
     proportional to the configured MAC channel check rate. */
  if(time == 0) {
    time = CLOCK_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE;
  }
  return time;
}
/*---------------------------------------------------------------------------*/
u8_t csma_queue_length(u8_t dest) {
  struct neighbor_queue *n = neighbor_queue_from_dest(dest);
  return n ? list_length(n->queued_packet_list) : 0;
}
/*---------------------------------------------------------------------------*/
struct queuebuf *csma_queue_next(u8_t dest) {
  struct neighbor_queue *n = neighbor_queue_from_dest(dest);
  struct queued_packet *next;
  if(n) {
    next = list_item_next(list_head(n->queued_packet_list));
    if(next) {
      return next->buf;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
static void
transmit_packet(void *ptr)
{
  struct queued_packet *q = ptr;
  if(q != NULL) {
    struct neighbor_queue *n = neighbor_queue_from_first_packet(q);
//    if(n->dest < node_id) { /* only one packet going to "previous" IDs */;
//#if WITH_ACK_FILTER
//      while(list_item_next(list_head(n->queued_packet_list)) != NULL) {
//        q = list_pop(n->queued_packet_list);
//        queuebuf_free(q->buf);
//        memb_free(&packet_memb, q);
//        csma_cpt_queued--;
//      }
//      q = list_head(n->queued_packet_list);
//#endif
//      packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS, 64);
//      n->max_transmissions = 64;
//    }

//    printf("-------- get %p %u\n", q->buf, (unsigned char)queuebuf_attr(q->buf, PACKETBUF_ATTR_MAC_SEQNO));
    queuebuf_to_packetbuf(q->buf);

    packetbuf_set_attr(PACKETBUF_ATTR_NUM_REXMIT, n->noacks);
//    PRINTF("csma: sending number %d %p, queue len %d\n", n->transmissions, q,
//           list_length(queued_packet_list));
    //    printf("s %d\n", packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0]);
    NETSTACK_RDC.send(packet_sent, q);
  }
}
/*---------------------------------------------------------------------------*/
static void
free_queued_packet(struct queued_packet *q)
{
  struct neighbor_queue *n = neighbor_queue_from_first_packet(q);
  if(q != NULL) {
    ctimer_stop(&n->transmit_timer);
    queuebuf_free(q->buf);
    list_pop(n->queued_packet_list);
    memb_free(&packet_memb, q);
    csma_cpt_queued--;
//    PRINTF("csma: free_queued_packet, queue length %d\n",
//        list_length(queued_packet_list));
    q = list_head(n->queued_packet_list);
    if(q) { /* enqueue next packet for this neighbor */
      n->transmissions = 0;
      n->collisions = 0;
      n->deferrals = 0;
      n->noacks = 0;
//      ctimer_set(&n->transmit_timer, 0, transmit_packet, q);
      process_post(&csma_process, PROCESS_EVENT_POLL, q);
    } else { /* free the neighbor */
      list_remove(neighbor_list, n);
      memb_free(&neighbor_memb, n);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
packet_sent(void *ptr, int status, int num_transmissions)
{
  struct queued_packet *q = ptr;
  clock_time_t time = 0;
  mac_callback_t sent;
  void *cptr;
  int num_tx;
  int backoff_transmissions;
  struct neighbor_queue *n = neighbor_queue_from_first_packet(q);

  switch(status) {
  case MAC_TX_OK:
    n->transmissions++;
    n->backofftx=0;
    COOJA_DEBUG_CINT('m', n->transmissions);
    break;
  case MAC_TX_NOACK:
    n->transmissions++;
    n->backofftx++;
    n->noacks++;
    break;
  case MAC_TX_COLLISION:
    n->collisions++;
    break;
  case MAC_TX_DEFERRED:
    n->deferrals++;
    break;
  }
//  printf("packet sent %d\n", status);
  //cptr = q->cptr;
  cptr = n->cptr;
  //sent = q->sent;
  sent = n->sent;
  num_tx = n->transmissions;
  
  if(status == MAC_TX_COLLISION ||
     status == MAC_TX_NOACK) {

    /* If the transmission was not performed because of a collision or
       noack, we must retransmit the packet. */
    
    switch(status) {
    case MAC_TX_COLLISION:
      PRINTF("csma: rexmit collision %d\n", n->transmissions);
      break;
    case MAC_TX_NOACK:
      PRINTF("csma: rexmit noack %d\n", n->transmissions);
      break;
    default:
      PRINTF("csma: rexmit err %d, %d\n", status, n->transmissions);
    }

    /* The retransmission time must be proportional to the channel
       check interval of the underlying radio duty cycling layer. */
    time = default_timebase();

    /* The retransmission time uses a linear backoff so that the
       interval between the transmissions increase with each
       retransmit. */
    backoff_transmissions = n->backofftx > 1 ? n->backofftx / 2: 1;
//    backoff_transmissions = 2;

    /* Clamp the number of backoffs so that we don't get a too long
       timeout here, since that will delay all packets in the
       queue. */
    if(backoff_transmissions > CSMA_MAX_BACKOFF) {
      backoff_transmissions = CSMA_MAX_BACKOFF;
    }

    time = /*time +*/ (random_rand() % (backoff_transmissions * time));

    if(n->transmissions < n->max_transmissions) {
      PRINTF("csma: retransmitting with time %lu %p\n", time, q);
      ctimer_set(&n->transmit_timer, time,
                 transmit_packet, q);
    } else {
      PRINTF("csma: drop with status %d after %d transmissions, %d collisions\n",
             status, n->transmissions, n->collisions);
      /*      queuebuf_to_packetbuf(q->buf);*/
      COOJA_DEBUG_PRINTF("csma: drop with status %d after %d transmissions, %d collisions\n",
                         status, n->transmissions, n->collisions);
      free_queued_packet(q);
      mac_call_sent_callback(sent, cptr, status, num_tx);
    }
  } else {
    if(status == MAC_TX_OK) {
      PRINTF("csma: rexmit ok %d\n", n->transmissions);
    } else {
      PRINTF("csma: rexmit failed %d: %d\n", n->transmissions, status);
    }
    /*    queuebuf_to_packetbuf(q->buf);*/
    free_queued_packet(q);
    mac_call_sent_callback(sent, cptr, status, num_tx);
  }
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
  struct queued_packet *q;
  struct neighbor_queue *n;
  uint8_t dest;
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_SEQNO, seqno++);
  
  /* If the packet is a broadcast, do not allocate a queue
     entry. Instead, just send it out.  */
  if(!rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                   &rimeaddr_null) &&
     packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS) > 0) {

    dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7];

    n = neighbor_queue_from_dest(dest);
    if(n == NULL) {
      n = memb_alloc(&neighbor_memb);
      if(n != NULL) {
        n->dest = dest;
        n->transmissions = 0;
        n->backofftx = 0;
        n->noacks = 0;
        n->collisions = 0;
        n->deferrals = 0;
        n->sent = sent;
        n->cptr = ptr;
        LIST_STRUCT_INIT(n, queued_packet_list);
        list_add(neighbor_list, n);
      }
    }

    if(n != NULL) {
      /* Remember packet for later. */
      q = memb_alloc(&packet_memb);
      if(q != NULL) {
        q->buf = queuebuf_new_from_packetbuf();
        //printf("-------- set %p %u\n", q->buf, (unsigned char)queuebuf_attr(q->buf, PACKETBUF_ATTR_MAC_SEQNO));
        if(q->buf != NULL) {

          if(ptr != n->cptr) {
            COOJA_DEBUG_STR("csma: warning, different cptr in same queue");
          }
          if(sent != n->sent) {
                      COOJA_DEBUG_STR("csma: warning, different sent callback in same queue");
                    }

          //q->cptr = ptr;
          //q->dest = dest;
          //q->sent = sent;

          if(packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS) == 0) {
            /* Use default configuration for max transmissions */
            n->max_transmissions = CSMA_MAX_MAC_TRANSMISSIONS;
          } else {
            n->max_transmissions =
                packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS);
          }

          if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
              PACKETBUF_ATTR_PACKET_TYPE_ACK) {
            list_push(n->queued_packet_list, q);
          } else {
            list_add(n->queued_packet_list, q);
          }

          csma_cpt_queued++;
          if(list_head(n->queued_packet_list) == q) {
           /* if(slip_receiving) {
              ctimer_set(&n->transmit_timer, 1,
                  transmit_packet, q);
            } else*/ {
              ctimer_stop(&n->transmit_timer);
              process_post(&csma_process, PROCESS_EVENT_POLL, q);
//              ctimer_set(&n->transmit_timer, 0,
  //                transmit_packet, q);
            }
          }
          return;
        }
        memb_free(&packet_memb, q);
        PRINTF("csma: could not allocate queuebuf, will drop if collision or noack\n");
      }
      if(list_length(n->queued_packet_list) == 0) {
        list_remove(neighbor_list, n);
        memb_free(&neighbor_memb, n);
      }
      PRINTF("csma: could not allocate packet, will drop if collision or noack\n");
//      COOJA_DEBUG_STR("csma: out of memory, could not allocate packet");
    } else {
//      COOJA_DEBUG_STR("csma: could not allocate neighbor memb");
    }
  } else {
    PRINTF("csma: send broadcast (%d) or without retransmissions (%d)\n",
        !rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
            &rimeaddr_null),
            packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS));
    NETSTACK_RDC.send(sent, ptr);
  }
}
/*---------------------------------------------------------------------------*/
static void
input_packet(void)
{
  NETSTACK_NETWORK.input();
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return NETSTACK_RDC.on();
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
  return NETSTACK_RDC.off(keep_radio_on);
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  if(NETSTACK_RDC.channel_check_interval) {
    return NETSTACK_RDC.channel_check_interval();
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  seqno = rand();
  memb_init(&packet_memb);
  process_start(&csma_process, NULL);
}
/*---------------------------------------------------------------------------*/
const struct mac_driver csma_driver = {
  "CSMA",
  init,
  send_packet,
  input_packet,
  on,
  off,
  channel_check_interval,
};
/*---------------------------------------------------------------------------*/
