#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "interrupttwi.h"


// initialize the Master TWI, uses included parameters from twim.h
void TWI_init(uint32_t scl_clock){
	sei();
	//SCL_CLOCK and transfer rate set in twim.h
	/* initialize TWI clock: TWPS = 0 => prescaler = 1 */
	TWCR = (TWI_ACK);
	TWSR = (0<<TWPS1) | (0<<TWPS0);                  /* no prescaler */
	TWBR = ((F_CPU/scl_clock)-16)/2;  /* must be > 10 for stable operation */
	TWI_busy=0;
 }

// master write to slave
void TWI_master_start_write(uint8_t slave_addr, uint16_t write_bytes) {//7 bit slave address, number of bytes to write
	TWI_busy=1;
	if(write_bytes>TWI_BUFFER_MAX){
        	TWI_write_bytes=TWI_BUFFER_MAX;
    	}
	else{
        	TWI_write_bytes=write_bytes;
	}
	TWI_operation=TWI_OP_WRITE_ONLY;
	TWI_master_state = TWI_WRITE_STATE;
	TWI_target_slave_addr = slave_addr;
	TWCR = TWI_START; // start TWI master mode
}

// master read from slave
void TWI_master_start_read(uint8_t slave_addr, uint16_t read_bytes){
	TWI_busy=1;
	if(read_bytes>TWI_BUFFER_MAX){
        	TWI_read_bytes=TWI_BUFFER_MAX;
    	}
	else{
		TWI_read_bytes=read_bytes;
	}
	TWI_operation=TWI_OP_READ_ONLY;
	TWI_master_state = TWI_READ_STATE;
	TWI_target_slave_addr = slave_addr;
	TWCR = TWI_START; // start TWI master mode
}

// master write then read without releasing buss between
void TWI_master_start_write_then_read(uint8_t slave_addr, uint16_t write_bytes, uint16_t read_bytes){
	TWI_busy=1;
	if(write_bytes>TWI_BUFFER_MAX){
		TWI_write_bytes=TWI_BUFFER_MAX;
	}
	else{
        	TWI_write_bytes=write_bytes;
	}
	if(read_bytes>TWI_BUFFER_MAX){
		TWI_read_bytes=TWI_BUFFER_MAX;
	}
	else{
		TWI_read_bytes=read_bytes;
	}
	TWI_operation=TWI_OP_WRITE_THEN_READ;
	TWI_master_state = TWI_WRITE_STATE;
	TWI_target_slave_addr = slave_addr;
	TWCR = TWI_START; // start TWI master mode 
}

// Routine to service interrupts from the TWI hardware.
// The most important thing is that this routine runs fast and returns control
// to the hardware asap. 
// See pages 229, 232, 235, and 238 of the ATmega328 datasheed for detailed 
// explaination of the logic below.
ISR(TWI_vect){

        TWI_status = TWSR & TWI_TWSR_status_mask;
    switch(TWI_status){
        case TWI_repeated_start_sent:
        case TWI_start_sent:
		switch(TWI_master_state){
			case TWI_WRITE_STATE:
				TWI_buffer_pos=0; // point to 1st byte
				TWDR = (TWI_target_slave_addr<<1) | 0x00; // set SLA_W
				break;
			
			case TWI_READ_STATE:
                        	TWI_buffer_pos=0; // point to first byte
				TWDR = (TWI_target_slave_addr<<1) | 0x01; // set SLA_R
				break;
        	}
	        TWCR = TWI_ACK; // transmit
		break;

	case TWI_SLA_W_sent_ack_received:   
        case TWI_data_sent_ack_received:
		if(TWI_buffer_pos==TWI_write_bytes){
                	if(TWI_operation==TWI_OP_WRITE_THEN_READ){
                    		TWI_master_state=TWI_READ_STATE; // now read from slave
                    		TWCR = TWI_START; // transmit repeated start
                	}
			else{
				TWCR = TWI_STOP; // release the buss
				// while(TWCR & (1<<TWSTO)); // wait for it*** do not use if continuing to have master run is priority over comms
				TWI_busy=0;
	                }
        	}
		else{ 
			TWDR = TWI_buffer_out[TWI_buffer_pos++]; // load data
			TWCR = TWI_ENABLE; // transmit
		}
		break;

        case TWI_data_received_ack_returned:
        	TWI_buffer_in[TWI_buffer_pos++]=TWDR; // save byte

        case TWI_SLA_R_sent_ack_received: 
        	if(TWI_buffer_pos==(TWI_read_bytes-1)){
                	TWCR = TWI_NACK; // get last byte then nack
            	}
		else{
                	TWCR = TWI_ACK; // get next byte then ack
            	}
            	break;

        case TWI_data_received_nack_returned:            
        	TWI_buffer_in[TWI_buffer_pos++]=TWDR; // save byte
        	TWCR = TWI_STOP; // release the buss
        	// while(TWCR & (1<<TWSTO)); // wait for it*** do not use if continuing to have master run is priority over comms
        	TWI_busy=0;
        	break;

        case TWI_data_sent_nack_received:
        case TWI_SLA_R_sent_nack_received:
        case TWI_arbitration_lost:
        default:
        	TWCR=TWI_STOP; 
        	//   while(TWCR & (1<<TWSTO)); // wait for it*** do not use if continuing to have master run is priority over comms
        	TWCR=TWI_START; // try again
        	break;
    }

}
