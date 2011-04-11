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
 *              2008 Jiri Dobry (jdobry-at-centrum-dot-cz) 
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
 * \file       motor.c
 * \brief      functions to control the HR20 motor
 * \author     Dario Carluccio <hr20-at-carluccio-dot-de>; Jiri Dobry <jdobry-at-centrum-dot-cz> Thomas Vosshagen (mod. for THERMOTronic) <openhr20-at-vosshagen-dot-com>
 * \date       $Date: 2010-01-26 19:36:55 +0100 (Tue, 26 Jan 2010) $
 * $Rev: 272 $
 */


// AVR LibC includes
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/version.h>


// HR20 Project includes
#include "main.h"
#include "motor.h"
#include "lcd.h"
#include "rtc.h"
#include "eeprom.h"
#include "task.h"
#include "rs232_485.h"
#include "controller.h"
#include "com.h"
#include "debug.h"

// typedefs


// vars
volatile int16_t MOTOR_PosAct=0;      //!< actual position
int16_t MOTOR_PosMax=0;      /*!< position if complete open (100%) <BR>
                                          0 if not calibrated */
static volatile int16_t MOTOR_PosStop;     //!< stop at this position
volatile motor_dir_t MOTOR_Dir;          //!< actual direction
// bool MOTOR_Mounted;         //!< mountstatus true: if valve is mounted
int8_t MOTOR_calibration_step=-2; // not calibrated
volatile uint16_t motor_diag = 0;
static volatile uint16_t motor_diag_cnt = 0;

static volatile uint16_t motor_max_time_for_impulse;

static volatile uint16_t motor_timer = 0;

static volatile uint16_t last_eye_change = 0;
static volatile uint16_t longest_low_eye = 0;

static void MOTOR_Control(motor_dir_t); // control H-bridge of motor

static uint8_t MOTOR_wait_for_new_calibration = 5;

/*!
 *******************************************************************************
 *  Reset calibration data
 *
 *  \param cal_type 0 = unmounted
 *  \param cal_type 1 = 
 *  \param cal_type 2 = switch to manual  
 *  \param cal_type 3 = switch to auto  
 *  \note
 *  - must be called if the motor is unmounted
 ******************************************************************************/
void MOTOR_updateCalibration(uint8_t cal_type)
{
    if (cal_type == 0) {
        MOTOR_Control(stop);       // stop motor
        MOTOR_PosAct=0;                  // not calibrated
        MOTOR_PosMax=0;                  // not calibrated
        MOTOR_calibration_step=-2;     // not calibrated
        MOTOR_wait_for_new_calibration = 5;
        CTL_error &= ~CTL_ERR_MOTOR;
    } else {
        if (MOTOR_wait_for_new_calibration != 0) {
            MOTOR_wait_for_new_calibration--;
        } else {
            if (MOTOR_calibration_step==1) {
                MOTOR_calibration_step++;
                MOTOR_PosStop= +MOTOR_MAX_IMPULSES;
                MOTOR_Control(open);
            }
        }
        if (MOTOR_calibration_step==-2) {
            if (cal_type == 3) {
                MOTOR_ManuCalibration=-1;               // automatic calibration
                eeprom_config_save((uint16_t)(&config.MOTOR_ManuCalibration_L)-(uint16_t)(&config));
                eeprom_config_save((uint16_t)(&config.MOTOR_ManuCalibration_H)-(uint16_t)(&config));
            }  else if (cal_type == 2) { 
                MOTOR_ManuCalibration=0;               // not calibrated
            }
            MOTOR_calibration_step=1;
        }
    }
}


/*!
 *******************************************************************************
 *  \returns
 *   - actual position of motor in percent
 *   - 255 if motor is not calibrated
 ******************************************************************************/
uint8_t MOTOR_GetPosPercent(void)
{
    if ((MOTOR_PosMax > 10) && (MOTOR_calibration_step==0)){
        return (uint8_t) ( (MOTOR_PosAct * 10) / (MOTOR_PosMax/10) );
    } else {
        return 255;
    }
}

/*!
 *******************************************************************************
 * \returns
 *   - true:  calibration finished succesfully
 *   - false: motor is not calibrated 
 ******************************************************************************/
bool MOTOR_IsCalibrated(void)
{
    return ((MOTOR_PosMax != 0) && (MOTOR_calibration_step==0));
}


/*!
 *******************************************************************************
 * drive motor to desired position in percent
 * \param  percent desired endposition 0-100
 *         - 0 : closed
 *         - 100 : open
 *
 * \note   works only if calibrated before
 ******************************************************************************/
void MOTOR_Goto(uint8_t percent)
{
    // works only if calibrated
    if (MOTOR_calibration_step==0){
        // set stop position
        if (percent == 100) {
            MOTOR_PosStop = MOTOR_PosMax;
        } else if (percent == 0) {
            MOTOR_PosStop = 0;
        } else {
            // MOTOR_PosMax>>2 and 100>>2 => overload protection
            #if (MOTOR_MAX_IMPULSES>>2)*(100>>2) > INT16_MAX
             #error OVERLOAD possible
            #endif
            MOTOR_PosStop = ((int16_t)percent * (MOTOR_PosMax>>2)) / (100>>2);
        }
        // switch motor on
        if (MOTOR_Dir==stop) {
            int16_t a=MOTOR_PosAct; // volatile variable optimization
            int16_t s=MOTOR_PosStop; // volatile variable optimization
            if (a > s){
                MOTOR_Control(close);
            } else if (a < s){
                MOTOR_Control(open);
            }
        }
    }
}

static motor_dir_t MOTOR_LastDir=stop;
static uint8_t motor_diag_ignore=MOTOR_IGNORE_IMPULSES;
static uint8_t pine_last=0;

/*!
 *******************************************************************************
 * control motor movement
 *
 * \param  direction motordirection
 *         - stop
 *         - open
 *         - close
 *
 * \note PWM runs at 15,625 kHz
 *
 * \note Output for direction: \verbatim
     direction  PG3  PG4  PB7  PB4/PWM(OC0A)    PE3    PCINT4
       stop:     0    0    0    0                0      off
       open:     0    1    1   invert. mode      1      on
       close:    1    0    0   non inv mode      1      on       \endverbatim
 ******************************************************************************/
static void MOTOR_Control(motor_dir_t direction) {
    if (direction == stop){                             // motor off
        // photo eye
        TIMSK0 = 0;                                     // disable interrupt
#ifdef THERMOTRONIC
        PCMSK0 &= ~(1<<PCINT1);                         // disable interrupt
        MOTOR_HR20_PE3_P |= (1<<MOTOR_HR20_PE3);       // deactivate photo eye
            #if DEBUG_PRINT_MOTOR
#ifdef THERMOTRONIC
                COM_putchar((PINE & _BV(PE1))?'Y':'y');
#else
                COM_putchar((PINE & _BV(PE4))?'Y':'y');
#endif
            #endif
		MOTOR_H_BRIDGE_stop();
        // stop pwm signal
        TCCR0A = 0;
#else
        PCMSK0 &= ~(1<<PCINT4);                         // disable interrupt
        MOTOR_HR20_PE3_P &= ~(1<<MOTOR_HR20_PE3);       // deactivate photo eye
            #if DEBUG_PRINT_MOTOR
                COM_putchar((PINE & _BV(PE4))?'Y':'y');
            #endif
		MOTOR_H_BRIDGE_stop();
        // stop pwm signal
        TCCR0A = (1<<WGM00) | (1<<WGM01); // 0b 0000 0011
#endif
	    MOTOR_Dir = stop;
    } else {                                            // motor on
        if (MOTOR_Dir != direction){
            if (MOTOR_Dir != MOTOR_LastDir) {
                motor_diag_cnt=0; last_eye_change=0; longest_low_eye = 0;
                motor_diag_ignore = MOTOR_IGNORE_IMPULSES;
                MOTOR_Dir = MOTOR_LastDir;
            }
		    MOTOR_Dir = direction;
            // photo eye
            MOTOR_HR20_PE3_P |= (1<<MOTOR_HR20_PE3);    // activate photo eye
            motor_max_time_for_impulse = ((uint16_t)config.motor_speed *
                      (((MOTOR_calibration_step == 0))?   
                            (uint16_t)config.motor_end_detect_run
                            :(uint16_t)config.motor_end_detect_cal) / 100)<<3;
            motor_timer = motor_max_time_for_impulse<<2; // *4 (for motor start-up)
            TIFR0 = (1<<TOV0);
            TCNT0 = 0;
            TIMSK0 = (1<<TOIE0); //enable interrupt from timer0 overflow
            pine_last=PINE;
#ifdef THERMOTRONIC
            PCMSK0 |= (1<<PCINT1); // enable interrupt from eye
#else
            PCMSK0 |= (1<<PCINT4); // enable interrupt from eye
#endif
            OCR0A = config.motor_pwm_max;
            if ( direction == close) {
                // set pins of H-Bridge
                MOTOR_H_BRIDGE_close();
#ifdef THERMOTRONIC
				TCCR0A = (1<<CS00); //start timer (no PWM)
#else
                // set PWM non inverting mode
                TCCR0A = (1<<WGM00) | (1<<WGM01) | (1<<COM0A1) | (1<<CS00);
#endif
            // close
            } else {
                // set pins of H-Bridge
				MOTOR_H_BRIDGE_open();
#ifdef THERMOTRONIC
				TCCR0A = (1<<CS00); //start timer (no PWM)
#else
                TCCR0A=(1<<WGM00)|(1<<WGM01)|(1<<COM0A1)|(1<<COM0A0)|(1<<CS00);
#endif
            }
            #if DEBUG_PRINT_MOTOR
#ifdef THERMOTRONIC
                COM_putchar((PINE & _BV(PE1))?'X':'x');
#else
                COM_putchar((PINE & _BV(PE4))?'X':'x');
#endif
            #endif
		}
    }
}

/*!
 *******************************************************************************
 * motor eye pulse
 *
 * \note called by TASK_MOTOR_PULSE event
 *
 ******************************************************************************/
void MOTOR_timer_pulse(void) {
    if (motor_diag <= MOTOR_MAX_VALID_TIMER) {
        if (motor_diag_ignore == 0) {  
            {
                int16_t chg = ((int16_t)((motor_diag+4)>>3)- (int16_t)config.motor_speed)
                        *config.motor_speed_ctl_gain/100;
                if (chg > config.motor_pwm_max_step) { chg=config.motor_pwm_max_step; }
                else if (chg < -config.motor_pwm_max_step) chg=-config.motor_pwm_max_step;

                {
                    int16_t pwm = OCR0A;
                    pwm += chg;
                    if (pwm > config.motor_pwm_max) { pwm=config.motor_pwm_max; }
                    else if (pwm < config.motor_pwm_min) { pwm=config.motor_pwm_min; }
                    OCR0A = pwm;
                }
            }
		} else {
		   motor_diag_ignore--;
        }		
    } 

    #if DEBUG_PRINT_MOTOR
        COM_debug_print_motor(MOTOR_Dir, motor_diag, OCR0A);
    #endif
}

/*!
 *******************************************************************************
 * motor timer
 *
 * \note called by TASK_MOTOR_STOP event
 *
 ******************************************************************************/
void MOTOR_timer_stop(void) {
	motor_dir_t d = MOTOR_Dir;
    MOTOR_Control(stop);
    if (motor_timer>0) { // normal stop on wanted position 
            if (MOTOR_calibration_step != 0) {
                MOTOR_calibration_step = -1;     // calibration error
		        CTL_error |=  CTL_ERR_MOTOR;
            }
	} else { // stop on timeout
        if (d == open) { // stopped on end
            {
                int16_t a = MOTOR_PosAct; // volatile variable optimization
                if (MOTOR_calibration_step == 2) {
                    MOTOR_PosMax = a;
                    if (MOTOR_ManuCalibration==0) {
                        if (a >= MOTOR_MIN_IMPULSES) {
                            MOTOR_ManuCalibration = a;
                            eeprom_config_save((uint16_t)(&config.MOTOR_ManuCalibration_L)-(uint16_t)(&config));
                            eeprom_config_save((uint16_t)(&config.MOTOR_ManuCalibration_H)-(uint16_t)(&config));
                            MOTOR_calibration_step = 0;
                        } else {
                            MOTOR_calibration_step = -1;     // calibration error
                            CTL_error |=  CTL_ERR_MOTOR;
                        }
                    } else {
                        MOTOR_Control(close);
                        MOTOR_PosStop= (a-MOTOR_MAX_IMPULSES);
                        MOTOR_calibration_step = 3;
                    }
                } else {
                    MOTOR_PosAct = MOTOR_PosMax; // motor position cleanup
                }
            }
        } else if (d == close) { // stopped on end
            {
                if (MOTOR_calibration_step == 3 ) {
                    MOTOR_calibration_step = 0;     // calibration DONE
                    MOTOR_PosMax -= MOTOR_PosAct;
                } else if (MOTOR_PosAct>MOTOR_MIN_IMPULSES) {
                    MOTOR_calibration_step = -1;     // calibration error
                    CTL_error |=  CTL_ERR_MOTOR;
                }
                MOTOR_PosAct = 0; // cleanup position
            }
        }
    }
    if ((MOTOR_calibration_step == 0) &&
        (MOTOR_PosMax<MOTOR_MIN_IMPULSES)) {
        MOTOR_calibration_step = -1;     // calibration error
        CTL_error |=  CTL_ERR_MOTOR;
    } 
}

// interrupts: 

/*!
 *******************************************************************************
 * Pinchange Interupt INT0
 *
 * \note count light eye impulss: \ref MOTOR_PosAct
 *
 * \note create TASK_UPDATE_MOTOR_POS
 ******************************************************************************/
ISR (PCINT0_vect){
	uint8_t pine=PINE;
	#if (defined COM_RS232) || (defined COM_RS485)
		if ((pine & (1<<PE0)) == 0) {
			RS_enable_rx(); // it is macro, not function
		    PCMSK0 &= ~(1<<PCINT0); // deactivate interrupt
		}
	#endif
    // motor eye
    // count  HIGH impulses for HR20 and LOW Pulses for THERMOTRONIC
#ifdef THERMOTRONIC
    if ((PCMSK0 & (1<<PCINT1)) && (((pine ^ pine_last) & (1<<PE1)) != 0)) {
#else
    if ((PCMSK0 & (1<<PCINT4)) && (((pine ^ pine_last) & (1<<PE4)) != 0)) {
#endif
        uint16_t dur = motor_diag_cnt - last_eye_change;
        last_eye_change = motor_diag_cnt;
#ifdef THERMOTRONIC
        if ((pine & _BV(PE1))!=0) {
#else
        if ((pine & _BV(PE4))==0) {
#endif
            if (dur > (config.motor_eye_high<<1)) {
                if (longest_low_eye > (config.motor_eye_low<<1)) {
                    MOTOR_PosAct+=MOTOR_Dir;
                    motor_diag = motor_diag_cnt;
                    longest_low_eye=0;
                    motor_diag_cnt=0;
                    last_eye_change=0;
                    task|=TASK_MOTOR_PULSE;
                    if (MOTOR_PosAct == MOTOR_PosStop) {
                        // motor fast STOP
                        MOTOR_H_BRIDGE_stop();
                        // complete STOP will be done later
#ifdef THERMOTRONIC
                		TCCR0A = 0;//tvossi (1<<WGM00) | (1<<WGM01); // 0b 0000 0011
                        task|=(TASK_MOTOR_STOP);
                        PCMSK0 &= ~(1<<PCINT1); // disable eye interrupt
#else
                		TCCR0A = (1<<WGM00) | (1<<WGM01); // 0b 0000 0011
                        task|=(TASK_MOTOR_STOP);
                        PCMSK0 &= ~(1<<PCINT4); // disable eye interrupt
#endif
                        TIMSK0 = 0; // disable timmer 1 interrupt 
                    } else {
                        motor_timer = motor_max_time_for_impulse;
                    }
                } else {
                    #if DEBUG_PRINT_MOTOR
                        COM_putchar('~');
                    #endif
                }
            }
        } else {
            if (dur > longest_low_eye) longest_low_eye=dur;
        }
    }
	pine_last=pine;
}

/*! 
 *******************************************************************************
 * Timer0 overflow interupt
 * runs at 15,625 kHz (variant 1953.125)
 * \note Timer0 is only active if the motor is running
 ******************************************************************************/
ISR (TIMER0_OVF_vect){
    if ( motor_timer > 0) {
        motor_diag_cnt++;
        motor_timer--;
    } else {
        motor_diag = motor_diag_cnt;
        // motor fast STOP
        MOTOR_H_BRIDGE_stop();
        // complete STOP will be done later
#ifdef THERMOTRONIC
		TCCR0A = 0;//tvossi (1<<WGM00) | (1<<WGM01); // 0b 0000 0011
        task|=(TASK_MOTOR_STOP);
        PCMSK0 &= ~(1<<PCINT1); // disable eye interrupt
#else
		TCCR0A = (1<<WGM00) | (1<<WGM01); // 0b 0000 0011
        task|=(TASK_MOTOR_STOP);
        PCMSK0 &= ~(1<<PCINT4); // disable eye interrupt
#endif
        TIMSK0 = 0; // disable timmer 1 interrupt 

    }
}
