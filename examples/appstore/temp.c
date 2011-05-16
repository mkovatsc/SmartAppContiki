/*
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
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "dev/battery-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/leds.h"
#include "node-id.h"

#include <string.h>

#define SEND_INTERVAL       1 * CLOCK_SECOND

/*---------------------------------------------------------------------------*/
PROCESS(temp_process, "temp");
AUTOSTART_PROCESSES(&temp_process);
static struct etimer et;
extern struct uip_udp_conn *collect_conn;

/*---------------------------------------------------------------------------*/
static void
timeout_handler(void)
{
  //  static int seq_id;
    char tmp[4];
    int sample;

    SENSORS_ACTIVATE(sht11_sensor);
    sample = sht11_sensor.value(SHT11_SENSOR_TEMP);
    tmp[0] = 't';
    tmp[1] = node_id;
    tmp[2] = sample >> 8;
    tmp[3] = sample;
    uip_udp_packet_send(collect_conn, tmp, sizeof(tmp));
  //  seq_id++;
    SENSORS_DEACTIVATE(sht11_sensor);
}

/*---------------------------------------------------------------------------*/
static void
exit_handler(void)
{
//  etimer_stop(&et);
  printf("exit\n");
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(temp_process, ev, data)
{
  PROCESS_EXITHANDLER(exit_handler())

  PROCESS_BEGIN();

  etimer_set(&et, SEND_INTERVAL);
  printf("-- Hello from Temp\n");
  leds_on(LEDS_RED);
  while(1) {
    PROCESS_YIELD();
    if(etimer_expired(&et)) {
//      leds_on(LEDS_RED);
      timeout_handler();
//      leds_off(LEDS_RED);
      etimer_restart(&et);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
