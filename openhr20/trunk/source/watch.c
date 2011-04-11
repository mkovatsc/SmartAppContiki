/*
 *  Open HR20
 *
 *  target:     ATmega169 @ 4 MHz in Honnywell Rondostat HR20E
 *
 *  compiler:   WinAVR-20071221
 *              avr-libc 1.6.0
 *              GCC 4.2.2
 *
 *  copyright:  2008 Jiri Dobry (jdobry-at-centrum-dot-cz)
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
 * \file       watch.h
 * \brief      watch variable for debug
 * \author     Jiri Dobry <jdobry-at-centrum-dot-cz>
 * \date       $Date: 2009-03-24 12:52:18 +0100 (Tue, 24 Mar 2009) $
 * $Rev: 224 $
 */

#include <stdint.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

#include "main.h"
#include "adc.h"
#include "controller.h"
#include "motor.h"
#include "watch.h"

#define B8 0x0000
#define B16 0x8000
#define B_MASK 0x8000

int16_t MOTOR_PosMax;


static uint16_t watch_map[WATCH_N] PROGMEM = {
    /* 00 */ ((uint16_t) &temp_average) + B16, // temperature 
    /* 01 */ ((uint16_t) &bat_average) + B16,  // battery 
    /* 02 */ ((uint16_t) &sumError) + B16,
	/* 03 */ ((uint16_t) &CTL_temp_wanted) + B8,
	/* 04 */ ((uint16_t) &CTL_temp_wanted_last) +B8,
	/* 05 */ ((uint16_t) &CTL_temp_auto) +B8,
	/* 06 */ ((uint16_t) &CTL_mode_auto) +B8,
	/* 07 */ ((uint16_t) &motor_diag) + B16,
	/* 08 */ ((uint16_t) &MOTOR_PosMax) + B16,
	/* 09 */ ((uint16_t) &MOTOR_PosAct) + B16,
};

uint16_t watch(uint8_t addr) {
	uint16_t p;
	if (addr >= sizeof(watch_map)/2) return 0;

	p=pgm_read_word(&watch_map[addr]);
	if ((p&B_MASK)==B16) { // 16 bit value
		return *((uint16_t *)(p & ~B_MASK));
	} else { // 8 bit value
		return (uint16_t)(*((uint8_t *)(p & ~B_MASK)));
	}
}

