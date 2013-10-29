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
static void res_resume_handler();

SEPARATE_RESOURCE(res_mode,
    "title=\"Mode Change\";rt=\"thermostat:state\"",
    res_get_handler,
    NULL,
    res_put_handler,
    NULL,
    res_resume_handler);

static uint8_t separate_active = 0;
extern application_separate_store_t separate_store_mode;

static void
res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  printf_P(PSTR("GM\n"));

  static char msg[20];

  if (poll_data.mode==manual_target) {
    strncpy_P((char*)msg, PSTR("manual target"), 19);
  }
  else if (poll_data.mode==manual_timer) {
    strncpy_P((char*)msg, PSTR("manual timer"), 19);
  }
  else if (poll_data.mode==radio_target) {
    strncpy_P((char*)msg, PSTR("radio target"), 19);
  }
  else if (poll_data.mode==radio_valve) {
    strncpy_P((char*)msg, PSTR("radio valve"), 19);
  }
  else {
    strncpy_P((char*)msg, PSTR("undefined"), 19);
  }

  coap_set_header_content_format(response, REST.type.TEXT_PLAIN);
  coap_set_payload(response,msg, strlen(msg));
}

static void
res_put_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t * string = NULL;
  uint8_t success = 1;
  char cmd[6];
  int len = coap_get_payload(request, &string);

  /* Clean-up timeout */
  if(separate_active && (separate_store_mode.last_call < clock_time()-RES_SEPARATE_CLEAN_UP_TIMEOUT*CLOCK_SECOND)) {
    separate_active = 0;
  }

  if(len == 0) {
    success = 0;
  } else {
    if(strncmp_P((char*)string,PSTR("manual target"),MAX(len,13))==0) {
      strncpy_P((char*)cmd, PSTR("SM00\n"),6);
    }
    else if(strncmp_P((char*)string,PSTR("manual timer"),MAX(len,12))==0) {
      strncpy_P((char*)cmd, PSTR("SM01\n"),6);
    }
    else if(strncmp_P((char*)string, PSTR("auto target"),MAX(len,11))==0) {
      strncpy_P((char*)cmd, PSTR("SM02\n"),6);
    }
    else if(strncmp_P((char*)string, PSTR("auto valve"),MAX(len,10))==0) {
      strncpy_P((char*)cmd, PSTR("SM03\n"),6);
    }
    else if(strncmp_P((char*)string, PSTR("auto timer"),MAX(len,10))==0) {
      strncpy_P((char*)cmd, PSTR("SM04\n"),6);
    } else {
      success = 0;
    }
  }

  if(success) {
    if(separate_active) {
      coap_separate_reject();
    } else {
      ++separate_active;
      coap_separate_accept(request, &separate_store_mode.request_metadata);
      separate_store_mode.last_call = clock_time();
      printf("%s",cmd);
    }
  } else {
    coap_set_status_code(response, BAD_REQUEST_4_00);
    strncpy_P((char*)buffer, PSTR("States: manual {target, timer}, auto {target, valve, timer}"), preferred_size);
    coap_set_header_content_format(response, TEXT_PLAIN);
    coap_set_payload(response, buffer, strlen((char*)buffer));
    return;
  }
}

static void
res_resume_handler()
{
  if(separate_active) {
    coap_transaction_t *transaction = NULL;
    if( (transaction = coap_new_transaction(separate_store_mode.request_metadata.mid, &separate_store_mode.request_metadata.addr, separate_store_mode.request_metadata.port)) ) {
      coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */

      if(separate_store_mode.error) {
        coap_separate_resume(response, &separate_store_mode.request_metadata, INTERNAL_SERVER_ERROR_5_00);
        separate_store_mode.error = 0;
      } else {
        coap_separate_resume(response, &separate_store_mode.request_metadata, CHANGED_2_04);
      }

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
