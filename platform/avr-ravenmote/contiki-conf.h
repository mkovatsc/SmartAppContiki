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
 *         Configuration for RZRAVEN USB stick "jackdaw"
 *
 * \author
 *         Simon Barner <barner@in.tum.de>
 *         David Kopf <dak664@embarqmail.com>
 */

#ifndef __CONTIKI_CONF_H__
#define __CONTIKI_CONF_H__

/* MCU and clock rate */
#define PLATFORM       PLATFORM_AVR
#define RAVEN_REVISION RAVENUSB_C
#ifndef F_CPU
#define F_CPU          8000000UL
#endif
#include <stdint.h>

typedef int32_t s32_t;
typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned long u32_t;
typedef unsigned short clock_time_t;
typedef unsigned short uip_stats_t;
typedef unsigned long off_t;

void clock_delay(unsigned int us2);
void clock_wait(int ms10);
void clock_set_seconds(unsigned long s);
unsigned long clock_seconds(void);

/* Maximum timer interval for 16 bit clock_time_t */
#define INFINITE_TIME 0xffff

/* Clock ticks per second */
#define CLOCK_CONF_SECOND 125

/* Maximum tick interval is 0xffff/125 = 524 seconds */
#define RIME_CONF_BROADCAST_ANNOUNCEMENT_MAX_TIME CLOCK_CONF_SECOND * 524UL /* Default uses 600UL */
#define COLLECT_CONF_BROADCAST_ANNOUNCEMENT_MAX_TIME CLOCK_CONF_SECOND * 524UL /* Default uses 600UL */

/* Get Mac address, RF channel, PANID from EEPROM settings manager, or use hard-coded values? */
/* Generate random MAC address on first startup? */
/* Random number from radio clock skew or ADC noise? */
#define JACKDAW_CONF_USE_SETTINGS		0
#define JACKDAW_CONF_RANDOM_MAC         0
#define RNG_CONF_USE_RADIO_CLOCK	    1
//#define RNG_CONF_USE_ADC	1

/* COM port to be used for SLIP connection. Not tested on Jackdaw. */
#define SLIP_PORT RS232_PORT_0

/* ************************************************************************** */
//#pragma mark Serial Port Settings
/* ************************************************************************** */
/* Set USB_CONF_MACINTOSH to prefer CDC-ECM+DEBUG enumeration for Mac/Linux 
 * Leave undefined to prefer RNDIS+DEBUG enumeration for Windows/Linux
 * TODO:Serial port would enumerate in all cases and prevent falling through to
 * the supported network interface if USB_CONF_MACINTOSH is used with Windows
 * or vice versa. The Mac configuration is set up to still enumerate as RNDIS-ONLY
 * on Windows (without the serial port). 
 * At present the Windows configuration will not enumerate on the Mac at all,
 * since it wants a custom descriptor for USB composite devices.
 */ 
#define USB_CONF_MACINTOSH 0

/* Set USB_CONF_SERIAL to enable the USB serial port that allows control of the
 * run-time configuration (COMx on Windows, ttyACMx on Linux, tty.usbmodemx on Mac)
 * Debug printfs will go to this port unless USB_CONF_RS232 is set.
 */
#define USB_CONF_SERIAL          0
 
/* RS232 debugs have less effect on network timing and are less likely
 * to be dropped due to buffer overflow. Only tx is implemented at present.
 * The tx pad is the middle one behind the jackdaw leds.
 * RS232 output will work with or without enabling the USB serial port
 */
#define USB_CONF_RS232           0

/* Disable mass storage enumeration for more program space */
//#define USB_CONF_STORAGE         1   /* TODO: Mass storage is currently broken */

/* ************************************************************************** */
//#pragma mark UIP Settings
/* ************************************************************************** */
/* Network setup. The new NETSTACK interface requires RF230BB (as does ip4) */
/* These mostly have no effect when the Jackdaw is a repeater (CONTIKI_NO_NET=1 using fakeuip.c) */

#if RF230BB
#else
#define PACKETBUF_CONF_HDR_SIZE    0         //RF230 combined driver/mac handles headers internally
#endif /*RF230BB */

#if UIP_CONF_IPV6

#ifndef UIP_CONF_IPV6_RPL
#define UIP_CONF_IPV6_RPL        1
#endif /*UIP_CONF_IPV6_RPL */

#define RIMEADDR_CONF_SIZE       8

#define UIP_CONF_ICMP6           1
#define UIP_CONF_UDP             1
#define UIP_CONF_TCP             0

#define NETSTACK_CONF_NETWORK       sicslowpan_driver
#define SICSLOWPAN_CONF_COMPRESSION SICSLOWPAN_COMPRESSION_HC06
#else
/* ip4 should build but is thoroughly untested */
#define RIMEADDR_CONF_SIZE       2
#define NETSTACK_CONF_NETWORK    rime_driver
#endif /* UIP_CONF_IPV6 */

/* See uip-ds6.h */
#define UIP_CONF_DS6_NBR_NBU      10
#define UIP_CONF_DS6_DEFRT_NBU    2
#define UIP_CONF_DS6_PREFIX_NBU   2
#define UIP_CONF_DS6_ROUTE_NBU    10
#define UIP_CONF_DS6_ADDR_NBU     3
#define UIP_CONF_DS6_MADDR_NBU    0
#define UIP_CONF_DS6_AADDR_NBU    0

#define UIP_CONF_LL_802154       1
#define UIP_CONF_LLH_LEN         0

/* 10 bytes per stateful address context - see sicslowpan.c */
/* Default is 1 context with prefix aaaa::/64 */
/* These must agree with all the other nodes or there will be a failure to communicate! */
#//define SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS 1
#define SICSLOWPAN_CONF_ADDR_CONTEXT_0 {addr_contexts[0].prefix[0]=0xaa;addr_contexts[0].prefix[1]=0xaa;}
#define SICSLOWPAN_CONF_ADDR_CONTEXT_1 {addr_contexts[1].prefix[0]=0xbb;addr_contexts[1].prefix[1]=0xbb;}
#define SICSLOWPAN_CONF_ADDR_CONTEXT_2 {addr_contexts[2].prefix[0]=0x20;addr_contexts[2].prefix[1]=0x01;addr_contexts[2].prefix[2]=0x49;addr_contexts[2].prefix[3]=0x78,addr_contexts[2].prefix[4]=0x1d;addr_contexts[2].prefix[5]=0xb1;}

/* 211 bytes per queue buffer */
#ifndef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM        8
#endif /*QUEUEBUF_CONF_NUM*/

/* 54 bytes per queue ref buffer */
#define QUEUEBUF_CONF_REF_NUM    2

#define UIP_CONF_MAX_CONNECTIONS 1
#define UIP_CONF_MAX_LISTENPORTS 1

#define UIP_CONF_IP_FORWARD      0
#define UIP_CONF_FWCACHE_SIZE    0

#define UIP_CONF_IPV6_CHECKS     1
#define UIP_CONF_IPV6_QUEUE_PKT  1
#define UIP_CONF_IPV6_REASSEMBLY 0

#define UIP_CONF_UDP_CHECKSUMS   1
#define UIP_CONF_TCP_SPLIT       0
#define UIP_CONF_STATISTICS      1


#ifndef RF_CHANNEL
#define RF_CHANNEL              26
#endif /* RF_CHANNEL */

  /* Network setup */
#if 1              /* No radio cycling */

#define NETSTACK_CONF_MAC         csma_driver
#define NETSTACK_CONF_RDC         nullrdc_driver
#define NETSTACK_CONF_FRAMER      framer_802154
#define NETSTACK_CONF_RADIO       rf230_driver

/* AUTOACK receive mode gives better rssi measurements, even if ACK is never requested */
#define RF230_CONF_AUTOACK        1
/* Request 802.15.4 ACK on all packets sent by sicslowpan.c (else autoretry) */
/* Broadcasts will be duplicated by the retry count! */
#define SICSLOWPAN_CONF_ACK_ALL   0
/* Number of auto retry attempts 0-15 (0 implies don't use extended TX_ARET_ON mode with CCA) */
#define RF230_CONF_AUTORETRIES    2
/* CCA theshold energy -91 to -61 dBm (default -77). Set this smaller than the expected minimum rssi to avoid packet collisions */
/* The Jackdaw menu 'm' command is helpful for determining the smallest ever received rssi */
#define RF230_CONF_CCA_THRES    -85
/* Number of CSMA attempts 0-7. 802.15.4 2003 standard max is 5. */
#define RF230_CONF_CSMARETRIES    5
/* Allow sneeze command from jackdaw menu. Useful for testing CCA on other radios */
/* During sneezing, any access to an RF230 register will hang the MCU and cause a watchdog reset */
/* The host interface, jackdaw menu and rf230_send routines are temporarily disabled to prevent this */
/* But some calls from an internal uip stack might get through, e.g. from CCA or low power protocols, */
/* as temporarily disabling all the possible accesses would add considerable complication to the radio driver! */
#define RF230_CONF_SNEEZER        0
/* Allow 6loWPAN fragmentation (more efficient for large payloads over a reliable channel) */

#define SICSLOWPAN_CONF_FRAG      1
#define SICSLOWPAN_CONF_MAXAGE    3


#elif 0  /* Contiki-mac radio cycling */
#define NETSTACK_CONF_MAC         nullmac_driver
#define NETSTACK_CONF_RDC         contikimac_driver
#define NETSTACK_CONF_FRAMER      framer_802154
#define NETSTACK_CONF_RADIO       rf230_driver
#define RF230_CONF_AUTORETRIES    1
#define RF230_CONF_AUTOACK        1
#define RF230_CONF_CSMARETRIES    0
#define SICSLOWPAN_CONF_FRAG      1
#define SICSLOWPAN_CONF_MAXAGE    3
/* Jackdaw has USB power, can be always listening */
#define CONTIKIMAC_CONF_RADIO_ALWAYS_ON  1
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 8

/* Contiki-mac is a memory hog */
#define PROCESS_CONF_NO_PROCESS_NAMES 1
#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM           2
#undef QUEUEBUF_CONF_REF_NUM
#define QUEUEBUF_CONF_REF_NUM       1
#undef UIP_CONF_TCP_SPLIT
#define UIP_CONF_TCP_SPLIT          0
#undef UIP_CONF_STATISTICS
#define UIP_CONF_STATISTICS         0
#undef UIP_CONF_IPV6_QUEUE_PKT
#define UIP_CONF_IPV6_QUEUE_PKT     0
#define UIP_CONF_PINGADDRCONF       0
#define UIP_CONF_LOGGING            0
#undef UIP_CONF_MAX_CONNECTIONS
#define UIP_CONF_MAX_CONNECTIONS    2
#undef UIP_CONF_MAX_LISTENPORTS
#define UIP_CONF_MAX_LISTENPORTS    2
#define UIP_CONF_UDP_CONNS          6

#elif 1             /* cx-mac radio cycling */
#define NETSTACK_CONF_MAC         nullmac_driver
//#define NETSTACK_CONF_MAC         csma_driver
#define NETSTACK_CONF_RDC         cxmac_driver
#define NETSTACK_CONF_FRAMER      framer_802154
#define NETSTACK_CONF_RADIO       rf230_driver
#define RF230_CONF_AUTOACK        1
#define RF230_CONF_AUTORETRIES    1
#define SICSLOWPAN_CONF_FRAG      1
#define SICSLOWPAN_CONF_MAXAGE    3
#define CXMAC_CONF_ANNOUNCEMENTS    0
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 8
#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM        8
#undef UIP_CONF_DS6_NBR_NBU
#define UIP_CONF_DS6_NBR_NBU       5
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_ROUTE_NBU     5

#else
#error Network configuration not specified!
#endif   /* Network setup */


/* ************************************************************************** */
//#pragma mark RPL Settings
/* ************************************************************************** */

#if UIP_CONF_IPV6_RPL

/* Not completely working yet. Works on Ubuntu after $ifconfig usb0 -arp to drop the neighbor solitications */
/* Dropping the NS on other OSs is more complicated, see http://www.sics.se/~adam/wiki/index.php/Jackdaw_RNDIS_RPL_border_router */

/* RPL requires the uip stack. Change #CONTIKI_NO_NET=1 to UIP_CONF_IPV6=1 in the examples makefile,
   or include the needed source files in /plaftorm/avr-ravenusb/Makefile.avr-ravenusb */
/* For the present the buffer_length calcs in rpl-icmp6.c will need adjustment by the length difference
   between 6lowpan (0) and ethernet (14) link-layer headers:
 // buffer_length = uip_len - uip_l2_l3_icmp_hdr_len;
    buffer_length = uip_len - uip_l2_l3_icmp_hdr_len + UIP_LLH_LEN; //Add jackdaw ethernet header
 */
 
/* Define MAX_*X_POWER to reduce tx power and ignore weak rx packets for testing a miniature multihop network.
 * Leave undefined for full power and sensitivity.
 * tx=0 (3dbm, default) to 15 (-17.2dbm)
 * RF230_CONF_AUTOACK sets the extended mode using the energy-detect register with rx=0 (-91dBm) to 84 (-7dBm)
 *   else the rssi register is used having range 0 (91dBm) to 28 (-10dBm)
 *   For simplicity RF230_MIN_RX_POWER is based on the energy-detect value and divided by 3 when autoack is not set.
 * On the RF230 a reduced rx power threshold will not prevent autoack if enabled and requested.
 * These numbers applied to both Raven and Jackdaw give a maximum communication distance of about 15 cm
 * and a 10 meter range to a full-sensitivity RF230 sniffer.
#define RF230_MAX_TX_POWER 15
#define RF230_MIN_RX_POWER 30
 */

#define UIP_CONF_ROUTER             1
#define UIP_CONF_ND6_SEND_RA        0
#define UIP_CONF_ND6_REACHABLE_TIME 600000
#define UIP_CONF_ND6_RETRANS_TIMER  10000
#define RPL_BORDER_ROUTER           0
#define RPL_CONF_STATS              0
#define UIP_CONF_BUFFER_SIZE        240
//#define UIP_CONF_DS6_NBR_NBU       12
//#define UIP_CONF_DS6_ROUTE_NBU     12

/* Save all the RAM we can */
#define PROCESS_CONF_NO_PROCESS_NAMES 1
#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM           2
#undef QUEUEBUF_CONF_REF_NUM
#define QUEUEBUF_CONF_REF_NUM       1
#undef UIP_CONF_TCP_SPLIT
#define UIP_CONF_TCP_SPLIT          0
#undef UIP_CONF_STATISTICS
#define UIP_CONF_STATISTICS         0
#undef UIP_CONF_IPV6_QUEUE_PKT
#define UIP_CONF_IPV6_QUEUE_PKT     0
#define UIP_CONF_PINGADDRCONF       0
#define UIP_CONF_LOGGING            0
#undef UIP_CONF_MAX_CONNECTIONS
#define UIP_CONF_MAX_CONNECTIONS    2
#undef UIP_CONF_MAX_LISTENPORTS
#define UIP_CONF_MAX_LISTENPORTS    2
#define UIP_CONF_UDP_CONNS          6

/* Optional, TCP needed to serve the RPL neighbor web page currently hard coded at bbbb::11 */
/* The RPL neighbors can also be viewed using the jack menu */
/* A small MSS is adequate for the internal jackdaw webserver and RAM is very limited*/

#ifndef RPL_HTTPD_SERVER
#define RPL_HTTPD_SERVER            0
#endif /* RPL_HTTPD_SERVER */

#if RPL_HTTPD_SERVER
#undef UIP_CONF_TCP            
#define UIP_CONF_TCP                1
#define UIP_CONF_TCP_MSS           48
#define UIP_CONF_RECEIVE_WINDOW    48
#undef UIP_CONF_DS6_NBR_NBU
#define UIP_CONF_DS6_NBR_NBU        5
#undef UIP_CONF_DS6_ROUTE_NBU
#define UIP_CONF_DS6_ROUTE_NBU      5
#undef UIP_CONF_MAX_CONNECTIONS
#define UIP_CONF_MAX_CONNECTIONS    2
#endif

#define UIP_CONF_ICMP_DEST_UNREACH 1
#define UIP_CONF_DHCP_LIGHT
#undef UIP_CONF_FWCACHE_SIZE
#define UIP_CONF_FWCACHE_SIZE    30
#define UIP_CONF_BROADCAST       1
//#define UIP_ARCH_IPCHKSUM        1

/* Experimental option to pick up a prefix from host interface router advertisements */
/* Requires changes in uip6 and uip-nd6.c to pass link-local RA broadcasts */
/* If this is zero the prefix will be manually set in contiki-raven-main.c */
#define UIP_CONF_ROUTER_RECEIVE_RA  0

#endif /* UIP_CONF_IPV6_RPL */

/* ************************************************************************** */
//#pragma mark Other Settings
/* ************************************************************************** */

/* Use Atmel 'Route Under MAC', currently just in RF230 sniffer mode! */
/* Route-Under-MAC uses 16-bit short addresses */
//#define UIP_CONF_USE_RUM  1
#if UIP_CONF_USE_RUM
#undef  UIP_CONF_LL_802154
#define UIP_DATA_RUM_OFFSET      5
#endif /* UIP_CONF_USE_RUM */

#define CCIF
#define CLIF

/* include the project config */
/* PROJECT_CONF_H might be defined in the project Makefile */
#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif /* PROJECT_CONF_H */

#endif /* __CONTIKI_CONF_H__ */
