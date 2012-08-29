/* coapemeter.c */

#ifndef _COAP_EMETER_
#define 	_COAP_EMETER_

/*----------------------------------------------------------------------------------------------------------------------------------------------*/

/**
 * @file coapemeter.c 
 * @author Stefan Willi <willist@student.ethz.ch>
 * @brief Contains a C implementation of an SML to REST adapter. The UART implementation is done with the contiki rs232 module, whereas the RESTful interface is implemented with the ERBIUM framework
 * Basically, the implementation is an adapter from COAP to SML. On the one hand, the implementation includes a parser to read and write from an UART interface of a smart meter (SML). On the other hand, the implementation includes an erbium engine to invoke the parser 
 * in a RESTful style with COAP over 6LowPAn.  
 */

/*-------------------------------------------------------------headers--------------------------------------------------------------------------*/

#include "coapemeter.h"
#include "smlparser.h"
#include <string.h>

#ifndef DOXYGEN_SHOULD_SKIP_THIS

const static char default_format[13] = {'%','l','d','.','%','0',FIXED_POINT_PRECISION+48,'l','u',' ','%','s','\0'};

static uint32_t alive_counter = 0;

#ifdef _EHZ363ZA_
uint64_t get_oldest_timestamp()
{
    uint64_t min_ts = (uint64_t) -1;
    if(smart_meter_state.product_identification.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.product_identification.last_update);
    }
    if(smart_meter_state.product_single_identification.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.product_single_identification.last_update);
    }
    if(smart_meter_state.public_key.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.public_key.last_update);
    }
    if(smart_meter_state.energy_value_total.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.energy_value_total.last_update);
    }
    if(smart_meter_state.energy_value1.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.energy_value1.last_update);
    }
    if(smart_meter_state.energy_value2.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.energy_value2.last_update);
    }
    if(smart_meter_state.current_active_power.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.current_active_power.last_update);
    }
    if(smart_meter_state.active_power_l1.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.active_power_l1.last_update);
    }
    if(smart_meter_state.active_power_l2.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.active_power_l2.last_update);
    }
    if(smart_meter_state.active_power_l3.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.active_power_l3.last_update);
    }
    if(smart_meter_state.current_chip_temperature.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.current_chip_temperature.last_update);
    }
    if(smart_meter_state.minimal_chip_temperature.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.minimal_chip_temperature.last_update);
    }
    if(smart_meter_state.averaged_chip_temperature.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.averaged_chip_temperature.last_update);
    }
    if(smart_meter_state.voltage_minimum.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.voltage_minimum.last_update);
    }
    if(smart_meter_state.voltage_maximum.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.voltage_maximum.last_update);
    }
    if(smart_meter_state.current_l1.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.current_l1.last_update);
    }
    if(smart_meter_state.current_l2.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.current_l2.last_update);
    }
    if(smart_meter_state.current_l3.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.current_l3.last_update);
    }
    if(smart_meter_state.voltage_l1.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.voltage_l1.last_update);
    }
    if(smart_meter_state.voltage_l2.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.voltage_l2.last_update);
    }
    if(smart_meter_state.voltage_l3.last_update)
    {
        min_ts = min(min_ts, smart_meter_state.voltage_l3.last_update);
    }
    if(min_ts == (uint64_t) -1)
    {
        #if DEBUG
            printf("ETIME");
        #endif
        min_ts = 0;
    }
    return min_ts;
}
#endif

/*-------------------------------------------------------------resource definitions------------------------------------------------------------*/
RESOURCE(meter, METHOD_GET, "meter", RESOURCE_TITLE);
void meter_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[128];
    //char time_data[64];
    memset(data,0,128);
    /*
     m emset(time_data,0,6*4)*;
     ctime(alive_counter*3,time_data,64);
        sprintf(data,"%s %s alive %s", smart_meter_state.product_identification.property_data, smart_meter_state.product_single_identification.property_data, time_data);
        */
    sprintf(data,"%s %s alive since %lu", smart_meter_state.product_identification.property_data, smart_meter_state.product_single_identification.property_data, alive_counter);
    REST.set_response_payload(response, (uint8_t*) data, strlen(data));
}

RESOURCE(meter_id, METHOD_GET, "meter/id", RESOURCE_TITLE);
void meter_id_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[16];
    memset(data,0,16);
    sprintf(data,"%d", SMART_METER_ID);
    REST.set_response_payload(response, (uint8_t*) data, strlen(data));
}

RESOURCE(meter_token, METHOD_GET, "meter/token", RESOURCE_TITLE);
void meter_token_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    sprintf(data,"%s", SMART_METER_TOKEN);
    REST.set_response_payload(response, (uint8_t*) data, strlen(data));
}

RESOURCE(meter_pkey, METHOD_GET, "meter/pkey", RESOURCE_TITLE);
void meter_pkey_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, (uint8_t*) smart_meter_state.public_key.property_key, strlen((char*)smart_meter_state.public_key.property_key));
}

PERIODIC_RESOURCE(meter_timestamp, METHOD_GET, "meter/timestamp", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_timestamp_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    ctime(get_oldest_timestamp(), data, 64);
    REST.set_response_payload(response, (uint8_t*) data, strlen(data));
}
static int32_t meter_timestamp_periodic_margin = 1;
static int64_t meter_timestamp_old_val = 0;
static int32_t meter_timestamp_periodic_i = 0;
int16_t meter_timestamp_periodic_handler(resource_t *r)
{
    int64_t cur_val = get_oldest_timestamp();
    if(reabs(meter_timestamp_old_val - cur_val) > meter_timestamp_periodic_margin)
    {
        meter_timestamp_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        ctime(cur_val, data, 64);
        REST.notify_subscribers(r->url, 1, meter_timestamp_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_total_power, METHOD_GET, "meter/total/power" , RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_total_power_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z = 0;
    uint32_t val_R = 0;
    double_to_2ints(smart_meter_state.current_active_power.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_active_power.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_total_power_periodic_margin = 1;
static double meter_total_power_old_val = 0;
static int32_t meter_total_power_periodic_i = 0;
int16_t meter_total_power_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.current_active_power.property_value;
    if(reabs(meter_total_power_old_val - cur_val) > meter_total_power_periodic_margin)
    {
        meter_total_power_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_active_power.property_unit));
        REST.notify_subscribers(r->url, 1, meter_total_power_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_total_current, METHOD_GET, "meter/total/current" , RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_total_current_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.current_l1.property_value + smart_meter_state.current_l2.property_value + smart_meter_state.current_l3.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l1.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_total_current_periodic_margin = 0.1;
static double meter_total_current_old_val = 0;
static int32_t meter_total_current_periodic_i = 0;
int16_t meter_total_current_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.current_l1.property_value + smart_meter_state.current_l2.property_value + smart_meter_state.current_l3.property_value;
    if(reabs(meter_total_current_old_val - cur_val) > meter_total_current_periodic_margin)
    {
        meter_total_current_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l1.property_unit));
        REST.notify_subscribers(r->url, 1, meter_total_current_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

//TODO
PERIODIC_RESOURCE(meter_total_phase2, METHOD_GET, "meter/total/phase2" , RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_total_phase2_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    
}
static double meter_total_phase2_periodic_margin = 5;
static double meter_total_phase2_old_val = 0;
static int32_t meter_total_phase2_periodic_i = 0;
int16_t meter_total_phase2_periodic_handler(resource_t *r)
{
    return 0;
}

//TODO
PERIODIC_RESOURCE(meter_total_phase3, METHOD_GET, "meter/total/phase3" , RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_total_phase3_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    
}
static double meter_total_phase3_periodic_margin = 5;
static double meter_total_phase3_old_val = 0;
static int32_t meter_total_phase3_periodic_i = 0;
int16_t meter_total_phase3_periodic_handler(resource_t *r)
{
    return 0;
}

PERIODIC_RESOURCE(meter_l1_power, METHOD_GET, "meter/l1/power", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l1_power_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.active_power_l1.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.active_power_l1.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l1_power_periodic_margin = 1;
static double meter_l1_power_old_val = 0;
static int32_t meter_l1_power_periodic_i = 0;
int16_t meter_l1_power_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.active_power_l1.property_value;
    if(reabs(meter_l1_power_old_val - cur_val) > meter_l1_power_periodic_margin)
    {
        meter_l1_power_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.active_power_l1.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l1_power_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l2_power, METHOD_GET, "meter/l2/power", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l2_power_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.active_power_l2.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.active_power_l2.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l2_power_periodic_margin = 1;
static double meter_l2_power_old_val = 0;
static int32_t meter_l2_power_periodic_i = 0;
int16_t meter_l2_power_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.active_power_l2.property_value;
    if(reabs(meter_l2_power_old_val - cur_val) > meter_l2_power_periodic_margin)
    {
        meter_l2_power_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.active_power_l2.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l2_power_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l3_power, METHOD_GET, "meter/l3/power", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l3_power_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.active_power_l3.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.active_power_l3.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l3_power_periodic_margin = 1;
static double meter_l3_power_old_val = 0;
static int32_t meter_l3_power_periodic_i = 0;
int16_t meter_l3_power_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.active_power_l3.property_value;
    if(reabs(meter_l3_power_old_val - cur_val) > meter_l3_power_periodic_margin)
    {
        meter_l3_power_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.active_power_l3.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l3_power_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l1_current, METHOD_GET, "meter/l1/current", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l1_current_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.current_l1.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l1.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l1_current_periodic_margin = 0.1;
static double meter_l1_current_old_val = 0;
static int32_t meter_l1_current_periodic_i = 0;
int16_t meter_l1_current_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.current_l1.property_value;
    if(reabs(meter_l1_current_old_val - cur_val) > meter_l1_current_periodic_margin)
    {
        meter_l1_current_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l1.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l1_current_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l2_current, METHOD_GET, "meter/l2/current", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l2_current_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.current_l2.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l2.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l2_current_periodic_margin = 0.1;
static double meter_l2_current_old_val = 0;
static int32_t meter_l2_current_periodic_i = 0;
int16_t meter_l2_current_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.current_l2.property_value;
    if(reabs(meter_l2_current_old_val - cur_val) > meter_l2_current_periodic_margin)
    {
        meter_l2_current_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l2.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l2_current_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l3_current, METHOD_GET, "meter/l3/current", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l3_current_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.current_l3.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l3.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l3_current_periodic_margin = 0.1;
static double meter_l3_current_old_val = 0;
static int32_t meter_l3_current_periodic_i = 0;
int16_t meter_l3_current_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.current_l3.property_value;
    if(reabs(meter_l3_current_old_val - cur_val) > meter_l3_current_periodic_margin)
    {
        meter_l3_current_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.current_l3.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l3_current_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l1_voltage, METHOD_GET, "meter/l1/voltage", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l1_voltage_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.voltage_l1.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.voltage_l1.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l1_voltage_periodic_margin = 1;
static double meter_l1_voltage_old_val = 0;
static int32_t meter_l1_voltage_periodic_i = 0;
int16_t meter_l1_voltage_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.voltage_l1.property_value;
    if(reabs(meter_l1_voltage_old_val - cur_val) > meter_l1_voltage_periodic_margin)
    {
        meter_l1_voltage_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.voltage_l1.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l1_voltage_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l2_voltage, METHOD_GET, "meter/l2/voltage", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l2_voltage_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.voltage_l2.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.voltage_l2.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l2_voltage_periodic_margin = 1;
static double meter_l2_voltage_old_val = 0;
static int32_t meter_l2_voltage_periodic_i = 0;
int16_t meter_l2_voltage_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.voltage_l2.property_value;
    if(reabs(meter_l2_voltage_old_val - cur_val) > meter_l2_voltage_periodic_margin)
    {
        meter_l2_voltage_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.voltage_l2.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l2_voltage_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l3_voltage, METHOD_GET, "meter/l3/voltage", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l3_voltage_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    char data[64];
    memset(data,0,64);
    int32_t val_Z;
    uint32_t val_R;
    double_to_2ints(smart_meter_state.voltage_l3.property_value, &val_Z, &val_R);
    sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.voltage_l3.property_unit));
    REST.set_response_payload(response,(uint8_t*) data, strlen(data));
}
static double meter_l3_voltage_periodic_margin = 1;
static double meter_l3_voltage_old_val = 0;
static int32_t meter_l3_voltage_periodic_i = 0;
int16_t meter_l3_voltage_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.voltage_l3.property_value;
    if(reabs(meter_l3_voltage_old_val - cur_val) > meter_l3_voltage_periodic_margin)
    {
        meter_l3_voltage_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t val_Z;
        uint32_t val_R;
        double_to_2ints(cur_val, &val_Z, &val_R);
        sprintf(data, default_format, val_Z, val_R, unit_to_string(smart_meter_state.voltage_l3.property_unit));
        REST.notify_subscribers(r->url, 1, meter_l3_voltage_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(meter_l1_phase, METHOD_GET, "meter/l1/phase", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l1_phase_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    
}
static double meter_l1_phase_periodic_margin = 5;
static double meter_l1_phase_old_val = 0;
static int32_t meter_l1_phase_periodic_i = 0;
int16_t meter_l1_phase_periodic_handler(resource_t *r)
{
    return 0;
}

PERIODIC_RESOURCE(meter_l2_phase, METHOD_GET, "meter/l2/phase", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l2_phase_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    
}
static double meter_l2_phase_periodic_margin = 5;
static double meter_l2_phase_old_val = 0;
static int32_t meter_l2_phase_periodic_i = 0;
int16_t meter_l2_phase_periodic_handler(resource_t *r)
{
    return 0;
}

PERIODIC_RESOURCE(meter_l3_phase, METHOD_GET, "meter/l3/phase", RESOURCE_TITLE, PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void meter_l3_phase_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    
}
static double meter_l3_phase_periodic_margin = 5;
static double meter_l3_phase_old_val = 0;
static int32_t meter_l3_phase_periodic_i = 0;
int16_t meter_l3_phase_periodic_handler(resource_t *r)
{
    return 0;
}

/*-------------------------------------------------------------post callback function----------------------------------------------------------*/
void client_chunk_handler(void *response)
{
    #if DEBUG
        uint8_t *chunk;
        printf("CB:\n");
        coap_get_payload(response, &chunk);
        printf("%s\n",(char*)chunk);
    #endif
}

/*-------------------------------------------------------------process definitions-------------------------------------------------------------*/

PROCESS_THREAD(sml_process, ev, data)
{
    static struct etimer sml_etimer;
    PROCESS_BEGIN();
    setup_parser();    
    sml_listen();
    PROCESS_PAUSE();
    
    while(1)
    {
        etimer_set(&sml_etimer, CLOCK_SECOND * SML_PARSER_UPDATE_INTERVAL);
        PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_TIMER);
        parser_send_requests();
        alive_counter += SML_PARSER_UPDATE_INTERVAL;
    }
    PROCESS_END();
}

PROCESS_THREAD(coap_process, ev, data)
{
    static struct etimer coap_etimer;
    
    static uip_ipaddr_t coap_http_proxy_server_ipaddr;
    static coap_packet_t request[1];
    static char json_data[REST_MAX_CHUNK_SIZE];
    static uint32_t length = 0;
    
    static uint8_t block_num = 0;
    
    static int32_t val_Z_pAP = 0;
    static uint32_t val_R_pAP = 0;
    static int32_t val_Z_pL1 = 0;
    static uint32_t val_R_pL1 = 0;
    static int32_t val_Z_pL2 = 0;
    static uint32_t val_R_pL2 = 0;
    static int32_t val_Z_pL3 = 0;
    static uint32_t val_R_pL3 = 0;
    static int32_t val_Z_cN = 0;
    static uint32_t val_R_cN = 0;
    static int32_t val_Z_cL1 = 0;
    static uint32_t val_R_cL1 = 0;
    static int32_t val_Z_cL2 = 0;
    static uint32_t val_R_cL2 = 0;
    static int32_t val_Z_cL3 = 0;
    static uint32_t val_R_cL3 = 0;
    static int32_t val_Z_vL1 = 0;
    static uint32_t val_R_vL1 = 0;
    static int32_t val_Z_vL2 = 0;
    static uint32_t val_R_vL2 = 0;
    static int32_t val_Z_vL3 = 0;
    static uint32_t val_R_vL3 = 0;
    static int32_t val_Z_pAVL2L1 = 0;
    static uint32_t val_R_pAVL2L1 = 0;
    static int32_t val_Z_pAVL3L1 = 0;
    static uint32_t val_R_pAVL3L1 = 0;
    static int32_t val_Z_pACVL1 = 0;
    static uint32_t val_R_pACVL1 = 0;
    static int32_t val_Z_pACVL2 = 0;
    static uint32_t val_R_pACVL2 = 0;
    static int32_t val_Z_pACVL3 = 0;
    static uint32_t val_R_pACVL3 = 0;
    
    static uint64_t val_time = 0;
    
    PROCESS_BEGIN();
    
    //initialize erbium 
    rest_init_engine();
    
    rest_activate_resource(&resource_meter);
    rest_activate_resource(&resource_meter_id);
    rest_activate_resource(&resource_meter_token);
    rest_activate_resource(&resource_meter_pkey);
    rest_activate_resource(&resource_meter_timestamp);
    rest_activate_periodic_resource(&periodic_resource_meter_timestamp);
    rest_activate_resource(&resource_meter_total_power);
    rest_activate_periodic_resource(&periodic_resource_meter_total_power);
    rest_activate_resource(&resource_meter_total_current);
    rest_activate_periodic_resource(&periodic_resource_meter_total_current);
    rest_activate_resource(&resource_meter_total_phase2);
    rest_activate_periodic_resource(&periodic_resource_meter_total_phase2);
    rest_activate_resource(&resource_meter_total_phase3);
    rest_activate_periodic_resource(&periodic_resource_meter_total_phase3);
    rest_activate_resource(&resource_meter_l1_power);
    rest_activate_periodic_resource(&periodic_resource_meter_l1_power);
    rest_activate_resource(&resource_meter_l2_power);
    rest_activate_periodic_resource(&periodic_resource_meter_l2_power);
    rest_activate_resource(&resource_meter_l3_power);
    rest_activate_periodic_resource(&periodic_resource_meter_l3_power);
    rest_activate_resource(&resource_meter_l1_current);
    rest_activate_periodic_resource(&periodic_resource_meter_l1_current);
    rest_activate_resource(&resource_meter_l2_current);
    rest_activate_periodic_resource(&periodic_resource_meter_l2_current);
    rest_activate_resource(&resource_meter_l3_current);
    rest_activate_periodic_resource(&periodic_resource_meter_l3_current);
    rest_activate_resource(&resource_meter_l1_voltage);
    rest_activate_periodic_resource(&periodic_resource_meter_l1_voltage);
    rest_activate_resource(&resource_meter_l2_voltage);
    rest_activate_periodic_resource(&periodic_resource_meter_l2_voltage);
    rest_activate_resource(&resource_meter_l3_voltage);
    rest_activate_periodic_resource(&periodic_resource_meter_l3_voltage);
    rest_activate_resource(&resource_meter_l1_phase);
    rest_activate_periodic_resource(&periodic_resource_meter_l1_phase);
    rest_activate_resource(&resource_meter_l2_phase);
    rest_activate_periodic_resource(&periodic_resource_meter_l2_phase);
    rest_activate_resource(&resource_meter_l3_phase);
    rest_activate_periodic_resource(&periodic_resource_meter_l3_phase);
    
    
    //initialize eMeter POST System
    COAP_HTTP_PROXY_SET_IPV6(&coap_http_proxy_server_ipaddr);
    coap_receiver_init();
    while(1)
    {
        etimer_set(&coap_etimer, CLOCK_SECOND * EMETER_PUSH_INTERVAL);
        PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_TIMER);
        
        // send post to eMeter system
        //char time_str[32];
        
        double_to_2ints(smart_meter_state.current_active_power.property_value, &val_Z_pAP, &val_R_pAP);
        double_to_2ints(smart_meter_state.active_power_l1.property_value, &val_Z_pL1, &val_R_pL1);
        double_to_2ints(smart_meter_state.active_power_l2.property_value, &val_Z_pL2, &val_R_pL2);
        double_to_2ints(smart_meter_state.active_power_l3.property_value, &val_Z_pL3, &val_R_pL3);
        double_to_2ints(smart_meter_state.current_l1.property_value, &val_Z_cL1, &val_R_cL1);
        double_to_2ints(smart_meter_state.current_l2.property_value, &val_Z_cL2, &val_R_cL2);
        double_to_2ints(smart_meter_state.current_l3.property_value, &val_Z_cL3, &val_R_cL3);
        double_to_2ints(smart_meter_state.voltage_l1.property_value, &val_Z_vL1, &val_R_vL1);
        double_to_2ints(smart_meter_state.voltage_l2.property_value, &val_Z_vL2, &val_R_vL2);
        double_to_2ints(smart_meter_state.voltage_l3.property_value, &val_Z_vL3, &val_R_vL3);
        double_to_2ints(smart_meter_state.current_l1.property_value + smart_meter_state.current_l2.property_value + smart_meter_state.current_l3.property_value, &val_Z_cN, &val_R_cN);
	
        //ctime(get_oldest_timestamp(),time_str,32);
        val_time = get_oldest_timestamp();
        
        memset(json_data,' ',REST_MAX_CHUNK_SIZE);
        length = snprintf(json_data, REST_MAX_CHUNK_SIZE,
                          "{"
                          "\"measurement\":"
                          "{"
                          "\"powerAllPhases\":%ld.%lu,"
                          "\"powerL1\":%ld.%lu,"
                          "\"powerL2\":%ld.%lu,"
                          "\"powerL3\":%ld.%lu,"
                          ,val_Z_pAP, val_R_pAP
                          ,val_Z_pL1, val_R_pL1
                          ,val_Z_pL2, val_R_pL2
                          ,val_Z_pL3, val_R_pL3);
        
        if(length >= REST_MAX_CHUNK_SIZE)
        {
            #if DEBUG
                printf("ECOAP");
            #endif
        }
        else
        {
            json_data[length] = ' ';
            coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_content_type(request, APPLICATION_JSON);
            coap_set_header_uri_path(request, "rd");
            coap_set_header_proxy_uri(request, EMETER_SERVER_URL);
            coap_set_header_block1(request, block_num++, 1, REST_MAX_CHUNK_SIZE);
            coap_set_payload(request, (uint8_t *)json_data, REST_MAX_CHUNK_SIZE);
            COAP_BLOCKING_REQUEST(&coap_http_proxy_server_ipaddr, COAP_HTTP_PROXY_SERVER_PORT, request, client_chunk_handler);
        }
        
        memset(json_data,' ',REST_MAX_CHUNK_SIZE);
        length = snprintf(json_data, REST_MAX_CHUNK_SIZE,  
                          "\"currentNeutral\":%ld.%lu,"
                          "\"currentL1\":%ld.%lu,"
                          "\"currentL2\":%ld.%lu,"
                          "\"currentL3\":%ld.%lu,"
                          ,val_Z_cN, val_R_cN
                          ,val_Z_cL1, val_R_cL1
                          ,val_Z_cL2, val_R_cL2
                          ,val_Z_cL3, val_R_cL3);
        if(length >= REST_MAX_CHUNK_SIZE)
        {
            #if DEBUG
                printf("ECOAP");
            #endif
        }
        else
        {
            json_data[length] = ' ';
            coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_content_type(request, APPLICATION_JSON);
            coap_set_header_uri_path(request, "rd");
            coap_set_header_proxy_uri(request, EMETER_SERVER_URL);
            coap_set_payload(request, (uint8_t *)json_data, REST_MAX_CHUNK_SIZE);
            coap_set_header_block1(request, block_num++, 1, REST_MAX_CHUNK_SIZE);
            COAP_BLOCKING_REQUEST(&coap_http_proxy_server_ipaddr, COAP_HTTP_PROXY_SERVER_PORT, request, client_chunk_handler);
        }
        
        memset(json_data,' ',REST_MAX_CHUNK_SIZE);
        length = snprintf(json_data, REST_MAX_CHUNK_SIZE,
                          "\"voltageL1\":%ld.%lu,"
                          "\"voltageL2\":%ld.%lu,"
                          "\"voltageL3\":%ld.%lu,"
                          "\"phaseAngleVoltageL2L1\":%ld.%lu,"
                          ,val_Z_vL1, val_R_vL1
                          ,val_Z_vL2, val_R_vL2
                          ,val_Z_vL3, val_R_vL3
                          ,val_Z_pAVL2L1, val_R_pAVL2L1);
        
        if(length >= REST_MAX_CHUNK_SIZE)
        {
            #if DEBUG
                printf("ECOAP");
            #endif
        }
        else
        {
            json_data[length] = ' ';
            coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_content_type(request, APPLICATION_JSON);
            coap_set_header_uri_path(request, "rd");
            coap_set_header_proxy_uri(request, EMETER_SERVER_URL);
            coap_set_payload(request, (uint8_t *)json_data, REST_MAX_CHUNK_SIZE);
            coap_set_header_block1(request, block_num++, 1, REST_MAX_CHUNK_SIZE);
            COAP_BLOCKING_REQUEST(&coap_http_proxy_server_ipaddr, COAP_HTTP_PROXY_SERVER_PORT, request, client_chunk_handler);
        }
        
        memset(json_data,' ',REST_MAX_CHUNK_SIZE);
        length = snprintf(json_data, REST_MAX_CHUNK_SIZE,
                          "\"phaseAngleVoltageL3L1\":%ld.%lu,"
                          "\"phaseAngleCurrentVoltageL1\":%ld.%lu,"
                          "\"phaseAngleCurrentVoltageL2\":%ld.%lu,"
                          "\"phaseAngleCurrentVoltageL3\":%ld.%lu,"
                          ,val_Z_pAVL3L1, val_R_pAVL3L1
                          ,val_Z_pACVL1, val_R_pACVL1
                          ,val_Z_pACVL2, val_R_pACVL2
                          ,val_Z_pACVL3, val_R_pACVL3);
        if(length >= REST_MAX_CHUNK_SIZE)
        {
            #if DEBUG
                printf("ECOAP");
            #endif
        }
        else
        {
            json_data[length] = ' ';
            coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_content_type(request, APPLICATION_JSON);
            coap_set_header_uri_path(request, "rd");
            coap_set_header_proxy_uri(request, EMETER_SERVER_URL);
            coap_set_payload(request, (uint8_t *)json_data, REST_MAX_CHUNK_SIZE);
            coap_set_header_block1(request, block_num++, 1, REST_MAX_CHUNK_SIZE);
            COAP_BLOCKING_REQUEST(&coap_http_proxy_server_ipaddr, COAP_HTTP_PROXY_SERVER_PORT, request, client_chunk_handler);
        }
        
        memset(json_data,0,REST_MAX_CHUNK_SIZE);
        if(val_time >> 32)
        {
            length = snprintf(json_data, REST_MAX_CHUNK_SIZE,
                              "\"createdOn\":%ld%lu,"
                              ,(int32_t) (val_time >> 32),(uint32_t) (val_time & ((uint32_t) -1)));
        }
        else
        {
            length = snprintf(json_data, REST_MAX_CHUNK_SIZE,
                              "\"createdOn\":%lu,"
                              ,(uint32_t) (val_time & ((uint32_t) -1)));
            
        }
        length += snprintf(json_data+length, REST_MAX_CHUNK_SIZE-length, 
                           "\"smartMeterId\":%d,"
                           "\"smartMeterToken\":%u"
                           "}"
                           "}"
                           ,SMART_METER_ID
                           ,0xFFFF);
        
        if(length >= REST_MAX_CHUNK_SIZE)
        {
            #if DEBUG
                printf("ECOAP");
            #endif
        }
        else
        {
            coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_content_type(request, APPLICATION_JSON);
            coap_set_header_uri_path(request, "rd");
            coap_set_header_proxy_uri(request, EMETER_SERVER_URL);
            coap_set_payload(request, (uint8_t *)json_data, strlen((char*)json_data));
            coap_set_header_block1(request, block_num++, 0, REST_MAX_CHUNK_SIZE);
            COAP_BLOCKING_REQUEST(&coap_http_proxy_server_ipaddr, COAP_HTTP_PROXY_SERVER_PORT, request, client_chunk_handler); 
        }
        block_num = 0;
    }
    PROCESS_END();
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */
#endif /* _COAP_EMETER_ */