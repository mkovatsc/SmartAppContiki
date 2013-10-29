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
 * \file       com.c
 * \brief      comunication
 * \author     Jiri Dobry <jdobry-at-centrum-dot-cz>
 * \date       $Date: 2011-03-21 22:58:53 +0100 (Mon, 21 Mar 2011) $
 * $Rev: 351 $
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/wdt.h>


#include "config.h"
#include "com.h"
#include "../common/rs232_485.h"
#include "main.h"
#include "../common/rtc.h"
#include "adc.h"
#include "task.h"
#include "watch.h"
#include "eeprom.h"
#include "controller.h"
#include "menu.h"
#include "../common/wireless.h"
#include "debug.h"


#define TX_BUFF_SIZE 128
#define RX_BUFF_SIZE 128

#define ENABLE_LOCAL_COMMANDS 1

static char tx_buff[TX_BUFF_SIZE];
static char rx_buff[RX_BUFF_SIZE];

static uint8_t tx_buff_in=0;
static uint8_t tx_buff_out=0;
static uint8_t rx_buff_in=0;
static uint8_t rx_buff_out=0;



/*!
 *******************************************************************************
 *  \brief transmit bytes
 *
 *  \note
 ******************************************************************************/
void COM_putchar(char c) {
	cli();
	if ((tx_buff_in+1)%TX_BUFF_SIZE!=tx_buff_out) {
		tx_buff[tx_buff_in++]=c;
		tx_buff_in%=TX_BUFF_SIZE;
	}
	sei();
}

/*!
 *******************************************************************************
 *  \brief support for interrupt for transmit bytes
 *
 *  \note
 ******************************************************************************/
char COM_tx_char_isr(void) {
	char c='\0';
	if (tx_buff_in!=tx_buff_out) {
		c=tx_buff[tx_buff_out++];
		tx_buff_out%=TX_BUFF_SIZE;
	}
	return c;
}

static volatile uint8_t COM_requests; 
/*!
 *******************************************************************************
 *  \brief support for interrupt for receive bytes
 *
 *  \note
 ******************************************************************************/
void COM_rx_char_isr(char c) {
	if (c!='\0') {  // ascii based protocol, \0 char is not alloweed, ignore it

		if (c=='\r') c='\n';  // mask diffrence between operating systems
		rx_buff[rx_buff_in++]=c;
		rx_buff_in%=RX_BUFF_SIZE;
		if (rx_buff_in==rx_buff_out) { // buffer overloaded, drop oldest char 
			rx_buff_out++;
			rx_buff_out%=RX_BUFF_SIZE;
		}

		if (c=='\n') {
			task |= TASK_COM;
			COM_requests++;
		}
	}
}

/*!
 *******************************************************************************
 *  \brief receive bytes
 *
 *  \note
 ******************************************************************************/
static char COM_getchar(void) {
	char c;
	cli();
	if (rx_buff_in!=rx_buff_out) {
		c=rx_buff[rx_buff_out++];
		rx_buff_out%=RX_BUFF_SIZE;
		COM_requests--;
	} else {
		COM_requests=0;
		c='\0';
	}
	sei();
	return c;
}

/*!
 *******************************************************************************
 *  \brief flush output buffer
 *
 *  \note
 ******************************************************************************/
void COM_flush (void) {
	if (tx_buff_in!=tx_buff_out) {
#if (defined COM_RS232) || (defined COM_RS485)
		RS_startSend();
#elif THERMOTRONIC	//UART for THERMOTRONIC not implemented
#else
#error "need todo"
#endif
	}
}


/*!
 *******************************************************************************
 *  \brief helper function print 2 digit dec number
 *
 *  \note only unsigned numbers
 ******************************************************************************/
static void print_decXX(uint8_t i) {
	if (i>=100) {
		COM_putchar(i/100+'0');
		i%=100;
	}
	COM_putchar(i/10+'0');
	COM_putchar(i%10+'0');
}


/*!
 *******************************************************************************
 *  \brief helper function print 1 digit dec number
 *
 *  \note only unsigned numbers
 ******************************************************************************/
static void print_digit(uint8_t i) {
	COM_putchar(i%10+'0');
}




/*!
 *******************************************************************************
 *  \brief helper function print 3 digit dec number
 *
 *  \note only unsigned numbers
 ******************************************************************************/
static void print_decXXX(uint8_t i) {
	if (i>=100) {
		COM_putchar(i/100+'0');
		i%=100;
	}
	else{
		COM_putchar('0');
	}
	COM_putchar(i/10+'0');
	COM_putchar(i%10+'0');
}




/*!
 *******************************************************************************
 *  \brief helper function print 4 digit dec number
 *
 *  \note only unsigned numbers
 ******************************************************************************/
static void print_decXXXX(uint16_t i) {
	print_decXX(i/100);
	print_decXX(i%100);
}



/*!
 *******************************************************************************
 *  \brief helper function print 2 digit hex number
 *
 *  \note only unsigned numbers
 ******************************************************************************/
static void print_hexXX(uint8_t i) {
	uint8_t x = i>>4;
	if (x>=10) {
		COM_putchar(x+'a'-10);	
	} else {
		COM_putchar(x+'0');
	}	
	x = i & 0xf;
	if (x>=10) {
		COM_putchar(x+'a'-10);	
	} else {
		COM_putchar(x+'0');
	}	
}


/*!
 *******************************************************************************
 *  \brief helper function print 4 digit hex number
 *
 *  \note
 ******************************************************************************/
static void print_hexXXXX(uint16_t i) {
	print_hexXX(i>>8);
	print_hexXX(i&0xff);
}

/*!
 *******************************************************************************
 *  \brief helper function print string without \n2 digit dec number
 *
 *  \note
 ******************************************************************************/
static void print_s_p(const char * s) {
	char c;
	for (c = pgm_read_byte(s); c; ++s, c = pgm_read_byte(s)) {
		COM_putchar(c);
	}
}

/*!
 *******************************************************************************
 *  \brief helper function print version string
 *
 *  \note
 ******************************************************************************/
static void print_version(bool sync) {
	const char * s = (PSTR(VERSION_STRING "\n"));
	COM_putchar('V');
	char c;
	for (c = pgm_read_byte(s); c; ++s, c = pgm_read_byte(s)) {
		COM_putchar(c);
	}
	COM_flush();
}



/*!
 *******************************************************************************
 *  \brief init communication
 *
 *  \note
 ******************************************************************************/
void COM_init(void) {
	print_version(false);
#if (THERMOTRONIC!=1)
	RS_Init();
#endif
	COM_flush();
}


/*!
 *******************************************************************************
 *  \brief Event Wheel Ticks *
 *  \note
 ******************************************************************************/
void COM_send_wheel_event(int8_t t){
	COM_putchar('E');
	COM_putchar('W');
	COM_putchar(':');
	uint8_t ticks = (uint8_t) t+128;
	print_decXXX(ticks);
	COM_putchar('\n');
	COM_flush();
}


/*!
 *******************************************************************************
 *  \brief Event Mode Change *
 *  \note
 ******************************************************************************/
void COM_send_mode_event(int8_t m){
	COM_putchar('E');
	COM_putchar('M');
	COM_putchar(':');
	print_decXXX(m);
	COM_putchar('\n');
	COM_flush();
}



/*!
 *******************************************************************************
 *  \brief Event Temperature Change *
 *  \note
 ******************************************************************************/
void COM_send_temperature_event(uint16_t t){
	COM_putchar('E');
	COM_putchar('T');
	COM_putchar(':');
	print_decXXXX(t);
	COM_putchar('\n');
	COM_flush();
}

/*!
 *******************************************************************************
 *  \brief Send Battery Change *
 *  \note
 ******************************************************************************/
void COM_send_battery_event(uint16_t t){
	COM_putchar('E');
	COM_putchar('B');
	COM_putchar(':');
	print_decXXXX(t);
	COM_putchar('\n');
	COM_flush();
}


/*!
 *******************************************************************************
 *  \brief Event Error *
 *  \note
 ******************************************************************************/
void COM_send_error_event(uint8_t e){
	COM_putchar('E');
	COM_putchar('E');
	COM_putchar(':');
	print_hexXX(e);
	COM_putchar('\n');
	COM_flush();
}

/*!
 *******************************************************************************
 *  \brief Event Valve Changed *
 *  \note
 ******************************************************************************/
void COM_send_valve_event(uint8_t e){
	COM_putchar('E');
	COM_putchar('V');
	COM_putchar(':');
	print_decXX(e);
	COM_putchar('\n');
	COM_flush();
}

/*!
 *******************************************************************************
 *  \brief Print debug line
 *
 *  \note
 ******************************************************************************/
void COM_print_debug(uint8_t type) {
	print_s_p(PSTR("D: "));
	print_hexXX(RTC_GetDayOfWeek()+0xd0);
	COM_putchar(' ');
	print_decXX(RTC_GetDay());
	COM_putchar('.');
	print_decXX(RTC_GetMonth());
	COM_putchar('.');
	print_decXX(RTC_GetYearYY());
	COM_putchar(' ');
	print_decXX(RTC_GetHour());
	COM_putchar(':');
	print_decXX(RTC_GetMinute());
	COM_putchar(':');
	print_decXX(RTC_GetSecond());
	COM_putchar(' ');
	switch(CTL_thermostat_mode){
		case manual_target:
			COM_putchar('M');
			COM_putchar('T');
			break;
		case manual_timer:
			COM_putchar('M');
			COM_putchar('P');
			break;
		case radio_target:
			COM_putchar('R');
			COM_putchar('T');
			break;
		case radio_valve:
			COM_putchar('R');
			COM_putchar('V');
			break;
		default:
			COM_putchar('-');
			COM_putchar('-');
	}
	print_s_p(PSTR(" V: "));
	print_decXX(valve_wanted);
	print_s_p(PSTR(" I: "));
	print_decXXXX(temp_average);
	print_s_p(PSTR(" S: "));
	if (CTL_temp_wanted_last>TEMP_MAX+1) {
		print_s_p(PSTR("BOOT"));
	} else {
		print_decXXXX(calc_temp(CTL_temp_wanted_last));
	}
	print_s_p(PSTR(" B: "));
	print_decXXXX(bat_average);
#if DEBUG_PRINT_I_SUM
	print_s_p(PSTR(" Is: "));
	print_hexXXXX(sumError>>16);
	print_hexXXXX(sumError);
	print_s_p(PSTR(" Ib: ")); //jr
	print_hexXX(CTL_integratorBlock);
	print_s_p(PSTR(" Ic: ")); //jr
	print_hexXX(CTL_interatorCredit);
	print_s_p(PSTR(" Ie: ")); //jr
	print_hexXX(CTL_creditExpiration);
#endif
	if (CTL_error!=0) {
		print_s_p(PSTR(" E:"));
		print_hexXX(CTL_error);
	}                   
	if (type>0) {
		print_s_p(PSTR(" X"));
	}
	if (mode_window()) {
		print_s_p(PSTR(" W"));
	}
	if (menu_locked) {
		print_s_p(PSTR(" L"));
	}
	COM_putchar('\n');
	COM_flush();
}

/*! 
	\note dirty trick with shared array for \ref COM_hex_parse and \ref COM_commad_parse
	code size optimalization
 */
static uint8_t com_hex[8];


/*!
 *******************************************************************************
 *  \brief parse hex number (helper function)
 *
 *	\note hex numbers use ONLY lowcase chars, upcase is reserved for commands
 *	
 ******************************************************************************/
static char COM_hex_parse (uint8_t n) {
	uint8_t i;
	for (i=0;i<n;i++) {
		uint8_t c = COM_getchar()-'0';
		if ( c>9 ) {  // chars < '0' overload var c
			if ((c>=('a'-'0')) && (c<=('f'-'0'))) {
				c-= (('a'-'0')-10);
			} else return c+'0';
		}
		if (i&1) {
			com_hex[i>>1]+=c;
		} else {
			com_hex[i>>1]=(uint8_t)c<<4;
		}
	}
	{
		char c;
		if ((c=COM_getchar())!='\n') return c;
	}
	return '\0';
}

/*!
 *******************************************************************************
 *  \brief print X[xx]=
 *
 ******************************************************************************/
static void print_idx(char t, uint8_t i) {
	COM_putchar(t);
	COM_putchar('[');
	print_hexXX(i);
	COM_putchar(']');
	COM_putchar('=');
}



/*!
 *******************************************************************************
 *  \brief parse command
 *
 *  \note commands have FIXED format
 *  \note command X.....\n    - X is upcase char as commad name, \n is termination char
 *  \note hex numbers use ONLY lowcase chars, upcase is reserved for commands
 *  \note   V\n - print version information
 *  \note   D\n - print status line 
 *  \note   Taa\n - print watched variable aa (return 2 or 4 hex numbers) see to \ref watch.c
 *  \note   Gaa\n - get configuration byte with hex address aa see to \ref eeprom.h 0xff address returns EEPROM layout version
 *  \note   Saadd\n - set configuration byte aa to value dd (hex)
 *  \note   Rab\n - get timer for day a slot b, return cddd=(timermode c time ddd) (hex)
 *  \note   Wabcddd\n - set timer  for day a slot b timermode c time ddd (hex)
 *  \note   B1324\n - reboot, 1324 is password (fixed at this moment)
 *  \note   Yyymmdd\n - set, year yy, month mm, day dd; HEX values!!!
 *  \note   Hhhmmss\n - set, hour hh, minute mm, second ss; HEX values!!!
 *  \note   Axx\n - set wanted temperature [unit 0.5C]
 *  \note   Mxx\n - set mode and close window (00=manu 01=auto fd=nochange/close window only)
 * 	\note	Lxx\n - Lock keys, and return lock status (00=unlock, 01=lock, 02=status only)
 *	
 ******************************************************************************/
void COM_command_parse (void) {
	char c;
	while (COM_requests) {
		switch(c=COM_getchar()) {
			case 'D':
				if (COM_getchar()=='\n') COM_print_debug(1);
				c='\0';
				break;
			case 'V':
				//Watch Variable
				{
					if (COM_hex_parse(1*2)!='\0') { break; }
					print_idx(c,com_hex[0]);
					print_hexXXXX(watch(com_hex[0]));
				}
				break;
			case 'R':  //RESET
				{
					if (COM_hex_parse(2*2)!='\0') { break; }
					if ((com_hex[0]==0x13) && (com_hex[1]==0x24)) {
						cli();
						wdt_enable(WDTO_15MS); //wd on,15ms
						while(1); //loop till reset
					}
				}
				break;
			case 'S':
				{
					char sub = COM_getchar();
					COM_putchar(c);
					switch (sub) {
						case 'M': 
							{
								COM_putchar(sub);
								if (COM_hex_parse(1*2)!='\0'){ 
									COM_putchar('0');
									break; 
								}
								if (com_hex[0]>=0 && com_hex[0]<=4){
									CTL_change_mode(com_hex[0]);
									COM_putchar('1');
									break;
								}
								else {
									COM_putchar('0');
									break;
								}	
								break;
							}
						case 'Y':
							{
								COM_putchar(sub);
								if (COM_hex_parse(6*2)!='\0') {
									COM_putchar('0');
									break; 
								}
								RTC_SetYear(com_hex[0]);
								RTC_SetMonth(com_hex[1]);
								RTC_SetDay(com_hex[2]);
                RTC_SetHour(com_hex[3]);
                RTC_SetMinute(com_hex[4]);
                RTC_SetSecond(com_hex[5]);
								COM_putchar('1');
								break;
							}
						case 'C':
							{
								COM_putchar(sub);
								if (COM_hex_parse(2*2)!='\0') { 
									COM_putchar('0');
									break; }
								if (com_hex[0]<CONFIG_RAW_SIZE) {
									config_raw[com_hex[0]]=(uint8_t)(com_hex[1]);
									eeprom_config_save(com_hex[0]);
									COM_putchar('1');
								}
								else{
									COM_putchar('0');
								}
								break;
							}
						case 'T':
							{
								COM_putchar(sub);
								if (COM_hex_parse(1*2)!='\0') { 
									COM_putchar('0');
									break;
								}
								if (com_hex[0]<TEMP_MIN-1 || com_hex[0]>TEMP_MAX+1) { 
									COM_putchar('0');
									break;
								}
                CTL_set_temp_auto(com_hex[0]);
								CTL_change_mode(radio_target);
								COM_putchar('1');
								break;
							}
						case 'V':
							{
								COM_putchar(sub);
								if (COM_hex_parse(1*2)!='\0') { 
									COM_putchar('0');
									break;
								}
								if (com_hex[0]>config.valve_max) {
									CTL_valve_wanted=config.valve_max;
								}
								else if (com_hex[0]<config.valve_min) {
									CTL_valve_wanted=config.valve_min;
								}
								else{
									CTL_valve_wanted=com_hex[0];
								}
                CTL_change_mode(radio_valve);
								COM_putchar('1');
								break;
							}
						case 'D':
							{
								COM_putchar(sub);
								if (COM_hex_parse(1*2)!='\0') { 
									COM_putchar('0');
									break;
								}
								if (com_hex[0] == 0){
									COM_putchar('0');
									break;
								}
								CTL_temp_threshold= ((int16_t) com_hex[0])*10;
								COM_putchar('1');
								break;
							}	
						case 'B':
							{
								COM_putchar(sub);
								if (COM_hex_parse(1*2)!='\0') { 
									COM_putchar('0');
									break;
								}
								if (com_hex[0] == 0){
									COM_putchar('0');
									break;
								}
								CTL_bat_threshold= ((int16_t) com_hex[0]);
								COM_putchar('1');
								break;
							}	

						default:
							COM_putchar('0');
							COM_putchar('0');
							break;
					}
					break;

				}

			case 'G':
				{
					char sub = COM_getchar();

					COM_putchar(c);
					switch (sub) {
						case 'M':
							{
								COM_putchar(sub);
								COM_putchar('1');
								COM_putchar(':');
								print_decXX(CTL_thermostat_mode);
								break;
							}
						case 'Y':
							{
								COM_putchar(sub);
								COM_putchar('1');
								COM_putchar(':');
                print_decXX(RTC_GetYearYY());
                COM_putchar('-');
                print_decXX(RTC_GetMonth());
                COM_putchar('-');
								print_decXX(RTC_GetDay());
								COM_putchar(' ');
                print_decXX(RTC_GetHour());
                COM_putchar(':');
                print_decXX(RTC_GetMinute());
                COM_putchar(':');
                print_decXX(RTC_GetSecond());
								break;
							}
						case 'C':
							{
								COM_putchar(sub);
								if (COM_hex_parse(1*2)!='\0') { 
									COM_putchar('0');
									break; 
								}
								COM_putchar('1');
								COM_putchar(':');
								print_idx('E',com_hex[0]);
								if (com_hex[0]==0xff) {
									print_hexXX(EE_LAYOUT);
								}
								else {
									print_hexXX(config_raw[com_hex[0]]);
								}
								break;
							} 
						case 'B':
							{
								COM_putchar(sub);
								COM_putchar('1');	
								COM_putchar(':');	
								print_decXXXX(bat_average);
								break;
							}
						case 'T':
							{
								COM_putchar(sub);
								COM_putchar('1');	
								COM_putchar(':');	
								print_decXXXX(calc_temp(CTL_temp_wanted_last));
								break;
							}
						case 'V':	
							{
								COM_putchar(sub);
								COM_putchar('1');	
								COM_putchar(':');	
								print_decXX(valve_wanted);
								break;
							}
						case 'I':
							{
								COM_putchar(sub);
								COM_putchar('1');	
								COM_putchar(':');	
								print_decXXXX(temp_average);
								break;
							}
						case 'P':
							{
								COM_putchar(sub);
								COM_putchar('1');
								COM_putchar(':');
								COM_putchar('F');
								print_hexXX(config_raw[1]);
								COM_putchar(',');
								COM_putchar('E');
								print_hexXX(config_raw[2]);
								COM_putchar(',');
								COM_putchar('C');
								print_hexXX(config_raw[3]);
								COM_putchar(',');
								COM_putchar('S');
								print_hexXX(config_raw[4]);
								break;
							}
						case 'S':
							{
								COM_putchar(sub);
								if (COM_hex_parse(1)!='\0') { 
									COM_putchar('0');

									break;
								}
								if ( (com_hex[0]>>4) < 0 || (com_hex[0]>>4) >= 8) {
									COM_putchar('0');
									break;
								}
								COM_putchar('1');
								COM_putchar(':');
								print_digit(com_hex[0]>>4);
								int i;
								for (i=0;i<RTC_TIMERS_PER_DOW;i++){
									COM_putchar(',');
									print_hexXXXX(eeprom_timers_read_raw(
												timers_get_raw_index((com_hex[0]>>4),i)));
								}
								break;

							}
						case 'D':
							{
								COM_putchar(sub);
								COM_putchar('1');
								COM_putchar(':');
								print_decXXXX(CTL_temp_threshold);
								break;
							}		
						case 'A':
							{
								COM_putchar(sub);
								COM_putchar('1');
								COM_putchar(':');
								print_decXXXX(CTL_bat_threshold);
								break;
							}		
						default:
							{
								COM_putchar('0');
								COM_putchar('0');
								break;
							}
					}
					break;
				}

			case 'L':
				if (COM_hex_parse(1*2)!='\0') { break; }
				if (com_hex[0]<=1) menu_locked=com_hex[0];
				print_hexXX(menu_locked);
				break;

			default:
				c='\0';
				break;
		}
		if (c!='\0') COM_putchar('\n');
		COM_flush();
	}
}

#if DEBUG_PRINT_MOTOR
void COM_debug_print_motor(int8_t dir, uint16_t m, uint8_t pwm) {
	if (dir>0) {
		COM_putchar('+');
	} else if (dir<0) {
		COM_putchar('-');
	}
	COM_putchar(' ');
	print_hexXXXX(m);
	COM_putchar(' ');
	print_hexXX(pwm);

	COM_putchar('\n');
	COM_flush();
}
#endif

#if DEBUG_PRINT_MEASURE
void COM_debug_print_temperature(uint16_t t) {
	print_s_p(PSTR("T: "));
	print_decXXXX(t);
	COM_putchar('\n');
	COM_flush();
}
#endif

#if DEBUG_PRINT_ADDITIONAL_TIMESTAMPS
void COM_print_time(uint8_t c) {
	print_decXX((task & TASK_RTC)?RTC_GetSecond()+1:RTC_GetSecond());
	COM_putchar('.');
	print_hexXX(RTC_s256);
	COM_putchar('-');
	COM_putchar(c);
	COM_putchar('\n');
	COM_flush();
}

#endif

#if DEBUG_PRINT_STR_16
void COM_printStr16 (const char * s, uint16_t x) {
	print_s_p(s);
	print_hexXXXX(x);
	COM_putchar('\n');
}
#endif
