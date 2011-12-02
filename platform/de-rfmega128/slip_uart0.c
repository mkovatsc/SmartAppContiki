/*
 * Copyright (c) 2010, University of Colombo School of Computing
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

#include "contiki.h"
#include "lib/sensors.h"
//#include "dev/sky-sensors.h"
//#include "dev/light-sensor.h"
//#include "dev/potentiometer-sensor.h"

/* Photodiode 1 (P64) on INCH_4 */
/* Photodiode 2 (P65) on INCH_5 */
//Enric#define INPUT_CHANNEL      ((1 << INCH_4) | (1 << INCH_5))
#ifndef INPUT_CHANNEL
#define INPUT_CHANNEL      (1 << INCH_4)
#endif

#ifndef INPUT_REFERENCE
#define INPUT_REFERENCE     SREF_0
#endif

#ifdef POTENTIOMETER_MEM
#define PHOTOSYNTHETIC_MEM  POTENTIOMETER_MEM
#define TOTAL_SOLAR_MEM     POTENTIOMETER_MEM
#else
#define PHOTOSYNTHETIC_MEM  ADC12MEM4
#define TOTAL_SOLAR_MEM     ADC12MEM4
#endif

#include <stdio.h>
#include "contiki.h"
#include "dev/rs232.h"
#include "slip.h"

/*---------------------------------------------------------------------------*/
static int
slip_putchar(char c, FILE *stream)
{
#define SLIP_END 0300
  static char debug_frame = 0;

  if (!debug_frame) {        /* Start of debug output */
    slip_arch_writeb(SLIP_END);
    slip_arch_writeb('\r'); /* Type debug line == '\r' */
    debug_frame = 1;
  }

  slip_arch_writeb((unsigned char)c);
          
  /*
   * Line buffered output, a newline marks the end of debug output and
   * implicitly flushes debug output.         
   */
  if (c == '\n') {
    slip_arch_writeb(SLIP_END);
    debug_frame = 0;
  }

  return c;
}
/*---------------------------------------------------------------------------*/
static FILE slip_stdout = FDEV_SETUP_STREAM(slip_putchar, NULL,
                                            _FDEV_SETUP_WRITE);
/*---------------------------------------------------------------------------*/
void
slip_arch_init(unsigned long ubr)
{
  rs232_set_input(RS232_PORT_0, slip_input_byte);
  stdout = &slip_stdout;
}
/*---------------------------------------------------------------------------*/
/*
 XXX:
      Currently, the following function is in cpu/avr/dev/rs232.c file. this
      should be moved to here from there hence this is a platform specific slip 
      related function. 
void
slip_arch_writeb(unsigned char c)
{
  rs232_send(RS232_PORT_0, c);
}
*/
