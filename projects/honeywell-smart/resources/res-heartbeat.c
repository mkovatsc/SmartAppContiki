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

#define DEBUG   DEBUG_NONE
#include "net/uip-debug.h"

static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_periodic_handler(void);

PERIODIC_RESOURCE(res_heartbeat,
    "title=\"Heartbeat\";obs;rt=\"debug\"",
    res_get_handler,
    NULL,
    NULL,
    NULL,
    5*CLOCK_SECOND,
    res_periodic_handler);

static int32_t event_counter = 0;

static int8_t rssi_value[5] = {0};
static int8_t rssi_count = 0;
static int8_t rssi_position = 0;
static int16_t rssi_avg;

static void
res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  snprintf_P((char*)buffer, preferred_size, PSTR("version:%s,uptime:%lu,rssi:%i"), VERSION, clock_seconds(), rssi_avg);
  coap_set_payload(response, buffer, strlen((char*)buffer));
}

static void
res_periodic_handler()
{
  ++event_counter;

  rssi_value[rssi_position] = radio_sensor.value(0);
  if (rssi_count<5)
  {
    ++rssi_count;
  }
  rssi_avg = (rssi_count>0) ? (rssi_value[0] + rssi_value[1] + rssi_value[2] + rssi_value[3] + rssi_value[4])/rssi_count : 0;
  rssi_position = (++rssi_position) % 5;

  REST.notify_subscribers(&res_heartbeat);
}
