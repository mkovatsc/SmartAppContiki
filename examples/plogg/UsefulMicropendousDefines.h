/*
  Copyright 2010-12 by Opendous Inc.  www.Micropendous.org

  Defines useful for controlling Micropendous boards.

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#ifndef __USEFUL_MICROPENDOUS_DEFINES__
#define __USEFUL_MICROPENDOUS_DEFINES__

// Defines related to the USB Signal and Power Switches
// which are under control of Pin PE7
#if (BOARD == BOARD_MICROPENDOUS)
        #ifndef SELECT_USB_B
                #define SELECT_USB_B            PORTE |= (1 << PE7); DDRE |= (1 << PE7);
        #endif

        #ifndef SELECT_USB_A
                #define SELECT_USB_A            PORTE &= ~(1 << PE7); DDRE |= (1 << PE7);
        #endif
#else
        #ifndef SELECT_USB_B
                #define SELECT_USB_B            __asm__ volatile ("NOP" ::)
        #endif

        #ifndef SELECT_USB_A
                #define SELECT_USB_A            __asm__ volatile ("NOP" ::)
        #endif
#endif


// Defines related to the TXB0108 Bidirectional Voltage Translator
// which is connected to PortB and its nOE pin is controlled by PE6
#if (BOARD == BOARD_MICROPENDOUS)
        #ifndef ENABLE_VOLTAGE_TXRX
                #define ENABLE_VOLTAGE_TXRX     PORTE |= (1 << PE6); DDRE |= (1 << PE6);
        #endif

        #ifndef DISABLE_VOLTAGE_TXRX
                #define DISABLE_VOLTAGE_TXRX    PORTE &= ~(1 << PE6); DDRE |= (1 << PE6);
        #endif
#else
        #ifndef ENABLE_VOLTAGE_TXRX
                #define ENABLE_VOLTAGE_TXRX     __asm__ volatile ("NOP" ::)
        #endif

        #ifndef DISABLE_VOLTAGE_TXRX
                #define DISABLE_VOLTAGE_TXRX    __asm__ volatile ("NOP" ::)
        #endif
#endif

// Defines related to external SRAM
// On Micropendous boards nCE is PE4 and Address Bit 17 is PE5
// On Micropendous3/Micropendous4 (USER) boards nCE is PE6 and Address Bit 17 is PE7
#if (defined(__AVR_AT90USB1287__) || defined(__AVR_AT90USB647__) ||  \
                        defined(__AVR_AT90USB1286__) || defined(__AVR_AT90USB646__) ||  \
                        defined(__AVR_ATmega32U6__))
        #if (BOARD == BOARD_MICROPENDOUS)
                #ifndef PORTE_EXT_SRAM_SETUP
                        #define PORTE_EXT_SRAM_SETUP    DDRE = 0x37; PORTE = 0x17;
                #endif
                #ifndef ENABLE_EXT_SRAM
                        #define ENABLE_EXT_SRAM         PORTE &= ~(1 << PE4); DDRE |= (1 << PE4);
                #endif
                #ifndef DISABLE_EXT_SRAM
                        #define DISABLE_EXT_SRAM        PORTE |= (1 << PE4); DDRE |= (1 << PE4);
                #endif
                #ifndef SELECT_EXT_SRAM_BANK0
                        #define SELECT_EXT_SRAM_BANK0   PORTE &= ~(1 << PE5); DDRE |= (1 << PE5);
                #endif
                #ifndef SELECT_EXT_SRAM_BANK1
                        #define SELECT_EXT_SRAM_BANK1   PORTE |= (1 << PE5); DDRE |= (1 << PE5);
                #endif
                #ifndef CURRENT_SRAM_BANK
                        #define CURRENT_SRAM_BANK               ((PINE >> 5) & (0x01))
                #endif
        #elif (BOARD == BOARD_USER)
                #ifndef PORTE_EXT_SRAM_SETUP
                        #define PORTE_EXT_SRAM_SETUP    DDRE = 0xC7; PORTE = 0x47;
                #endif
                #ifndef ENABLE_EXT_SRAM
                        #define ENABLE_EXT_SRAM         PORTE &= ~(1 << PE6); DDRE |= (1 << PE6);
                #endif
                #ifndef DISABLE_EXT_SRAM
                        #define DISABLE_EXT_SRAM        PORTE |= (1 << PE6); DDRE |= (1 << PE6);
                #endif
                #ifndef SELECT_EXT_SRAM_BANK0
                        #define SELECT_EXT_SRAM_BANK0   PORTE &= ~(1 << PE7); DDRE |= (1 << PE7);
                #endif
                #ifndef SELECT_EXT_SRAM_BANK1
                        #define SELECT_EXT_SRAM_BANK1   PORTE |= (1 << PE7); DDRE |= (1 << PE7);
                #endif
                #ifndef CURRENT_SRAM_BANK
                        #define CURRENT_SRAM_BANK               ((PINE >> 7) & (0x01))
                #endif
        #endif
#else
        #ifndef PORTE_EXT_SRAM_SETUP
                #define PORTE_EXT_SRAM_SETUP    __asm__ volatile ("NOP" ::)
        #endif
        #ifndef ENABLE_EXT_SRAM
                #define ENABLE_EXT_SRAM         __asm__ volatile ("NOP" ::)
        #endif
        #ifndef DISABLE_EXT_SRAM
                #define DISABLE_EXT_SRAM        __asm__ volatile ("NOP" ::)
        #endif
        #ifndef SELECT_EXT_SRAM_BANK0
                #define SELECT_EXT_SRAM_BANK0   __asm__ volatile ("NOP" ::)
        #endif
        #ifndef SELECT_EXT_SRAM_BANK1
                #define SELECT_EXT_SRAM_BANK1   __asm__ volatile ("NOP" ::)
        #endif
        #ifndef CURRENT_SRAM_BANK
                #define CURRENT_SRAM_BANK               __asm__ volatile ("NOP" ::)
        #endif
#endif


#if (defined(__AVR_AT90USB1287__) || defined(__AVR_AT90USB1286__))
        #ifndef EXT_SRAM_START
                #define EXT_SRAM_START          0x2100
        #endif
#elif (defined(__AVR_AT90USB647__) || defined(__AVR_AT90USB646__))
        #ifndef EXT_SRAM_START
                #define EXT_SRAM_START          0x1100
        #endif
#endif


#ifndef EXT_SRAM_END
        #define EXT_SRAM_END                    0xFFFF
#endif
#ifndef EXT_SRAM_SIZE
        #define EXT_SRAM_SIZE                   (EXT_SRAM_END - EXT_SRAM_START)
#endif



#ifndef PINHIGH
        #define PINHIGH(PORT, PIN)              PORT |= (1 << PIN);
#endif

#ifndef PINLOW
        #define PINLOW(PORT, PIN)               PORT &= ~(1 << PIN);
#endif


#endif // __USEFUL_MICROPENDOUS_DEFINES__

