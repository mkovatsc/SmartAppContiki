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
 * $Id: contikimac.c,v 1.46 2011/01/09 21:07:01 adamdunkels Exp $
 */

/**
 * \file
 *         The Contiki power-saving MAC protocol (ContikiMAC)
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Niclas Finne <nfi@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */

/* LOG
 * added burst mode
 * added multi channel
 * check ack seqno
 * check ret == ok before phase_update
 */

#include "net/netstack.h"
#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "lib/random.h"
#include "net/mac/contikimac.h"
#include "net/rime.h"
#include "sys/compower.h"
#include "sys/pt.h"
#include "sys/rtimer.h"
#include "net/mac/csma.h"
#include "static-routing.h"

/*#include "cooja-debug.h"*/
#include "contiki-conf.h"

#ifdef EXPERIMENT_SETUP
#include "experiment-setup.h"
#endif

#include <string.h>

#ifndef WITH_PHASE_OPTIMIZATION
#define WITH_PHASE_OPTIMIZATION      1
#endif
#ifndef WITH_STREAMING
#define WITH_STREAMING               0
#endif
#ifndef WITH_CONTIKIMAC_HEADER
#define WITH_CONTIKIMAC_HEADER       1
#endif
#ifndef WITH_FAST_SLEEP
#define WITH_FAST_SLEEP              1
#endif

struct announcement_data {
  uint16_t id;
  uint16_t value;
};

/* The maximum number of announcements in a single announcement
   message - may need to be increased in the future. */
#define ANNOUNCEMENT_MAX 10

#if WITH_CONTIKIMAC_HEADER
#define CONTIKIMAC_ID 0x00

struct hdr {
  uint8_t id;
  uint8_t len;
};
#endif /* WITH_CONTIKIMAC_HEADER */

/* The structure of the announcement messages. */
struct announcement_msg {
  uint8_t announcement_magic[2];
  uint16_t num;
  struct announcement_data data[ANNOUNCEMENT_MAX];
};

#define ANNOUNCEMENT_MAGIC1 0xAD
#define ANNOUNCEMENT_MAGIC2 0xAD

/* The length of the header of the announcement message, i.e., the
   "num" field in the struct. */
#define ANNOUNCEMENT_MSG_HEADERLEN (sizeof(uint16_t) * 2)

#ifdef CONTIKIMAC_CONF_CYCLE_TIME
#define CYCLE_TIME (CONTIKIMAC_CONF_CYCLE_TIME)
#else
#define CYCLE_TIME (RTIMER_ARCH_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE)
#endif


/* ContikiMAC performs periodic channel checks. Each channel check
   consists of two or more CCA checks. CCA_COUNT_MAX is the number of
   CCAs to be done for each periodic channel check. The default is
   two.*/
#define CCA_COUNT_MAX                      2
/* based on the time between consective packets of a burst */
#define CCA_COUNT_MAX_BURST                 8

/* CCA_CHECK_TIME is the time it takes to perform a CCA check. */
#define CCA_CHECK_TIME                     RTIMER_ARCH_SECOND / 8192

/* CCA_SLEEP_TIME is the time between two successive CCA checks. */
#define CCA_SLEEP_TIME                     RTIMER_ARCH_SECOND / 2000

/* CHECK_TIME is the total time it takes to perform CCA_COUNT_MAX
   CCAs. */
#define CHECK_TIME                         (CCA_COUNT_MAX * (CCA_CHECK_TIME + CCA_SLEEP_TIME))

/* LISTEN_TIME_AFTER_PACKET_DETECTED is the time that we keep checking
   for activity after a potential packet has been detected by a CCA
   check. */
#define LISTEN_TIME_AFTER_PACKET_DETECTED  RTIMER_ARCH_SECOND / 80

/* MAX_SILENCE_PERIODS is the maximum amount of periods (a period is
   CCA_CHECK_TIME + CCA_SLEEP_TIME) that we allow to be silent before
   we turn of the radio. */
#define MAX_SILENCE_PERIODS                5

/* MAX_NONACTIVITY_PERIODS is the maximum number of periods we allow
   the radio to be turned on without any packet being received, when
   WITH_FAST_SLEEP is enabled. */
//#define MAX_NONACTIVITY_PERIODS            10
#define MAX_NONACTIVITY_PERIODS            24



/* STROBE_TIME is the maximum amount of time a transmitted packet
   should be repeatedly transmitted as part of a transmission. */
#define STROBE_TIME                        (CYCLE_TIME + 2 * CHECK_TIME)

/* GUARD_TIME is the time before the expected phase of a neighbor that
   a transmitted should begin transmitting packets. */
#if CCA_BEFORE_BURST
#define GUARD_TIME                         12 * CHECK_TIME
#else
#define GUARD_TIME                         9 * CHECK_TIME
#endif


/* INTER_PACKET_INTERVAL is the interval between two successive packet transmissions */
//#define INTER_PACKET_INTERVAL              RTIMER_ARCH_SECOND / 5000
// increased because no cc2420 cca
#define INTER_PACKET_INTERVAL              1+(RTIMER_ARCH_SECOND / 5000)

/* AFTER_ACK_DETECTECT_WAIT_TIME is the time to wait after a potential
   ACK packet has been detected until we can read it out from the
   radio. */
//#define AFTER_ACK_DETECTECT_WAIT_TIME      RTIMER_ARCH_SECOND / 1500
#define AFTER_ACK_DETECTECT_WAIT_TIME      RTIMER_ARCH_SECOND / 4000

/* MAX_PHASE_STROBE_TIME is the time that we transmit repeated packets
   to a neighbor for which we have a phase lock. */
#define MAX_PHASE_STROBE_TIME              RTIMER_ARCH_SECOND / 60
//#define MAX_PHASE_STROBE_TIME              RTIMER_ARCH_SECOND / 40

/* SHORTEST_PACKET_SIZE is the shortest packet that ContikiMAC
   allows. Packets have to be a certain size to be able to be detected
   by two consecutive CCA checks, and here is where we define this
   shortest size. */
#define SHORTEST_PACKET_SIZE               43

//SD burst off and on periods
#define MAX_COLLISIONS_WHEN_SENDING 1

#define INTER_BURST_ROOM                      1
#define BURST_OFF_TIME                        (RTIMER_ARCH_SECOND / 700)
#define BURST_ON_TIME                         16 * CHECK_TIME
#define BURST_TXGUART_TIME                    32 * CHECK_TIME
//#define EXTRA_PHASE_STROBE_TIME              (RTIMER_ARCH_SECOND / 240)
static enum{BURST_RX_OFF, BURST_RX_ON} recv_burst_state = BURST_RX_OFF;
static enum{BURST_TX_OFF, BURST_TX_FIRST, BURST_TX_ON, BURST_TX_LAST} send_burst_state = BURST_TX_OFF;
static uint8_t is_bursting_to;
static rtimer_clock_t burst_start;
static rtimer_clock_t burst_until;
static rtimer_clock_t encounter_time;
static struct queuebuf *next_qbuf;
void (*cc2420_it_callback)();

//static rtimer_clock_t it_time;
//static rtimer_clock_t next_rt_time;
//extern int curr_ns_target;
static rtimer_clock_t node_phase;
#define NEXT_CYCLE_TIME ((RTIMER_NOW() & ~(CYCLE_TIME - 1)) + CYCLE_TIME)

/* The cycle time for announcements. */
#ifdef ANNOUNCEMENT_CONF_PERIOD
#define ANNOUNCEMENT_PERIOD ANNOUNCEMENT_CONF_PERIOD
#else /* ANNOUNCEMENT_CONF_PERIOD */
#define ANNOUNCEMENT_PERIOD 1 * CLOCK_SECOND
#endif /* ANNOUNCEMENT_CONF_PERIOD */

/* The time before sending an announcement within one announcement
   cycle. */
#define ANNOUNCEMENT_TIME (random_rand() % (ANNOUNCEMENT_PERIOD))


#define ACK_LEN 3

#include <stdio.h>
static struct rtimer rt;
static struct pt pt;

static volatile uint8_t contikimac_is_on = 0;
static volatile uint8_t contikimac_keep_radio_on = 0;

static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;

#define PRINTFB(...)
//#define PRINTFB(...) printf(__VA_ARGS__)

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTDEBUG(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#define PRINTDEBUG(...)
#endif

#if CONTIKIMAC_CONF_ANNOUNCEMENTS
/* Timers for keeping track of when to send announcements. */
static struct ctimer announcement_cycle_ctimer, announcement_ctimer;

static int announcement_radio_txpower;
#endif /* CONTIKIMAC_CONF_ANNOUNCEMENTS */

/* Flag that is used to keep track of whether or not we are snooping
   for announcements from neighbors. */
static volatile uint8_t is_snooping;

#if CONTIKIMAC_CONF_COMPOWER
static struct compower_activity current_packet;
#endif /* CONTIKIMAC_CONF_COMPOWER */

#if WITH_PHASE_OPTIMIZATION

#include "net/mac/phase.h"

#ifndef MAX_PHASE_NEIGHBORS
#define MAX_PHASE_NEIGHBORS 30
#endif

PHASE_LIST(phase_list, MAX_PHASE_NEIGHBORS);

#endif /* WITH_PHASE_OPTIMIZATION */

static uint8_t collisions;
static volatile uint8_t is_streaming;
static rimeaddr_t is_streaming_to, is_streaming_to_too;
static volatile rtimer_clock_t stream_until;

#define DEFAULT_STREAM_TIME (1 * CYCLE_TIME)

#ifndef MIN
#define MIN(a, b) ((a) < (b)? (a) : (b))
#endif /* MIN */

struct seqno {
  rimeaddr_t sender;
  uint8_t seqno;
};

#define MAX_SEQNOS 8
static struct seqno received_seqnos[MAX_SEQNOS];

#if CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT
static struct timer broadcast_rate_timer;
static int broadcast_rate_counter;
#endif /* CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT */

/*---------------------------------------------------------------------------*/
int contikimac_is_busy() {
  return recv_burst_state != BURST_RX_OFF || send_burst_state != BURST_TX_OFF;
}
/*---------------------------------------------------------------------------*/
static void
on(void)
{
  if(contikimac_is_on && radio_is_on == 0) {
    radio_is_on = 1;
    NETSTACK_RADIO.on();
  }
}
/*---------------------------------------------------------------------------*/
static void
off(void)
{
  if(contikimac_is_on && radio_is_on != 0 && is_streaming == 0 &&
     contikimac_keep_radio_on == 0
     /* && is_snooping == 0*/) {
    radio_is_on = 0;
    NETSTACK_RADIO.off();
  }
  if(contikimac_keep_radio_on) {
    NETSTACK_RADIO.off();
    NETSTACK_RADIO.on();
  }
}
/*---------------------------------------------------------------------------*/
static volatile rtimer_clock_t cycle_start;
static char powercycle(struct rtimer *t, void *ptr);
static void
schedule_powercycle(struct rtimer *t, rtimer_clock_t time)
{
  int r;

  if(contikimac_is_on) {

    if(RTIMER_CLOCK_LT(RTIMER_TIME(t) + time, RTIMER_NOW() + 2)) {
      time = RTIMER_NOW() - RTIMER_TIME(t) + 2;
    }

#if NURTIMER
    r = rtimer_reschedule(t, time, 1);
#else
    r = rtimer_set(t, RTIMER_TIME(t) + time, 1,
                   (void (*)(struct rtimer *, void *))powercycle, NULL);
#endif
    if(r != RTIMER_OK) {
      printf("schedule_powercycle: could not set rtimer\n");
    }
  }
}
static void
schedule_powercycle_fixed(struct rtimer *t, rtimer_clock_t fixed_time)
{
  int r;

  if(contikimac_is_on) {

    if(RTIMER_CLOCK_LT(fixed_time, RTIMER_NOW() + 1)) {
      fixed_time = RTIMER_NOW() + 1;
    }

#if NURTIMER
    r = rtimer_reschedule(t, RTIMER_TIME(t) - time, 1);
#else
    r = rtimer_set(t, fixed_time, 1,
                   (void (*)(struct rtimer *, void *))powercycle, NULL);
#endif
    if(r != RTIMER_OK) {
      printf("schedule_powercycle: could not set rtimer\n");
    }
  }
}
static void
powercycle_turn_radio_off(void)
{
  if(we_are_sending == 0) {
    off();
  }
}
static void
powercycle_turn_radio_on(void)
{
  if(we_are_sending == 0) {
    on();
  }
}
static char
powercycle(struct rtimer *t, void *ptr)
{
  PT_BEGIN(&pt);

  cycle_start = RTIMER_NOW();
  
  while(1) {
    static uint8_t packet_seen;
    static rtimer_clock_t t0;
    static uint8_t count;

    cycle_start += CYCLE_TIME;

    if(WITH_STREAMING && is_streaming) {
#if NURTIMER
      if(!RTIMER_CLOCK_LT(cycle_start, RTIMER_NOW(), stream_until))
#else
        if(!RTIMER_CLOCK_LT(RTIMER_NOW(), stream_until))
#endif
          {
            is_streaming = 0;
            rimeaddr_copy(&is_streaming_to, &rimeaddr_null);
            rimeaddr_copy(&is_streaming_to_too, &rimeaddr_null);
          }
    }

    packet_seen = 0;

    do {
      for(count = 0; count < CCA_COUNT_MAX; ++count) {
        t0 = RTIMER_NOW();
        if(we_are_sending == 0
#if CONTIKIMAC_PREVENTIVE_LPP
             && get_queuebuf_available() > 2 // leave some room for other use of qbuf
#endif
            ) {
          powercycle_turn_radio_on();
          //          schedule_powercycle_fixed(t, t0 + CCA_CHECK_TIME);
#if 0
#if NURTIMER
          while(RTIMER_CLOCK_LT(t0, RTIMER_NOW(), t0 + CCA_CHECK_TIME));
#else
          while(RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + CCA_CHECK_TIME));
#endif
#endif /* 0 */
          /* Check if a packet is seen in the air. If so, we keep the
             radio on for a while (LISTEN_TIME_AFTER_PACKET_DETECTED) to
             be able to receive the packet. We also continuously check
             the radio medium to make sure that we wasn't woken up by a
             false positive: a spurious radio interference that was not
             caused by an incoming packet. */
          if(NETSTACK_RADIO.channel_clear() == 0
#if CMAC_RSSI_HACK
              && cc2420_rssi() > -20
#endif
              ) {
            packet_seen = 1;
            break;
          }
          powercycle_turn_radio_off();
        }
        //        schedule_powercycle_fixed(t, t0 + CCA_CHECK_TIME + CCA_SLEEP_TIME);
        schedule_powercycle_fixed(t, RTIMER_NOW() + CCA_SLEEP_TIME);
        PT_YIELD(&pt);
      }

      if(packet_seen) {
        static rtimer_clock_t start;
        static uint8_t silence_periods, periods;
        start = RTIMER_NOW();

        periods = silence_periods = 0;
        while(we_are_sending == 0 && radio_is_on &&
            RTIMER_CLOCK_LT(RTIMER_NOW(),
                (start + LISTEN_TIME_AFTER_PACKET_DETECTED))) {

          /* Check for a number of consecutive periods of
               non-activity. If we see two such periods, we turn the
               radio off. Also, if a packet has been successfully
               received (as indicated by the
               NETSTACK_RADIO.pending_packet() function), we stop
               snooping. */
          if(NETSTACK_RADIO.channel_clear()) {
            ++silence_periods;
          } else {
            silence_periods = 0;
          }

          ++periods;

          if(NETSTACK_RADIO.receiving_packet()) {
            silence_periods = 0;
          }
          if(silence_periods > MAX_SILENCE_PERIODS) {
            powercycle_turn_radio_off();
#if CONTIKIMAC_CONF_COMPOWER
            compower_accumulate(&compower_idle_activity);
#endif /* CONTIKIMAC_CONF_COMPOWER */
            break;
          }
          if(WITH_FAST_SLEEP &&
              periods > MAX_NONACTIVITY_PERIODS &&
              !(NETSTACK_RADIO.receiving_packet() ||
                  NETSTACK_RADIO.pending_packet())) {
            powercycle_turn_radio_off();
#if CONTIKIMAC_CONF_COMPOWER
            compower_accumulate(&compower_idle_activity);
#endif /* CONTIKIMAC_CONF_COMPOWER */
            break;
          }
          if(NETSTACK_RADIO.pending_packet()) {
            break;
          }

          schedule_powercycle(t, CCA_CHECK_TIME + CCA_SLEEP_TIME);
          PT_YIELD(&pt);
        }
        if(radio_is_on) {
          if(!(NETSTACK_RADIO.receiving_packet() ||
              NETSTACK_RADIO.pending_packet()) ||
              !RTIMER_CLOCK_LT(RTIMER_NOW(),
                  (start + LISTEN_TIME_AFTER_PACKET_DETECTED))) {
            powercycle_turn_radio_off();
#if CONTIKIMAC_CONF_COMPOWER
            compower_accumulate(&compower_idle_activity);
#endif /* CONTIKIMAC_CONF_COMPOWER */
          }
        }
      } else {
#if CONTIKIMAC_CONF_COMPOWER
        compower_accumulate(&compower_idle_activity);
#endif /* CONTIKIMAC_CONF_COMPOWER */
      }
    } while(is_snooping &&
        RTIMER_CLOCK_LT(RTIMER_NOW() - cycle_start, CYCLE_TIME - CHECK_TIME));

//    if(RTIMER_CLOCK_LT(RTIMER_NOW() - cycle_start, CYCLE_TIME)) {
//      /*      schedule_powercycle(t, CYCLE_TIME - (RTIMER_NOW() - cycle_start));*/
//      schedule_powercycle_fixed(t, CYCLE_TIME + cycle_start);
//      /*      printf("cycle_start 0x%02x now 0x%02x wait 0x%02x\n",
//              cycle_start, RTIMER_NOW(), CYCLE_TIME - (RTIMER_NOW() - cycle_start));*/
//      PT_YIELD(&pt);
//    }
    rtimer_set(&rt, NEXT_CYCLE_TIME, 1, (void (*)(struct rtimer *, void *))powercycle, NULL);
    PT_YIELD(&pt);
  }

  PT_END(&pt);
}
/*---------------------------------------------------------------------------*/
#if CONTIKIMAC_CONF_ANNOUNCEMENTS
static int
parse_announcements(void)
{
  /* Parse incoming announcements */
  struct announcement_msg adata;
  const rimeaddr_t *from;
  int i;

  memcpy(&adata, packetbuf_dataptr(),
         MIN(packetbuf_datalen(), sizeof(adata)));
  from = packetbuf_addr(PACKETBUF_ADDR_SENDER);

  /*  printf("%d.%d: probe from %d.%d with %d announcements\n",
     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
     from->u8[0], from->u8[1], adata.num); */
  /*  for(i = 0; i < packetbuf_datalen(); ++i) {
     printf("%02x ", ((uint8_t *)packetbuf_dataptr())[i]);
     }
     printf("\n"); */

  if(adata.num / sizeof(struct announcement_data) > sizeof(struct announcement_msg)) {
    /* Sanity check. The number of announcements is too large -
       corrupt packet has been received. */
    return 0;
  }

  for(i = 0; i < adata.num; ++i) {
    /*    printf("%d.%d: announcement %d: %d\n",
       rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
       adata.data[i].id,
       adata.data[i].value); */

    announcement_heard(from, adata.data[i].id, adata.data[i].value);
  }
  return i;
}
/*---------------------------------------------------------------------------*/
static int
format_announcement(char *hdr)
{
  struct announcement_msg adata;
  struct announcement *a;

  /* Construct the announcements */
  /*  adata = (struct announcement_msg *)hdr; */

  adata.announcement_magic[0] = ANNOUNCEMENT_MAGIC1;
  adata.announcement_magic[1] = ANNOUNCEMENT_MAGIC2;
  adata.num = 0;
  for(a = announcement_list();
      a != NULL && adata.num < ANNOUNCEMENT_MAX;
      a = a->next) {
    if(a->has_value) {
      adata.data[adata.num].id = a->id;
      adata.data[adata.num].value = a->value;
      adata.num++;
    }
  }

  memcpy(hdr, &adata, sizeof(struct announcement_msg));

  if(adata.num > 0) {
    return ANNOUNCEMENT_MSG_HEADERLEN +
      sizeof(struct announcement_data) * adata.num;
  } else {
    return 0;
  }
}
#endif /* CONTIKIMAC_CONF_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
static int
broadcast_rate_drop(void)
{
#if CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT
  if(!timer_expired(&broadcast_rate_timer)) {
    broadcast_rate_counter++;
    if(broadcast_rate_counter < CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT) {
      return 0;
    } else {
      return 1;
    }
  } else {
    timer_set(&broadcast_rate_timer, CLOCK_SECOND);
    broadcast_rate_counter = 0;
    return 0;
  }
#else /* CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT */
  return 0;
#endif /* CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT */
}
/*---------------------------------------------------------------------------*/
static int
transmit_strobes(int is_broadcast, int is_known_receiver, int transmit_len) {
  int i;
  int strobes;
  uint8_t got_strobe_ack;
  uint8_t contikimac_was_on;
  rtimer_clock_t t0;
  uint8_t seqno;
  rtimer_clock_t previous_txtime = 0;
  rtimer_clock_t max_phase_strobe_time;

  /* Switch off the radio to ensure that we didn't start sending while
    the radio was doing a channel check. */
  off();

  strobes = 0;

  /* Send a train of strobes until the receiver answers with an ACK. */
  collisions = 0;

  got_strobe_ack = 0;

  /* Set contikimac_is_on to one to allow the on() and off() functions
    to control the radio. We restore the old value of
    contikimac_is_on when we are done. */
  contikimac_was_on = contikimac_is_on;
  contikimac_is_on = 1;
  static int cptcol;
  static int cptsent;
#if CCA_BEFORE_BURST
  if(is_streaming == 0 && (send_burst_state == BURST_TX_OFF || send_burst_state == BURST_TX_FIRST)) {
    /* Check if there are any transmissions by others. */
    for(i = 0; i < CCA_COUNT_MAX_BURST; ++i) {
      t0 = RTIMER_NOW();
      on();
#if NURTIMER
      while(RTIMER_CLOCK_LT(t0, RTIMER_NOW(), t0 + CCA_CHECK_TIME));
#else
      while(RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + CCA_CHECK_TIME)) { }
#endif
      if(NETSTACK_RADIO.channel_clear() == 0) {
        collisions++;
        off();
        break;
      }
      off();
      t0 = RTIMER_NOW();
#if NURTIMER
      while(RTIMER_CLOCK_LT(t0, RTIMER_NOW(), t0 + CCA_SLEEP_TIME));
#else
      while(RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + CCA_SLEEP_TIME)) { }
#endif
    }
//    printf("%d (%d %d)\n", collisions, cptsent, cptcol);
    cptsent=0;
  }

  if(collisions > 0) {
    we_are_sending = 0;
    cptcol++;
    off();
    PRINTF("contikimac: collisions before sending\n");
    contikimac_is_on = contikimac_was_on;
    return MAC_TX_COLLISION;
  }
  cptcol = 0;
  cptsent++;
#endif

  if(!is_broadcast) {
    on();
  }
  watchdog_periodic();
  t0 = RTIMER_NOW();
  seqno = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);

#if NURTIMER
  for(strobes = 0, collisions = 0;
      got_strobe_ack == 0 && collisions <= MAX_COLLISIONS_WHEN_SENDING &&
          RTIMER_CLOCK_LT(t0, RTIMER_NOW(), t0 + STROBE_TIME); strobes++) {
#else
  for(strobes = 0, collisions = 0;
      got_strobe_ack == 0 && collisions <= MAX_COLLISIONS_WHEN_SENDING &&
          RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + STROBE_TIME); strobes++) {
#endif

   watchdog_periodic();

   max_phase_strobe_time = MAX_PHASE_STROBE_TIME /*+ packetbuf_attr(PACKETBUF_ATTR_NUM_REXMIT) * EXTRA_PHASE_STROBE_TIME*/;

   if(is_known_receiver && !RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + max_phase_strobe_time)) {
     PRINTF("miss to %d\n", packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0]);
     break;
   }
    /*if(is_known_receiver && !RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + MAX_PHASE_STROBE_TIME)) {
      PRINTF("miss to %d\n", packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0]);
      break;
    }*/

    previous_txtime = RTIMER_NOW();
    {
      rtimer_clock_t wt;
      rtimer_clock_t txtime;
      int ret;
      int len = 0;

      txtime = RTIMER_NOW();

      ret = NETSTACK_RADIO.transmit(transmit_len);

      wt = RTIMER_NOW();
#if NURTIMER
      while(RTIMER_CLOCK_LT(wt, RTIMER_NOW(), wt + INTER_PACKET_INTERVAL));
#else
      while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + INTER_PACKET_INTERVAL)) { }
#endif

      if(!is_broadcast && (NETSTACK_RADIO.receiving_packet() ||
          NETSTACK_RADIO.pending_packet() ||
          NETSTACK_RADIO.channel_clear() == 0)) {
        uint8_t ackbuf[ACK_LEN];
        wt = RTIMER_NOW();
#if NURTIMER
        while(RTIMER_CLOCK_LT(wt, RTIMER_NOW(), wt + AFTER_ACK_DETECTECT_WAIT_TIME));
#else
        while(RTIMER_CLOCK_LT(RTIMER_NOW(), wt + AFTER_ACK_DETECTECT_WAIT_TIME)) { }
#endif
        len = NETSTACK_RADIO.read(ackbuf, ACK_LEN);

        if(len == ACK_LEN) {
          if(seqno == ackbuf[ACK_LEN-1]) {
            got_strobe_ack = 1;
            if(strobes > 0) {
              /* do not consider exceptionaly early successful tranmissions,
               * which cannot occur after usual contikimac channel checks by the receiver */
              encounter_time = previous_txtime;
            } else {
              encounter_time = 0;
            }
            break;
          } else {
            COOJA_DEBUG_STR("contikimac: drop ACK because of a wrong seqno");
          }
        } else {
          PRINTF("contikimac: collisions while sending\n");
//          collisions++;
//          COOJA_DEBUG_STR("collisions while sending");
//          COOJA_DEBUG_INT(len);
        }
      }
      previous_txtime = txtime;
    }
  }

#if WITH_LETHARGY
  off();
#endif

  PRINTF("contikimac: send (strobes=%u, len=%u, %s, %s), done\n", strobes,
      packetbuf_totlen(),
      got_strobe_ack ? "ack" : "no ack",
          collisions ? "collision" : "no collision");

#if CONTIKIMAC_CONF_COMPOWER
  /* Accumulate the power consumption for the packet transmission. */
  compower_accumulate(&current_packet);

  /* Convert the accumulated power consumption for the transmitted
    packet to packet attributes so that the higher levels can keep
    track of the amount of energy spent on transmitting the
    packet. */
  compower_attrconv(&current_packet);

  /* Clear the accumulated power consumption so that it is ready for
    the next packet. */
  compower_clear(&current_packet);
#endif /* CONTIKIMAC_CONF_COMPOWER */

  contikimac_is_on = contikimac_was_on;

#if WITH_PHASE_OPTIMIZATION
  if(is_known_receiver && got_strobe_ack) {
    PRINTF("no miss %d wake-ups %d\n", packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
           strobes);
  }
#endif /* WITH_PHASE_OPTIMIZATION */

  /* Determine the return value that we will return from the
     function. We must pass this value to the phase module before we
     return from the function.  */

  if(collisions > MAX_COLLISIONS_WHEN_SENDING) {
    COOJA_DEBUG_CINT('x', strobes);
    return MAC_TX_COLLISION;
  } else if(!is_broadcast && !got_strobe_ack) {
    COOJA_DEBUG_CINT('x', strobes);
    return MAC_TX_NOACK;
  } else {
    COOJA_DEBUG_CINT('o', strobes+1);
    return MAC_TX_OK;
  }
}
/*---------------------------------------------------------------------------*/
static int
send_packet(mac_callback_t mac_callback, void *mac_callback_ptr)
{
  int hdrlen;
  uint8_t is_broadcast = 0;
  uint8_t is_reliable = 0;
  uint8_t is_known_receiver = 0;
  int transmit_len;
  int ret;
  uint16_t max_phase_strobe_time;
  int do_update = 0;
  uint8_t queue_length;

  encounter_time = 0;
#if WITH_CONTIKIMAC_HEADER
  struct hdr *chdr;
#endif /* WITH_CONTIKIMAC_HEADER */

  if(packetbuf_totlen() == 0) {
    PRINTF("contikimac: send_packet data len 0\n");
    return MAC_TX_ERR_FATAL;
  }
  //printf("cmac send\n");
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
  if(rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_null)) {
    is_broadcast = 1;
    PRINTDEBUG("contikimac: send broadcast\n");

    if(broadcast_rate_drop()) {
      return MAC_TX_COLLISION;
    }
  } else {
#if UIP_CONF_IPV6
    PRINTDEBUG("contikimac: send unicast to %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[2],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[3],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[4],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[5],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[6],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]);
#else /* UIP_CONF_IPV6 */
    PRINTDEBUG("contikimac: send unicast to %u.%u\n",
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
               packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
#endif /* UIP_CONF_IPV6 */
  }

  is_reliable = packetbuf_attr(PACKETBUF_ATTR_RELIABLE) ||
    packetbuf_attr(PACKETBUF_ATTR_ERELIABLE);
  
  if(WITH_STREAMING) {
    if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
       PACKETBUF_ATTR_PACKET_TYPE_STREAM) {
      if(rimeaddr_cmp(&is_streaming_to, &rimeaddr_null)) {
        rimeaddr_copy(&is_streaming_to,
                      packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
      } else if(!rimeaddr_cmp
                (&is_streaming_to, packetbuf_addr(PACKETBUF_ADDR_RECEIVER))) {
        rimeaddr_copy(&is_streaming_to_too,
                      packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
      }
      stream_until = RTIMER_NOW() + DEFAULT_STREAM_TIME;
      is_streaming = 1;
    } else {
      is_streaming = 0;
    }
  }

//  if(is_streaming) {
//    packetbuf_set_attr(PACKETBUF_ATTR_PENDING, 1);
//  }
  packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);

#if WITH_BURST
    is_bursting_to = packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7];
    queue_length = csma_queue_length(is_bursting_to);
    next_qbuf = csma_queue_next(is_bursting_to);

    switch(send_burst_state) {
    case BURST_TX_OFF:
      if(queue_length > 1) {
        send_burst_state = BURST_TX_FIRST;
        burst_start = RTIMER_NOW();
        burst_until = burst_start + BURST_DURATION;
      }
      break;
    case BURST_TX_FIRST:
    case BURST_TX_ON:
      send_burst_state = BURST_TX_ON;
      if(queue_length == 1 || !RTIMER_CLOCK_LT(RTIMER_NOW(), burst_until)) {
        send_burst_state = BURST_TX_LAST;
      }
      is_known_receiver = 1;
      break;
    default:
      break;
    }
    packetbuf_set_attr(PACKETBUF_ATTR_PENDING, send_burst_state == BURST_TX_FIRST || send_burst_state == BURST_TX_ON);
#else
    next_qbuf = NULL;
#endif

#if WITH_CONTIKIMAC_HEADER
    hdrlen = packetbuf_totlen();
    if(packetbuf_hdralloc(sizeof(struct hdr)) == 0) {
      /* Failed to allocate space for contikimac header */
      PRINTF("contikimac: send failed, too large header\n");
      return MAC_TX_ERR_FATAL;
    }
    chdr = packetbuf_hdrptr();
    chdr->id = CONTIKIMAC_ID;
    chdr->len = hdrlen;

    /* Create the MAC header for the data packet. */
    hdrlen = NETSTACK_FRAMER.create();
    if(hdrlen == 0) {
      /* Failed to send */
      PRINTF("contikimac: send failed, too large header\n");
      packetbuf_hdr_remove(sizeof(struct hdr));
      return MAC_TX_ERR_FATAL;
    }
    hdrlen += sizeof(struct hdr);
#else
    /* Create the MAC header for the data packet. */
    hdrlen = NETSTACK_FRAMER.create();
    if(hdrlen == 0) {
      /* Failed to send */
      PRINTF("contikimac: send failed, too large header\n");
      return MAC_TX_ERR_FATAL;
    }
#endif

  /* Make sure that the packet is longer or equal to the shortest
     packet length. */
  transmit_len = packetbuf_totlen();

  if(transmit_len < SHORTEST_PACKET_SIZE) {
#if 0
    /* Pad with zeroes */
    uint8_t *ptr;
    ptr = packetbuf_dataptr();
    memset(ptr + packetbuf_datalen(), 0, SHORTEST_PACKET_SIZE - packetbuf_totlen());
#endif

    PRINTF("contikimac: shorter than shortest (%d)\n", packetbuf_totlen());
    transmit_len = SHORTEST_PACKET_SIZE;
  }
  packetbuf_compact();

  NETSTACK_RADIO.prepare(packetbuf_hdrptr(), transmit_len);

  /* Remove the MAC-layer header since it will be recreated next time around. */
  packetbuf_hdr_remove(hdrlen);

  if(!is_broadcast && !is_streaming && (send_burst_state == BURST_TX_OFF || send_burst_state == BURST_TX_FIRST)) {
#if WITH_PHASE_OPTIMIZATION
    ret = phase_wait(&phase_list, packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     CYCLE_TIME, GUARD_TIME,
                     mac_callback, mac_callback_ptr, recv_burst_state == BURST_RX_ON);
    if(ret == PHASE_DEFERRED) {
      return MAC_TX_DEFERRED;
    }
    if(ret != PHASE_UNKNOWN) {
      is_known_receiver = 1;
    }
    do_update = 1;
#endif /* WITH_PHASE_OPTIMIZATION */ 
  }

  /* By setting we_are_sending to one, we ensure that the rtimer
     powercycle interrupt do not interfere with us sending the packet. */
  we_are_sending = 1;

  /* If we have a pending packet in the radio, we should not send now,
     because we will trash the received packet. Instead, we signal
     that we have a collision, which lets the packet be received. This
     packet will be retransmitted later by the MAC protocol
     instread. */
  if(NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet()) {
    we_are_sending = 0;
    PRINTF("contikimac: collision receiving %d, pending %d\n",
           NETSTACK_RADIO.receiving_packet(), NETSTACK_RADIO.pending_packet());
    return MAC_TX_COLLISION;
  }

  ret = transmit_strobes(is_broadcast, is_known_receiver, transmit_len);

  if(send_burst_state == BURST_TX_LAST || send_burst_state == BURST_TX_OFF) {
    we_are_sending = 0;
  }

#if WITH_PHASE_OPTIMIZATION
  if(!is_broadcast) {
    if(do_update == 1) {
      phase_update(&phase_list, packetbuf_addr(PACKETBUF_ADDR_RECEIVER), encounter_time,
          CYCLE_TIME, GUARD_TIME, ret);
    }
  }
#endif /* WITH_PHASE_OPTIMIZATION */

    if(send_burst_state == BURST_TX_LAST) {
      rtimer_clock_t burst_duration = RTIMER_NOW() - burst_start;
      if(burst_duration > 0.5*CYCLE_TIME) {
        phase_block_until(&phase_list, packetbuf_addr(PACKETBUF_ADDR_RECEIVER), RTIMER_NOW() + INTER_BURST_ROOM * burst_duration);
      }
      send_burst_state = BURST_TX_OFF;
      burst_off(NULL);
    }
  return ret;
}
/*---------------------------------------------------------------------------*/
static void
burst_off(void *ptr)
{
  off(); /* turn on the radio for the next packet of burst, and reschedule powercycle */
  compower_accumulate(&compower_idle_activity);
  recv_burst_state = BURST_RX_OFF;
  send_burst_state = BURST_TX_OFF;
  we_are_sending = 0;
  cc2420_it_callback = NULL;
  rtimer_set(&rt, NEXT_CYCLE_TIME, 1, (void (*)(struct rtimer *, void *))powercycle, NULL);
}
/*---------------------------------------------------------------------------*/
void
burst_recv_on()
{
  on();
  rtimer_set(&rt, RTIMER_NOW() + BURST_ON_TIME, 1, burst_off, NULL);
}
/*---------------------------------------------------------------------------*/
void
burst_recv_off()
{
#if WITH_LETHARGY
  off();
#endif
  rtimer_set(&rt, RTIMER_NOW() + BURST_OFF_TIME, 1, burst_recv_on, NULL);
}
/*---------------------------------------------------------------------------*/
static void
qsend_packet(mac_callback_t sent, void *ptr)
{
  int ret;

#if GET_LATENCY
    if(node_rank == 1 && packetbuf_datalen() > 100) {
      int seq;
      seq = ((unsigned char*)packetbuf_dataptr())[26] << 8 | ((unsigned char*)packetbuf_dataptr())[27];
      if(!WITH_BURST || seq % 16 == 0) printf("seq: %d\n", seq);
    }
#endif

#if WITH_BURST
  if(send_burst_state != BURST_TX_OFF && is_bursting_to != packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[7]) {
    ret = MAC_TX_COLLISION; /* crappy, we should rather defer to the next RDC period */
  } else {
    ret = send_packet(sent, ptr);
    if(ret != MAC_TX_OK) {
      send_burst_state = BURST_TX_OFF;
      we_are_sending = 0;
    }
    PRINTFB("send_packet %d %d\n", ret, send_burst_state);
  }
#else
  ret = send_packet(sent, ptr);
#endif

  if(send_burst_state != BURST_TX_OFF && !contikimac_keep_radio_on) {
    rtimer_set(&rt, RTIMER_NOW() + BURST_TXGUART_TIME, 1, burst_off, NULL);
  }

  if(ret != MAC_TX_DEFERRED) {
    //    printf("contikimac qsend_packet %p\n", ptr);
    mac_call_sent_callback(sent, ptr, ret, 1);
  }
}
/*---------------------------------------------------------------------------*/
static void
input_packet(void)
{
  /*  printf("cycle_start 0x%02x 0x%02x\n", cycle_start, cycle_start % CYCLE_TIME);*/
  if(packetbuf_totlen() > 0 && NETSTACK_FRAMER.parse()) {
#if !WITH_BURST
    off();
#endif

#if WITH_CONTIKIMAC_HEADER
    struct hdr *chdr;
    chdr = packetbuf_dataptr();
    if(chdr->id != CONTIKIMAC_ID) {
      PRINTF("contikimac: failed to parse hdr (%u)\n", packetbuf_totlen());
      return;
    }

    packetbuf_hdrreduce(sizeof(struct hdr));
    packetbuf_set_datalen(chdr->len);
#endif /* WITH_CONTIKIMAC_HEADER */

    if(packetbuf_datalen() > 0 &&
       packetbuf_totlen() > 0 &&
       (rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &rimeaddr_node_addr) ||
        rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                     &rimeaddr_null))) {

#if WITH_BURST
      if(packetbuf_attr(PACKETBUF_ATTR_PENDING)) {
        if(recv_burst_state == BURST_RX_OFF) {
          recv_burst_state = BURST_RX_ON;
        }
        rtimer_set(&rt, RTIMER_NOW() + BURST_ON_TIME, 1, burst_off, NULL);
        cc2420_it_callback = burst_recv_off;
        /*if(RTIMER_CLOCK_LT(RTIMER_NOW(), it_time + BURST_OFF_TIME)) {
          rtimer_set(&rt, it_time + BURST_OFF_TIME, 1, recv_burst, NULL);
          off();
        } else {
          rtimer_set(&rt, RTIMER_NOW() + BURST_ON_TIME, 1, burst_off, NULL);
        }*/
      } else {
        off();
        cc2420_it_callback = NULL;
        if(recv_burst_state == BURST_RX_ON) {
          rtimer_set(&rt, NEXT_CYCLE_TIME, 1, (void (*)(struct rtimer *, void *))powercycle, NULL);
        }
        recv_burst_state = BURST_RX_OFF;
      }
#endif
      /* This is a regular packet that is destined to us or to the
         broadcast address. */

#if CONTIKIMAC_CONF_ANNOUNCEMENTS
      {
        struct announcement_msg *hdr = packetbuf_dataptr();
        uint8_t magic[2];
        memcpy(magic, hdr->announcement_magic, 2);
        if(magic[0] == ANNOUNCEMENT_MAGIC1 &&
           magic[1] == ANNOUNCEMENT_MAGIC2) {
          parse_announcements();
        }
      }
#endif /* CONTIKIMAC_CONF_ANNOUNCEMENTS */

//#if WITH_PHASE_OPTIMIZATION
      /* If the sender has set its pending flag, it has its radio
         turned on and we should drop the phase estimation that we
         have from before. */
//      if(packetbuf_attr(PACKETBUF_ATTR_PENDING)) {
//        phase_remove(&phase_list, packetbuf_addr(PACKETBUF_ADDR_SENDER));
//      }
//#endif /* WITH_PHASE_OPTIMIZATION */

      /* Check for duplicate packet by comparing the sequence number
         of the incoming packet with the last few ones we saw. */
      {
        int i;
        for(i = 0; i < MAX_SEQNOS; ++i) {
          if(packetbuf_attr(PACKETBUF_ATTR_PACKET_ID) == received_seqnos[i].seqno &&
             rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_SENDER),
                          &received_seqnos[i].sender)) {
            /* Drop the packet. */
            /*        printf("Drop duplicate ContikiMAC layer packet\n");*/
            //COOJA_DEBUG_STR("Drop duplicate ContikiMAC layer packet");
                    //printf("Drop duplicate ContikiMAC layer packet %d\n", packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));
            return;
          }
        }
        for(i = MAX_SEQNOS - 1; i > 0; --i) {
          memcpy(&received_seqnos[i], &received_seqnos[i - 1],
                 sizeof(struct seqno));
        }
        received_seqnos[0].seqno = packetbuf_attr(PACKETBUF_ATTR_PACKET_ID);
        rimeaddr_copy(&received_seqnos[0].sender,
                      packetbuf_addr(PACKETBUF_ADDR_SENDER));
      }

#if CONTIKIMAC_CONF_COMPOWER
      /* Accumulate the power consumption for the packet reception. */
      compower_accumulate(&current_packet);
      /* Convert the accumulated power consumption for the received
         packet to packet attributes so that the higher levels can
         keep track of the amount of energy spent on receiving the
         packet. */
      compower_attrconv(&current_packet);

      /* Clear the accumulated power consumption so that it is ready
         for the next packet. */
      compower_clear(&current_packet);
#endif /* CONTIKIMAC_CONF_COMPOWER */

      PRINTDEBUG("contikimac: data (%u)\n", packetbuf_datalen());

      PRINTFB("input %d\n", recv_burst_state);

      NETSTACK_MAC.input();

//      if(get_queuebuf_available() <= 2) { /* needed for enqueue / reassembly*/
//        COOJA_DEBUG_STR("no more memory available, stop recv burst");
//        burst_off(NULL);
//      }

      return;
    } else {
      off();
      PRINTDEBUG("contikimac: data not for us\n");
    }
  } else {
    PRINTF("contikimac: failed to parse (%u)\n", packetbuf_totlen());
  }
}
/*---------------------------------------------------------------------------*/
#if CONTIKIMAC_CONF_ANNOUNCEMENTS
static void
send_announcement(void *ptr)
{
  int announcement_len;
  int transmit_len;
#if WITH_CONTIKIMAC_HEADER
  struct hdr *chdr;
#endif /* WITH_CONTIKIMAC_HEADER */
  
  /* Set up the probe header. */
  packetbuf_clear();
  announcement_len = format_announcement(packetbuf_dataptr());

  if(announcement_len > 0) {
    packetbuf_set_datalen(announcement_len);

    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &rimeaddr_null);
    packetbuf_set_attr(PACKETBUF_ATTR_RADIO_TXPOWER,
                       announcement_radio_txpower);
#if WITH_CONTIKIMAC_HEADER
    transmit_len = packetbuf_totlen();
    if(packetbuf_hdralloc(sizeof(struct hdr)) == 0) {
      /* Failed to allocate space for contikimac header */
      PRINTF("contikimac: send announcement failed, too large header\n");
      return;
    }
    chdr = packetbuf_hdrptr();
    chdr->id = CONTIKIMAC_ID;
    chdr->len = transmit_len;
#endif /* WITH_CONTIKIMAC_HEADER */

    if(NETSTACK_FRAMER.create()) {
      rtimer_clock_t t;
      int i, collisions;
      we_are_sending = 1;

      /* Make sure that the packet is longer or equal to the shorest
         packet length. */
      transmit_len = packetbuf_totlen();
      if(transmit_len < SHORTEST_PACKET_SIZE) {
#if 0
        /* Pad with zeroes */
        uint8_t *ptr;
        ptr = packetbuf_dataptr();
        memset(ptr + packetbuf_datalen(), 0, SHORTEST_PACKET_SIZE - transmit_len);
#endif

        PRINTF("contikimac: shorter than shortest (%d)\n", packetbuf_totlen());
        transmit_len = SHORTEST_PACKET_SIZE;
      }

      collisions = 0;
      /* Check for collisions */
      for(i = 0; i < CCA_COUNT_MAX; ++i) {
        t = RTIMER_NOW();
        on();
#if NURTIMER
        while(RTIMER_CLOCK_LT(t, RTIMER_NOW(), t + CCA_CHECK_TIME));
#else
        while(RTIMER_CLOCK_LT(RTIMER_NOW(), t + CCA_CHECK_TIME));
#endif
        if(NETSTACK_RADIO.channel_clear() == 0) {
          collisions++;
          off();
          break;
        }
        off();
#if NURTIMER
        while(RTIMER_CLOCK_LT(t0, RTIMER_NOW(), t + CCA_SLEEP_TIME + CCA_CHECK_TIME));
#else
        while(RTIMER_CLOCK_LT(RTIMER_NOW(), t + CCA_SLEEP_TIME + CCA_CHECK_TIME)) { }
#endif
      }

      if(collisions == 0) {
        
        NETSTACK_RADIO.prepare(packetbuf_hdrptr(), transmit_len);
        
        NETSTACK_RADIO.transmit(transmit_len);
        t = RTIMER_NOW();
#if NURTIMER
        while(RTIMER_CLOCK_LT(t, RTIMER_NOW(), t + INTER_PACKET_INTERVAL));
#else
        while(RTIMER_CLOCK_LT(RTIMER_NOW(), t + INTER_PACKET_INTERVAL)) { }
#endif
        NETSTACK_RADIO.transmit(transmit_len);
      }
      we_are_sending = 0;
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
cycle_announcement(void *ptr)
{
  ctimer_set(&announcement_ctimer, ANNOUNCEMENT_TIME,
             send_announcement, NULL);
  ctimer_set(&announcement_cycle_ctimer, ANNOUNCEMENT_PERIOD,
             cycle_announcement, NULL);
  if(is_snooping > 0) {
    is_snooping--;
    /*    printf("is_snooping %d\n", is_snooping); */
  }
}
/*---------------------------------------------------------------------------*/
static void
listen_callback(int periods)
{
  printf("Snoop\n");
  is_snooping = periods + 1;
}
#endif /* CONTIKIMAC_CONF_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
void
contikimac_set_announcement_radio_txpower(int txpower)
{
#if CONTIKIMAC_CONF_ANNOUNCEMENTS
  announcement_radio_txpower = txpower;
#endif /* CONTIKIMAC_CONF_ANNOUNCEMENTS */
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  rtimer_clock_t now = RTIMER_NOW();
  node_phase = now & (CYCLE_TIME - 1);

  radio_is_on = 0;
  PT_INIT(&pt);
#if NURTIMER
  rtimer_setup(&rt, RTIMER_HARD,
               (void (*)(struct rtimer *, void *, int status))powercycle,
               NULL);
  rtimer_schedule(&rt, CYCLE_TIME, 1);
#else
  rtimer_set(&rt, NEXT_CYCLE_TIME, 1,
             (void (*)(struct rtimer *, void *))powercycle, NULL);
#endif

  contikimac_is_on = 1;

#if WITH_PHASE_OPTIMIZATION
  phase_init(&phase_list);
#endif /* WITH_PHASE_OPTIMIZATION */

#if CONTIKIMAC_CONF_ANNOUNCEMENTS
  announcement_register_listen_callback(listen_callback);
  ctimer_set(&announcement_cycle_ctimer, ANNOUNCEMENT_TIME,
             cycle_announcement, NULL);
#endif /* CONTIKIMAC_CONF_ANNOUNCEMENTS */

}
/*---------------------------------------------------------------------------*/
static int
turn_on(void)
{
  if(contikimac_is_on == 0) {
    contikimac_is_on = 1;
    contikimac_keep_radio_on = 0;
#if NURTIMER
    rtimer_schedule(&rt, CYCLE_TIME, 1);
#else
    rtimer_set(&rt, RTIMER_NOW() + CYCLE_TIME, 1,
               (void (*)(struct rtimer *, void *))powercycle, NULL);
#endif
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
turn_off(int keep_radio_on)
{
  contikimac_is_on = 0;
  contikimac_keep_radio_on = keep_radio_on;
  if(keep_radio_on) {
    radio_is_on = 1;
    return NETSTACK_RADIO.on();
  } else {
    radio_is_on = 0;
    return NETSTACK_RADIO.off();
  }
}
/*---------------------------------------------------------------------------*/
static unsigned short
duty_cycle(void)
{
  return (1ul * CLOCK_SECOND * CYCLE_TIME) / RTIMER_ARCH_SECOND;
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver contikimac_driver = {
  "ContikiMAC",
  init,
  qsend_packet,
  input_packet,
  turn_on,
  turn_off,
  duty_cycle,
};
/*---------------------------------------------------------------------------*/
uint16_t
contikimac_debug_print(void)
{
  static rtimer_clock_t one_cycle_start;
  printf("Drift %d\n", (one_cycle_start - cycle_start) % CYCLE_TIME);
  one_cycle_start = cycle_start;
  return 0;
}
/*---------------------------------------------------------------------------*/
