/*
 *  Copyright (c) 2008  Swedish Institute of Computer Science
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \file
 *
 * \brief
 *      Functions to control the ADC of the MCU. This is used to read the
 *      joystick.
 *
 * \author
 *      Mike Vidales mavida404@gmail.com
 *
 */

#include "adc.h"



static bool adc_initialized;
static bool adc_conversion_started;

/*---------------------------------------------------------------------------*/

/**
 *   \brief This function will init the ADC with the following parameters.
 *
 *   \param chan Determines the ADC channel to open.
 *   \param muc5 Sets mux5 bit
 *   \param trig Sets what type of trigger is needed.
 *   \param ref Sets the proper reference voltage.
 *   \param prescale Sets the prescale to be used against the XTAL choice.
 *
 *   \return 0
*/
int
adc_init(adc_chan_t chan, adc_mux5 mux5,adc_trig_t trig, adc_ref_t ref, adc_ps_t prescale, adc_adj_t adjust)
{
	/* Enable ADC module */
	PRR0 &= ~(1 << PRADC);

	/* Configure */
	ADCSRA = (1<<ADEN)|prescale|(0<<ADATE)|(1<<ADIE);
	ADCSRB = trig|(uint8_t)mux5;
	ADMUX = (uint8_t)ref | (uint8_t)chan | (uint8_t) adjust;
	ADCSRC = ((1<<ADTHT1)|(1<<ADTHT0)|(0<<ADSUT4)|(0<<ADSUT3)|(0<<ADSUT2)|(1<<ADSUT1)|(1<<ADSUT0));
	
	adc_initialized = true;
	adc_conversion_started = false;

	return 0;

}

/*---------------------------------------------------------------------------*/

/**
 *   \brief This will disable the adc.
*/
void
adc_deinit(void)
{
    /* Disable ADC */
    ADCSRA &= ~(1<<ADEN);
    PRR0 |= (1 << PRADC);

    adc_initialized = false;
    adc_conversion_started = false;
}

/*---------------------------------------------------------------------------*/

/**
 *   \brief This will start an ADC conversion
 *
 *   \return 0
*/
int
adc_conversion_start(void)
{
    if (adc_initialized == false){
        return EOF;
    }
    adc_conversion_started = true;
    ADCSRA |= (1<<ADSC);
    return 0;
}


/*---------------------------------------------------------------------------*/

/**
 *   \brief This will read the ADC result during the ADC conversion and return
 *   the raw ADC conversion result.
 *
 *   \param adjust This will Left or Right Adjust the ADC conversion result.
 *
 *   \return ADC raw 16-byte ADC conversion result.
*/
int16_t
adc_result_get(adc_adj_t adjust)
{
    if (adc_conversion_started == false){
        return EOF;
    }
    if (ADCSRA & (1<<ADSC)){
        return EOF;
    }
    adc_conversion_started = false;
    ADMUX |= (adjust<<ADLAR);
    return (int16_t)ADC;
}


/*---------------------------------------------------------------------------*/

/**
 *   \brief This do an ADC conversion

 *   \param chan Determines the ADC channel to open.
 *   \param muc5 Sets mux5 bit
 *   \param avg Number of measurments
 *
 *   \return average over the measurments
*/
int16_t doAdc(adc_chan_t chan, adc_mux5 mux5, int8_t avg){ // "avg" value here is the number of measurments to average
      int32_t average; // the averaged value (the return value)
     // ADCSRB = ADC_TRIG_FREE_RUN |(uint8_t)mux5;
     // ADMUX = ADC_REF_INT | chan;
      average=0;
      int8_t j;
      set_sleep_mode (SLEEP_MODE_ADC);
      for (j=0;j<avg;j++){
        //ADCSRA |= (1<<ADSC); // Start converting
        sleep_enable();
        sei();
        // Enter Sleep Mode To Trigger ADC Measurement
        // CPU Will Wake Up From ADC Interrupt
        sleep_cpu();
        sleep_disable();
        if (ADCSRA & (1<<ADSC))
        {  return EOF;  }        
        average += (int16_t)ADC;
      }
      average /=avg;
      return (int16_t)average;

}

// ADC Interrupt Is Used To Wake Up CPU From Sleep Mode
EMPTY_INTERRUPT (ADC_vect); 



