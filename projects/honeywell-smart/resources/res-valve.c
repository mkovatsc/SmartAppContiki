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
 *      Honeywell resource
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include "honeywell.h"

#define DEBUG   DEBUG_NONE
#include "net/uip-debug.h"

static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_put_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_resume_handler(void);

/* A simple actuator example. Toggles the red led */
SEPARATE_RESOURCE(res_valve,
    "title=\"Valve Control\";rt=\"ucum:%%\"",
    res_get_handler,
    NULL,
    res_put_handler,
    NULL,
    res_resume_handler);

static uint8_t separate_active = 0;
extern application_separate_store_t separate_store_valve;

static void
res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* Clean-up timeout */
  if(separate_active && (separate_store_valve.last_call < clock_time()-RES_SEPARATE_CLEAN_UP_TIMEOUT*CLOCK_SECOND)) {
    separate_active = 0;
  }

  if(poll_data.last_valve_wanted_reading > clock_time()-5*CLOCK_SECOND) {
    snprintf_P((char*)buffer, preferred_size, PSTR("%u"), poll_data.valve_wanted);
    coap_set_header_content_format(response, TEXT_PLAIN);
    coap_set_payload(response, buffer, strlen((char*)buffer));
    printf_P(PSTR("GV\n"));
  } else {
    if(separate_active) {
      coap_separate_reject();
    } else {
      ++separate_active;
      coap_separate_accept(request, &separate_store_valve.request_metadata);
      separate_store_valve.last_call = clock_time();
      separate_store_valve.action = COAP_GET;
      printf_P(PSTR("GV\n"));
    }
  }
}

static void
res_put_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t * string = NULL;
  int success = 1;
  int len = coap_get_payload(request, &string);
  int new_valve;

  /* Clean-up timeout */
  if(separate_active && (separate_store_valve.last_call < clock_time()-RES_SEPARATE_CLEAN_UP_TIMEOUT*CLOCK_SECOND)) {
    separate_active = 0;
  }

  if(len > 3) {
    success = 0;
  } else {
    new_valve = atoi((char*)string);
    if (new_valve < 0 || new_valve > 100) {
      success = 0;
    }
  }

  if(success) {
    if(separate_active) {
      coap_separate_reject();
    } else {
      ++separate_active;
      coap_separate_accept(request, &separate_store_valve.request_metadata);
      separate_store_valve.last_call = clock_time();
      separate_store_valve.action = COAP_PUT;
      printf_P(PSTR("SV%02x\n"),new_valve);
    }
  } else {
    coap_set_status_code(response, BAD_REQUEST_4_00);
    strncpy_P((char*)buffer, PSTR("Format: 0-100 (e.g., 47 sets the valve to 47%"), preferred_size);
    coap_set_header_content_format(response, TEXT_PLAIN);
    coap_set_payload(response, buffer, strlen((char*)buffer));
    return;
  }
}

static void
res_resume_handler()
{
  printf("vres\n");
  if(separate_active) {
    char buffer[10];
    coap_transaction_t *transaction = NULL;
    if( (transaction = coap_new_transaction(separate_store_valve.request_metadata.mid, &separate_store_valve.request_metadata.addr, separate_store_valve.request_metadata.port)) ) {
      coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */

      if(separate_store_valve.error) {
        coap_separate_resume(response, &separate_store_valve.request_metadata, INTERNAL_SERVER_ERROR_5_00);
        separate_store_valve.error = 0;
      } else {
        coap_separate_resume(response, &separate_store_valve.request_metadata, CHANGED_2_04);
        if(separate_store_valve.action == COAP_GET) {
          snprintf_P(buffer, 10 , PSTR("%d.%02d"), poll_data.target_temperature/100, poll_data.target_temperature%100);
          coap_set_status_code(response, CONTENT_2_05);
          coap_set_header_content_format(response, TEXT_PLAIN);
          coap_set_payload(response, buffer, strlen(buffer));
        }
      }

      printf("vres-send\n");

      // FIXME necessary?
      //coap_set_header_block2(response, separate_get_target_store->request_metadata.block2_num, 0, separate_get_target_store->request_metadata.block2_size);

      transaction->packet_len = coap_serialize_message(response, transaction->packet);
      coap_send_transaction(transaction);
      --separate_active;
    } else {
      separate_active = 0;
      /*
       * TODO: ERROR HANDLING: Set timer for retry, send error message, ...
       */
    }
  }
}
