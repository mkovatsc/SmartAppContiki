
#include "sys/clock.h"
#include "dev/clock-avr.h"
#include "sys/etimer.h"

#include <avr/io.h>
#include <avr/interrupt.h>

/*
  CLOCK_SECOND is the number of ticks per second.
  It is defined through CONF_CLOCK_SECOND in the contiki-conf.h for each platform.
  The usual AVR defaults are 128 or 125 ticks per second, counting a prescaled CPU clock
  using the 8 bit timer0.
  
  As clock_time_t is an unsigned 16 bit data type, intervals up to 512 or 524 seconds
  can be measured with ~8 millisecond precision. 
  For longer intervals a 32 bit global is incremented every second. 
 
  clock-avr.h contains the specific setup code for each mcu.

*/

/* count is a 16 bit tick counter that wraps every ~10 minutes, returned by clock_time() */
static volatile clock_time_t count;
/* scount is the 8 bit counter that counts ticks modulo CLOCK_SECONDS */
static volatile uint8_t scount;
/* seconds is the number of seconds since startup, returned by clock_seconds() */
volatile unsigned long seconds;
/* sleepseconds is the number of seconds sleeping since startup, available globally */
long sleepseconds;

/* Set RADIOSTATS to monitor radio on time (must also be set in the radio driver) */
#if RF230BB && AVR_WEBSERVER
#define RADIOSTATS 1
#endif

#if RADIOSTATS
static volatile uint8_t rcount;
volatile unsigned long radioontime;
extern uint8_t RF230_receive_on;
#endif

/* Set RADIO_CONF_CALIBRATE_INTERVAL for periodic calibration of the PLL during extended radio on time.
 * The RF230 data sheet suggests every 5 minutes if the temperature is fluctuating.
 * At present the specified interval is ignored, and an 8 bit counter gives 256 second intervals.
 * Actual calibration is done by the driver on the next transmit request.
 */
#if RADIO_CONF_CALIBRATE_INTERVAL
extern volatile uint8_t rf230_calibrate;
static uint8_t calibrate_interval;
#endif

#if 0
/*---------------------------------------------------------------------------*/
/* This routine can be called to add seconds to the clock after a sleep
 * of an integral number of seconds.
 */
void clock_adjust_seconds(uint8_t howmany) {
   seconds += howmany;
   sleepseconds +=howmany;
   count += howmany * CLOCK_SECOND;
#if RADIOSTATS
  if (RF230_receive_on) radioontime += howmany;
#endif
}
#endif

/*---------------------------------------------------------------------------*/
/* This routine can be called to add ticks to the clock after a sleep.
 * Leap ticks or seconds can (rarely) be introduced if the ISR is not blocked.
 */
void clock_adjust_ticks(uint16_t howmany) {
// uint8_t sreg = SREG;cli();
   count  += howmany;
   howmany+= scount;
   while(howmany >= CLOCK_SECOND) {
      howmany -= CLOCK_SECOND;
      seconds++;
      sleepseconds++;
#if RADIOSTATS
      if (RF230_receive_on) radioontime += 1;
#endif
   }
   scount = howmany;
// SREG=sreg;
}
/*---------------------------------------------------------------------------*/
//SIGNAL(SIG_OUTPUT_COMPARE0)
ISR(AVR_OUTPUT_COMPARE_INT)
{
  count++;
  if(++scount >= CLOCK_SECOND) {
    scount = 0;
    seconds++;
  }
#if RADIO_CONF_CALIBRATE_INTERVAL
   if (++calibrate_interval==0) {
    rf230_calibrate=1;
  }
#endif
#if RADIOSTATS
  if (RF230_receive_on) {
    if (++rcount >= CLOCK_SECOND) {
      rcount=0;
      radioontime++;
    }
  }
#endif

#if 1
/*  gcc will save all registers on the stack if an external routine is called */
  if(etimer_pending()) {
    etimer_request_poll();
  }
#else
/* doing this locally saves 9 pushes and 9 pops, but these etimer.c and process.c variables have to lose the static qualifier */
  extern struct etimer *timerlist;
  extern volatile unsigned char poll_requested;

#define PROCESS_STATE_NONE        0
#define PROCESS_STATE_RUNNING     1
#define PROCESS_STATE_CALLED      2

  if (timerlist) {
    if(etimer_process.state == PROCESS_STATE_RUNNING ||
       etimer_process.state == PROCESS_STATE_CALLED) {
      etimer_process.needspoll = 1;
      poll_requested = 1;
    }
  }
#endif
}

/*---------------------------------------------------------------------------*/
void
clock_init(void)
{
  cli ();
  OCRSetup();
//scount = count = 0;
  sei ();
}

/*---------------------------------------------------------------------------*/
clock_time_t
clock_time(void)
{
  clock_time_t tmp;
  do {
    tmp = count;
  } while(tmp != count);
  return tmp;
}
#if 0
/*---------------------------------------------------------------------------*/
/**
 * Delay the CPU for a multiple of TODO
 */
void
clock_delay(unsigned int i)
{
  for (; i > 0; i--) {		/* Needs fixing XXX */
    unsigned j;
    for (j = 50; j > 0; j--)
      asm volatile("nop");
  }
}

/*---------------------------------------------------------------------------*/
/**
 * Wait for a number of clock ticks.
 *
 */
void
clock_wait(int i)
{
  clock_time_t start;

  start = clock_time();
  while(clock_time() - start < (clock_time_t)i);
}
/*---------------------------------------------------------------------------*/
void
clock_set_seconds(unsigned long sec)
{
    seconds = sec;
}
#endif

unsigned long
clock_seconds(void)
{
  unsigned long tmp;
  do {
    tmp = seconds;
  } while(tmp != seconds);
  return tmp;
}
#ifdef HANDLE_UNSUPPORTED_INTERRUPTS
/* Ignore unsupported interrupts, optionally hang for debugging */
ISR(BADISR_vect) {
//static volatile uint8_t x;while (1) x++;
}
#endif
#ifdef HANG_ON_UNKNOWN_INTERRUPT
/* Hang on any unsupported interrupt */
/* Useful for diagnosing unknown interrupts that reset the mcu.
 * Currently set up for 12mega128rfa1.
 * For other mcus, enable all and then disable the conflicts.
 */
static volatile uint8_t x;
ISR( _VECTOR(0)) {while (1) x++;}
ISR( _VECTOR(1)) {while (1) x++;}
ISR( _VECTOR(2)) {while (1) x++;}
ISR( _VECTOR(3)) {while (1) x++;}
ISR( _VECTOR(4)) {while (1) x++;}
ISR( _VECTOR(5)) {while (1) x++;}
ISR( _VECTOR(6)) {while (1) x++;}
ISR( _VECTOR(7)) {while (1) x++;}
ISR( _VECTOR(8)) {while (1) x++;}
ISR( _VECTOR(9)) {while (1) x++;}
ISR( _VECTOR(10)) {while (1) x++;}
ISR( _VECTOR(11)) {while (1) x++;}
ISR( _VECTOR(12)) {while (1) x++;}
ISR( _VECTOR(13)) {while (1) x++;}
ISR( _VECTOR(14)) {while (1) x++;}
ISR( _VECTOR(15)) {while (1) x++;}
ISR( _VECTOR(16)) {while (1) x++;}
ISR( _VECTOR(17)) {while (1) x++;}
ISR( _VECTOR(18)) {while (1) x++;}
ISR( _VECTOR(19)) {while (1) x++;}
//ISR( _VECTOR(20)) {while (1) x++;}
//ISR( _VECTOR(21)) {while (1) x++;}
ISR( _VECTOR(22)) {while (1) x++;}
ISR( _VECTOR(23)) {while (1) x++;}
ISR( _VECTOR(24)) {while (1) x++;}
//ISR( _VECTOR(25)) {while (1) x++;}
ISR( _VECTOR(26)) {while (1) x++;}
//ISR( _VECTOR(27)) {while (1) x++;}
ISR( _VECTOR(28)) {while (1) x++;}
ISR( _VECTOR(29)) {while (1) x++;}
ISR( _VECTOR(30)) {while (1) x++;}
ISR( _VECTOR(31)) {while (1) x++;}
//ISR( _VECTOR(32)) {while (1) x++;}
ISR( _VECTOR(33)) {while (1) x++;}
ISR( _VECTOR(34)) {while (1) x++;}
ISR( _VECTOR(35)) {while (1) x++;}
//ISR( _VECTOR(36)) {while (1) x++;}
ISR( _VECTOR(37)) {while (1) x++;}
//ISR( _VECTOR(38)) {while (1) x++;}
ISR( _VECTOR(39)) {while (1) x++;}
ISR( _VECTOR(40)) {while (1) x++;}
ISR( _VECTOR(41)) {while (1) x++;}
ISR( _VECTOR(42)) {while (1) x++;}
ISR( _VECTOR(43)) {while (1) x++;}
ISR( _VECTOR(44)) {while (1) x++;}
ISR( _VECTOR(45)) {while (1) x++;}
ISR( _VECTOR(46)) {while (1) x++;}
ISR( _VECTOR(47)) {while (1) x++;}
ISR( _VECTOR(48)) {while (1) x++;}
ISR( _VECTOR(49)) {while (1) x++;}
ISR( _VECTOR(50)) {while (1) x++;}
ISR( _VECTOR(51)) {while (1) x++;}
ISR( _VECTOR(52)) {while (1) x++;}
ISR( _VECTOR(53)) {while (1) x++;}
ISR( _VECTOR(54)) {while (1) x++;}
ISR( _VECTOR(55)) {while (1) x++;}
ISR( _VECTOR(56)) {while (1) x++;}
//ISR( _VECTOR(57)) {while (1) x++;}
//ISR( _VECTOR(58)) {while (1) x++;}
//ISR( _VECTOR(59)) {while (1) x++;}
//ISR( _VECTOR(60)) {while (1) x++;}
ISR( _VECTOR(61)) {while (1) x++;}
ISR( _VECTOR(62)) {while (1) x++;}
ISR( _VECTOR(63)) {while (1) x++;}
ISR( _VECTOR(64)) {while (1) x++;}
ISR( _VECTOR(65)) {while (1) x++;}
ISR( _VECTOR(66)) {while (1) x++;}
ISR( _VECTOR(67)) {while (1) x++;}
ISR( _VECTOR(68)) {while (1) x++;}
ISR( _VECTOR(69)) {while (1) x++;}
ISR( _VECTOR(70)) {while (1) x++;}
ISR( _VECTOR(71)) {while (1) x++;}
ISR( _VECTOR(72)) {while (1) x++;}
ISR( _VECTOR(73)) {while (1) x++;}
ISR( _VECTOR(74)) {while (1) x++;}
ISR( _VECTOR(75)) {while (1) x++;}
ISR( _VECTOR(76)) {while (1) x++;}
ISR( _VECTOR(77)) {while (1) x++;}
ISR( _VECTOR(78)) {while (1) x++;}
ISR( _VECTOR(79)) {while (1) x++;}
#endif