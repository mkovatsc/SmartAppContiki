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
 *
 */

#ifndef __PROJECT_RPL_WEB_CONF_H__
#define __PROJECT_RPL_WEB_CONF_H__

/* avr-ravenmote: 2 */
#undef COAP_MAX_OPEN_TRANSACTIONS
#define COAP_MAX_OPEN_TRANSACTIONS 10

#undef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS         COAP_MAX_OPEN_TRANSACTIONS-1

#undef REST_MAX_CHUNK_SIZE
#define REST_MAX_CHUNK_SIZE     128

#undef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE    240
/*
 * avr-ravenmote
#define UIP_CONF_BUFFER_SIZE    224
 */

/* RADIOSTATS is used in rf230bb, clock.c and the webserver cgi to report radio usage */
#undef PERIODICPRINTS
#define PERIODICPRINTS 0
#undef RADIOSTATS
#define RADIOSTATS 0


#ifndef UIP_CONF_RECEIVE_WINDOW
#define UIP_CONF_RECEIVE_WINDOW  60
#endif

#ifndef WEBSERVER_CONF_CFS_CONNS
#define WEBSERVER_CONF_CFS_CONNS 2
#endif


#undef RF_CHANNEL
#define RF_CHANNEL 24

#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID 0xBEEF

#undef EUI64_ADDRESS
#define EUI64_ADDRESS {0x00,0x21,0x2e,0xff,0xff,0x00,0x03,0xe2}
//#define EUI64_ADDRESS {0x00,0x50,0xc2,0xff,0xff,0x18,0x8d,0x2d}

#define COAP_RD_SET_IPV6(ipaddr)	uip_ip6addr(ipaddr, 0x2001, 0x620, 0x8, 0x35db, 0, 0, 0, 0x1f)
#define COAP_RD_PORT							UIP_HTONS(5683)


#endif /* __PROJECT_RPL_WEB_CONF_H__ */
