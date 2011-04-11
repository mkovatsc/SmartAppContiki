/*
 *  Open HR20
 *
 *  target:     ATmega169 @ 4 MHz in Honnywell Rondostat HR20E
 *
 *  ompiler:    WinAVR-20071221
 *              avr-libc 1.6.0
 *              GCC 4.2.2
 *
 *  copyright:  2008 Juergen Sachs (juergen-sachs-at-gmx-dot-de)
 *
 *  license:    This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU Library General Public
 *              License as published by the Free Software Foundation; either
 *              version 2 of the License, or (at your option) any later version.
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *              GNU General Public License for more details.
 *
 *              You should have received a copy of the GNU General Public License
 *              along with this program. If not, see http:*www.gnu.org/licenses
 */

/*!
 * \file       engine.h
 * \brief      
 * \author     Juergen Sachs (juergen-sachs-at-gmx-dot-de)
 * \date       13.08.2008
 * $Rev: 8 $
 */

#ifndef ENGINE_H
#define ENGINE_H

// AVR LibC includes 
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/version.h>

#include "com.h"
#include "main.h"
#include "adc.h"
#include "rtc.h"

/*
for test mainly
*/
struct serState 
{
	uint16_t volt;
	uint16_t tempMin;
	uint16_t tempCur;
	uint16_t tempMax;	
};

extern struct serState st;

void e_meassure(void);
void e_Init(void);
void e_resetTemp(void);

#endif /* ENGINE_H */
