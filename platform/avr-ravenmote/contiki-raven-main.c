/*
 * Copyright (c) 2006, Technical University of Munich
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
 * @(#)$$
 */

/**
 * \file
 *         Contiki 2.4 kernel for Jackdaw USB stick
 *
 * \author
 *         Simon Barner <barner@in.tum.de>
 *         David Kopf <dak664@embarqmail.com>
 */

#define DEBUG 0
#if DEBUG
#define PRINTF(FORMAT,args...) printf_P(PSTR(FORMAT),##args)
#else
#define PRINTF(...)
#endif

#include <avr/pgmspace.h>
#include <avr/fuse.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include <dev/watchdog.h>

#include "contiki-raven.h"

#include "loader/symbols-def.h"
#include "loader/symtab.h"

#if RF230BB        //radio driver using contiki core mac
#include "radio/rf230bb/rf230bb.h"
#include "net/mac/frame802154.h"
#include "net/mac/framer-802154.h"
#include "net/sicslowpan.h"

#if UIP_CONF_IPV6
#include "net/uip-ds6.h"
#endif /* UIP_CONF_IPV6 */

#else                 //radio driver using Atmel/Cisco 802.15.4'ish MAC
#include <stdbool.h>
#include "mac.h"
#include "sicslowmac.h"
#include "sicslowpan.h"
#include "ieee-15-4-manager.h"
#endif /*RF230BB*/

#include "contiki.h"
#include "contiki-net.h"
#include "contiki-lib.h"

#include "dev/rs232.h"
#include "dev/serial-line.h"

#include "net/rime.h"

/* Set ANNOUNCE to send boot messages to USB or RS232 serial port */
#define ANNOUNCE 1

/* Test rtimers, also useful for pings and time stamps in simulator */
#define TESTRTIMER 0
#if TESTRTIMER
#define PINGS 0
#define STAMPS 30
uint8_t rtimerflag=1;
uint16_t rtime;
struct rtimer rt;
void rtimercycle(void) {rtimerflag=1;}
#endif /* TESTRTIMER */

#ifndef BAUD_RATE
#define BAUD_RATE USART_BAUD_19200
#endif

/*-------------------------------------------------------------------------*/
/*----------------------Configuration of the .elf file---------------------*/
typedef struct {unsigned char B2;unsigned char B1;unsigned char B0;} __signature_t;
#define SIGNATURE __signature_t __signature __attribute__((section (".signature")))
SIGNATURE = {
/* Older AVR-GCCs may not define the SIGNATURE_n bytes so use explicit values */
  .B2 = 0x82,//SIGNATURE_2, //AT90USB128x
  .B1 = 0x97,//SIGNATURE_1, //128KB flash
  .B0 = 0x1E,//SIGNATURE_0, //Atmel
};
FUSES ={.low = 0xde, .high = 0x99, .extended = 0xff,};

/* Put default MAC address in EEPROM */
#if !JACKDAW_CONF_USE_SETTINGS
#if RAVEN_ADDRESS_LAST_BYTE
uint8_t mac_address[8] EEMEM = {0x02, 0x11, 0x11, 0xff, 0xfe, 0x11, 0x11, RAVEN_ADDRESS_LAST_BYTE};
#else
uint8_t mac_address[8] EEMEM = {0x02, 0x11, 0x11, 0xff, 0xfe, 0x11, 0x11, 0x11};
#endif

#ifdef RF_CHANNEL
uint8_t rf_channel[2] EEMEM = {RF_CHANNEL, ~RF_CHANNEL};
#else /* RF_CHANNEL */
#error "No RF channel defined!"
#endif /* RF_CHANNEL */
#endif /* JACKDAW_CONF_USE_SETTINGS */

static uint8_t get_channel_from_eeprom() {
#if JACKDAW_CONF_USE_SETTINGS
	uint8_t chan = settings_get_uint8(SETTINGS_KEY_CHANNEL, 0);
	if(!chan)
		chan = RF_CHANNEL;
	return chan;
#else
	uint8_t eeprom_channel;
	uint8_t eeprom_check;

	eeprom_channel = eeprom_read_byte((uint8_t *)9);
	eeprom_check = eeprom_read_byte((uint8_t *)10);

	if(eeprom_channel==~eeprom_check)
		return eeprom_channel;

#ifdef RF_CHANNEL
	return RF_CHANNEL;
#else /* RF_CHANNEL */
#error "No RF channel defined!"
#endif /* RF_CHANNEL */

#endif
	
}

static bool
get_mac_from_eeprom(uint8_t macptr[8]) {
#if JACKDAW_CONF_USE_SETTINGS
	size_t size = 8;

	if(settings_get(SETTINGS_KEY_EUI64, 0, (unsigned char*)macptr, &size)==SETTINGS_STATUS_OK)
		return true;
		
	// Fallback to reading the traditional mac address
	eeprom_read_block ((void *)macptr,  0, 8);
#else
	eeprom_read_block ((void *)macptr,  &mac_address, 8);
#endif
	return macptr[0]!=0xFF;
}

static uint16_t
get_panid_from_eeprom(void) {
#if JACKDAW_CONF_USE_SETTINGS
	uint16_t x = settings_get_uint16(SETTINGS_KEY_PAN_ID, 0);
	if(!x)
		x = IEEE802154_PANID;
	return x;
#else
	// TODO: Writeme!
	return IEEE802154_PANID;
#endif
}

static uint16_t
get_panaddr_from_eeprom(void) {
#if JACKDAW_CONF_USE_SETTINGS
	return settings_get_uint16(SETTINGS_KEY_PAN_ADDR, 0);
#else
	// TODO: Writeme!
	return 0;
#endif
}


/*-------------------------------------------------------------------------*/
/*-----------------------------Low level initialization--------------------*/
static void initialize(void) {
  watchdog_init();
  watchdog_start();
  
  /* Clock */
  clock_init();

  /* Use rs232 port for serial out (tx, rx, gnd are the three pads behind jackdaw leds */
  rs232_init(RS232_PORT_0, BAUD_RATE,USART_PARITY_NONE | USART_STOP_BITS_1 | USART_DATA_BITS_8);
  /* Redirect stdout to second port */
  rs232_redirect_stdout(RS232_PORT_0);

  PRINTF("\n\n\n********BOOTING CONTIKI*********\n");

  /* rtimer init needed for low power protocols */
  rtimer_init();

  Leds_init();
  Leds_off();

  /* Process subsystem. */
  process_init();

  /* etimer process must be started before ctimer init */
  process_start(&etimer_process, NULL);
  
#if RF230BB

  ctimer_init();
  /* Start radio and radio receive process */
  NETSTACK_RADIO.init();

  /* Set addresses BEFORE starting tcpip process */

  rimeaddr_t addr;
  memset(&addr, 0, sizeof(rimeaddr_t));
  get_mac_from_eeprom(addr.u8);

#if UIP_CONF_IPV6
  memcpy(&uip_lladdr.addr, &addr.u8, 8);
#endif
  rf230_set_pan_addr(
	get_panid_from_eeprom(),
	get_panaddr_from_eeprom(),
	(uint8_t *)&addr.u8
  );
  rf230_set_channel(get_channel_from_eeprom());

  rimeaddr_set_node_addr(&addr);

  PRINTF("MAC address %x:%x:%x:%x:%x:%x:%x:%x\n",addr.u8[0],addr.u8[1],addr.u8[2],addr.u8[3],addr.u8[4],addr.u8[5],addr.u8[6],addr.u8[7]);

  /* Initialize stack protocols */
  queuebuf_init();
  NETSTACK_RDC.init();
  NETSTACK_MAC.init();
  NETSTACK_NETWORK.init();

#if ANNOUNCE_BOOT
  PRINTF("%s %s, channel %u",NETSTACK_MAC.name, NETSTACK_RDC.name,rf230_get_channel());
  if (NETSTACK_RDC.channel_check_interval) {//function pointer is zero for sicslowmac
    unsigned short tmp;
    tmp=CLOCK_SECOND / (NETSTACK_RDC.channel_check_interval == 0 ? 1:\
                                   NETSTACK_RDC.channel_check_interval());
    if (tmp<65535) PRINTF(", check rate %u Hz",tmp);
  }
  PRINTF("\n");

#if UIP_CONF_IPV6_RPL
  PRINTF("RPL Enabled\n");
#endif
#if UIP_CONF_ROUTER
  PRINTF("Routing Enabled\n");
#endif

#endif /* ANNOUNCE_BOOT */

  process_start(&tcpip_process, NULL);

#else /* RF230BB */

/* Original RF230 combined mac/radio driver */
/* mac process must be started before tcpip process! */
  process_start(&mac_process, NULL);
  process_start(&tcpip_process, NULL);
#endif /*RF230BB*/

  /* Autostart other processes */
  autostart_start(autostart_processes);
  
  //printf_P(PSTR("OK\r\n"));
  Led0_on(); // blue
}

/*-------------------------------------------------------------------------*/
/*---------------------------------Main Routine----------------------------*/
int
main(void)
{
  /* GCC depends on register r1 set to 0 (?) */
  asm volatile ("clr r1");
  
  /* Initialize in a subroutine to maximize stack space */
  initialize();


#if DEBUG
{struct process *p;
 for(p = PROCESS_LIST();p != NULL; p = ((struct process *)p->next)) {
  printf_P(PSTR("Process=%p Thread=%p  Name=\"%s\" \n"),p,p->thread,PROCESS_NAME_STRING(p));
 }
}
#endif
  while(1) {
    process_run();
    watchdog_periodic();

/* Print rssi of all received packets, useful for range testing */
#ifdef RF230_MIN_RX_POWER
    uint8_t lastprint;
    if (rf230_last_rssi != lastprint) {        //can be set in halbb.c interrupt routine
        printf_P(PSTR("%u "),rf230_last_rssi);
        lastprint=rf230_last_rssi;
    }
#endif

#if TESTRTIMER
    if (rtimerflag) {  //8 seconds is maximum interval, my jackdaw 4% slow
      rtimer_set(&rt, RTIMER_NOW()+ RTIMER_ARCH_SECOND*1UL, 1,(void *) rtimercycle, NULL);
      rtimerflag=0;
#if STAMPS
      if ((rtime%STAMPS)==0) {
        printf("%us ",rtime);
      }
#endif
      rtime+=1;
#if PINGS
      if ((rtime%PINGS)==0) {
        PRINTF("**Ping\n");
        pingsomebody();
      }
#endif
    }
#endif /* TESTRTIMER */

/* Use with rf230bb.c DEBUGFLOW to show the sequence of driver calls from the uip stack */
#if RF230BB&&0
extern uint8_t debugflowsize,debugflow[];
  if (debugflowsize) {
    debugflow[debugflowsize]=0;
    printf("%s",debugflow);
    debugflowsize=0;
   }
#endif

/* Use for low level interrupt debugging */
#if RF230BB&&0
extern uint8_t rf230interruptflag;   //in halbb.c
extern uint8_t rf230processflag;     //in rf230bb.c
  if (rf230processflag) {
    printf("**RF230 process flag %u\n\r",rf230processflag);
    rf230processflag=0;
  }
  if (rf230interruptflag) {
//  if (rf230interruptflag!=11) {
      printf("**RF230 Interrupt %u\n\r",rf230interruptflag);
 // }
    rf230interruptflag=0;
  }
#endif
  }

  return 0;
}

