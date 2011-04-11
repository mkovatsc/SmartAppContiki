/*
 *  Open HR20
 *
 *  target:     ATmega169 @ 4 MHz in Honnywell Rondostat HR20E
 *
 *  compiler:   WinAVR-20071221
 *              avr-libc 1.6.0
 *              GCC 4.2.2
 *
 *  copyright:  2008 Dario Carluccio (hr20-at-carluccio-dot-de)
 *				2009 Thomas Vosshagen (mod. for THERMOTronic) (openhr20-at-vosshagen-dot-com)
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
 * \file       motor.h
 * \brief      header file for motor.c, functions to control the HR20 motor
 * \author     Dario Carluccio <hr20-at-carluccio-dot-de> Thomas Vosshagen (mod. for THERMOTronic) <openhr20-at-vosshagen-dot-com>
 * \date       $Date: 2009-03-30 22:11:55 +0200 (Mon, 30 Mar 2009) $
 * $Rev: 229 $
 */


/*****************************************************************************
*   Macros
*****************************************************************************/

// How is the H-Bridge connected to the AVR?
#define MOTOR_HR20_PE3     PE3     //!< HR20: pin to activate photo eye
#define MOTOR_HR20_PE3_P   PORTE   //!< HR20: port to activate photo eye

static inline void MOTOR_H_BRIDGE_open(void) {
   PORTG  =  (1<<PG4);   // PG3 LOW, PG4 HIGH
#ifndef THERMOTRONIC   //not needed (no enable-Pin for motor) 
   PORTB |=  (1<<PB7);   // PB7 HIGH
#endif
}
static inline void MOTOR_H_BRIDGE_close(void) {
   PORTG  =  (1<<PG3);   // PG3 HIGH, PG4 LOW
#ifndef THERMOTRONIC   //not needed (no enable-Pin for motor) 
   PORTB &= ~(1<<PB7);   // PB7 LOW
#endif
}
static inline void MOTOR_H_BRIDGE_stop(void) {
   PORTG  =  0;          // PG3 LOW, PG4 LOW
#ifndef THERMOTRONIC   //not needed (no enable-Pin for motor) 
   PORTB &= ~(1<<PB7);   // PB7 LOW
#endif
}


//! How many photoeye impulses maximal form one endposition to the other. <BR>
//! The value measured on a HR20 are 737 to 740 (385 - 390 for THERMOTRONIC)= so more than 1000 (500) should
//! never occure if it is mounted
#ifdef THERMOTRONIC
#define	MOTOR_MAX_IMPULSES 500
#define	MOTOR_MIN_IMPULSES 50
#else
#define	MOTOR_MAX_IMPULSES 1000
#define	MOTOR_MIN_IMPULSES 100
#endif
#define MOTOR_MAX_VALID_TIMER 20000
#define MOTOR_IGNORE_IMPULSES       2
#define DEFAULT_motor_max_time_for_impulse 3072
#define DEFAULT_motor_eye_noise_protection 120

/*****************************************************************************
*   Typedefs
*****************************************************************************/
//! motor direction
typedef enum {                                      
    close=-1, stop=0, open=1 
} motor_dir_t;

/*****************************************************************************
*   Prototypes
*****************************************************************************/
#define MOTOR_Init(void) (MOTOR_updateCalibration(1)) // Init motor control
void MOTOR_Goto(uint8_t);                     // Goto position in percent
bool MOTOR_IsCalibrated(void);                // is motor successful calibrated?
void MOTOR_updateCalibration(uint8_t cal_type);            // reset the calibration 
uint8_t MOTOR_GetPosPercent(void);  // get percental position of motor (0-100%)
void MOTOR_timer_stop(void);
void MOTOR_timer_pulse(void);

#define timer0_need_clock() (TCCR0A & ((1<<CS02)|(1<<CS01)|(1<<CS00)))

extern volatile int16_t MOTOR_PosAct;
extern volatile uint16_t motor_diag;
extern int8_t MOTOR_calibration_step;
extern uint16_t motor_diag_count;
extern volatile motor_dir_t MOTOR_Dir;          //!< actual direction
