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
 * \file       controller.h
 * \brief      Controller for temperature
 * \author     Jiri Dobry <jdobry-at-centrum-dot-cz>
 * \date       $Date: 2011-03-21 22:14:11 +0100 (Mon, 21 Mar 2011) $
 * $Rev: 350 $
 */

#pragma once

extern uint8_t CTL_temp_wanted; //!< wanted temperature
extern uint8_t CTL_valve_wanted; //!< wanted valve (just valid in the valve mode)
extern uint8_t CTL_temp_wanted_last;   // desired temperatur value used for last PID control
extern uint8_t CTL_temp_auto;

enum mode {manual_target=0, manual_timers=1, auto_target=2, auto_valve=3, auto_timers=4};

extern enum mode CTL_mode_auto;
extern int8_t PID_force_update;      // signed value, val<0 means disable force updates
extern uint8_t CTL_error;
extern uint8_t CTL_mode_window;

extern uint8_t CTL_mode_changed;
extern uint8_t CTL_mode_changed_timer;


#define mode_window() (CTL_mode_window!=0)

#define VALVE_HISTORY_LEN 1
extern uint8_t valveHistory[VALVE_HISTORY_LEN];
#define valve_wanted (valveHistory[0])

#define CTL_update_temp_auto() (CTL_temp_auto=0)
#define CTL_test_auto() (CTL_mode_auto && (CTL_temp_auto==CTL_temp_wanted))
#define CTL_set_temp(t) (PID_force_update = 10, CTL_temp_wanted=t)

void CTL_update(bool minute_ch);
void CTL_temp_change_inc (int8_t ch);
void CTL_valve_change_inc (int8_t ch);


#define CTL_CHANGE_AUTO        		-1
#define CTL_CHANGE_MINOR_MODE  		-2
#define CTL_CHANGE_MODE_REWOKE		-3
#define CTL_CLOSE_WINDOW_FORCE 		-4

void CTL_change_mode(int8_t dif);

#define DEFINE_INTEGRATOR_BLOCK 6
#define I_ERR_TOLLERANCE_AROUND_0 15 // unit 0,01°C. Set it quite restrictive !
#define I_ERR_WEIGHT 25 //impact of error on I part

extern uint8_t CTL_integratorBlock;


// ERRORS
#define CTL_ERR_BATT_LOW                (1<<7)
#define CTL_ERR_BATT_WARNING            (1<<6)
#define CTL_ERR_NA_5                    (1<<5)
#define CTL_ERR_RFM_SYNC                (1<<4)
#define CTL_ERR_MOTOR                   (1<<3)
#define CTL_ERR_MONTAGE                 (1<<2)
#define CTL_ERR_NA_1                    (1<<1)
#define CTL_ERR_NA_0                    (1<<0)

extern int32_t sumError;
extern int8_t CTL_interatorCredit;
extern uint8_t CTL_creditExpiration;
