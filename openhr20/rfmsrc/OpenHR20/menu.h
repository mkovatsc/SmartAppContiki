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
 * \file       menu.h
 * \brief      menu view & controler for free HR20E project
 * \author     Jiri Dobry <jdobry-at-centrum-dot-cz>
 * \date       $Date: 2011-02-24 15:51:55 +0100 (Thu, 24 Feb 2011) $
 * $Rev: 293 $
 */

#pragma once

extern int8_t menu_auto_update_timeout;
bool menu_controller(bool new_state); 
void menu_view(bool update);

extern bool menu_locked; 

extern uint32_t hourbar_buff;

static inline void menu_update_hourbar(uint8_t dow) {
    hourbar_buff = RTC_DowTimerGetHourBar(dow);
}

 
