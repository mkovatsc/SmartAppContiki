/*
 *  Open HR20
 *
 *  target:     ATmega32 @ 10 MHz in Honnywell Rondostat HR20E
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
 * \file       task.h
 * \brief      task definition for main loop
 * \author     Jiri Dobry <jdobry-at-centrum-dot-cz>
 * \date       $Date: 2010-02-02 22:26:39 +0100 (Tue, 02 Feb 2010) $
 * $Rev: 276 $
 */

#include "config.h"

#pragma once

// #include <stdint.h>

//extern volatile unsigned char task;
//#define  task        GPIOR0
extern volatile uint8_t task;
// if task is not SFR disable this:
#define  TASK_IS_SFR 0


#define TASK_0_BIT	          0
#define TASK_RTC_BIT	      1
#define TASK_TIMER_BIT        2
#define TASK_3_BIT	            3
#define TASK_4_BIT              4
#define TASK_5_BIT	            5
#define TASK_COM_BIT            6  
#if (RFM==1)
  #define TASK_RFM_BIT          7  
#endif 

#define TASK_0			     (1 << TASK_0_BIT)
#define TASK_RTC		     (1 << TASK_RTC_BIT)
#define TASK_TIMER		     (1 << TASK_TIMER_BIT)
#define TASK_3  		     (1 << TASK_3_BIT)
#define TASK_4               (1 << TASK_4_BIT)
#define TASK_5 	             (1 << TASK_5_BIT)
#define TASK_COM		     (1 << TASK_COM_BIT)
#if (RFM==1)
  #define TASK_RFM		     (1 << TASK_RFM_BIT)
#endif


