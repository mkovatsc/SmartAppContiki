/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
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
 */

/**
 * \file
 *      Example resource
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include "honeywell.h"
#include "params.h"
#include "dev/watchdog.h"

#define DEBUG   DEBUG_NONE
#include "net/uip-debug.h"

extern uint8_t eemem_mac_address[] EEMEM;

static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_put_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_mac,
    "title=\"MAC address\";rt=\"debug\"",
    res_get_handler,
    NULL,
    res_put_handler,
    NULL);

static void
res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  snprintf((char*)buffer, preferred_size, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
      uip_lladdr.addr[0],
      uip_lladdr.addr[1],
      uip_lladdr.addr[2],
      uip_lladdr.addr[3],
      uip_lladdr.addr[4],
      uip_lladdr.addr[5],
      uip_lladdr.addr[6],
      uip_lladdr.addr[7] );
  coap_set_payload(response, buffer, strlen((char*)buffer));
}

static void
res_put_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int success = 0;
  uint8_t addr[8];
  const char *string = NULL;
  int len = coap_get_payload(request, (const uint8_t **) &string);

  if(len > 0 && string[2]=='-' && string[5]=='-' && string[8]=='-' && string[11]=='-' && string[14]=='-' && string[17]=='-' && string[20]=='-') {
    addr[0] = strtol(string, NULL, 16);
    addr[1] = strtol(&string[3], NULL, 16);
    addr[2] = strtol(&string[6], NULL, 16);
    addr[3] = strtol(&string[9], NULL, 16);
    addr[4] = strtol(&string[12], NULL, 16);
    addr[5] = strtol(&string[15], NULL, 16);
    addr[6] = strtol(&string[18], NULL, 16);
    addr[7] = strtol(&string[21], NULL, 16);

    success = 1;
  }

  if(success) {
    cli();
    eeprom_write_block(addr,  &eemem_mac_address, sizeof(addr));
    sei();

    watchdog_reboot();
  } else {
    coap_set_status_code(response, BAD_REQUEST_4_00);
  }
}
