
// IPSO Resource Interface
RESOURCE(msg, METHOD_GET, "msg","title=\"eMETER-COAP System: Messages\";rt=\"ipso:msg\"");
void msg_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    
}

RESOURCE(msg_status, METHOD_GET, "msg/status", "title=\"eMETER-COAP System: Statusmessages\";rt=\"ipso:msg-status\"");
void msg_status_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    
    #if DEBUG
        REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
        char *data= NULL;
        parser_create_plain_history(&data);
        REST.set_response_payload(response, (uint8_t*) data, strlen(data));
    #else
        REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
        char data[32];
        memset(data,0,32);
        sprintf(data, "SCA alive since %ld",alive_counter);
        REST.set_response_payload(response,(uint8_t*)data, strlen(data));
    #endif
}

// /dev
RESOURCE(dev, METHOD_GET, "dev", "title=\"eMETER-COAP System: Device\";rt=\"ipso:dev\"");
void dev_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    
}

RESOURCE(dev_mfg, METHOD_GET, "dev/mfg", "title=\"eMETER-COAP System: Devicemanufacturer\";rt=\"ipso:dev-mfg\"");
void dev_mfg_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        char data[64];
        memset(data,0,64);
        int32_t index = strlen((const char*)smart_meter_state.product_identification.property_data);
        memcpy(data, smart_meter_state.product_identification.property_data, index);
        data[index++] = ' ';
        ctime(smart_meter_state.product_identification.last_update, data+index, 64-index);
        REST.set_response_payload(response, (uint8_t*) data, strlen(data));
    #endif
}

RESOURCE(dev_mdl, METHOD_GET, "dev/mdl", "title=\"eMETER-COAP System: Devicemodel\"rt=\"ipso:dev-mdl\"");
void dev_mdl_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        char data[64];
        memset(data,0,64);
        int32_t index = strlen((const char*)smart_meter_state.product_single_identification.property_data);
        memcpy(data, smart_meter_state.product_single_identification.property_data, index);
        data[index++] = ' ';
        //ctime(smart_meter_state.product_single_identification.last_update, data+index, 64-index);
        REST.set_response_payload(response, (uint8_t*) data, strlen(data));
    #endif
}

RESOURCE(dev_pkey, METHOD_GET, "dev/pkey", "title=\"eMETER-COAP System: Public Key\"rt=\"ipso:dev\"");
void dev_pkey_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        char data[64];
        memset(data,0,64);
        int32_t index = strlen((const char*) smart_meter_state.public_key.property_key);
        memcpy(data,smart_meter_state.public_key.property_key, index);
        data[index++] = ' ';
        //ctime(smart_meter_state.public_key.last_update, data+index, 64-index);
        REST.set_response_payload(response, (uint8_t*) data, strlen(data));
    #endif
}

// /pwr
RESOURCE(pwr, METHOD_GET, "pwr", "title=\"eMETER-COAP System: Power\";rt=\"ipso:pwr\"");
void pwr_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    
}

PERIODIC_RESOURCE(pwr_phases, METHOD_GET, "pwr/phases/w", "title=\"eMETER-COAP System: Power\";rt=\"ipso:pwr-w\"", PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void pwr_phases_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        int32_t val_Z = smart_meter_state.current_active_power.property_Z_value;
        uint32_t val_R = smart_meter_state.current_active_power.property_R_value;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data, "%ld.%lu %s ", val_Z, val_R, unit_to_string(smart_meter_state.current_active_power.property_unit));
        ctime(smart_meter_state.current_active_power.last_update, data+index, 64-index);
        REST.set_response_payload(response,(uint8_t*) data, strlen(data));
    #endif
}
static double pwr_phases_periodic_margin = 5;
static double pwr_phases_old_val = 0;
static int32_t pwr_phases_periodic_i = 0;
int16_t pwr_phases_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.current_active_power.property_value;
    if(d_abs(pwr_phases_old_val - cur_val) > pwr_phases_periodic_margin)
    {
        pwr_phases_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data,"%ld.%lu %s ", smart_meter_state.current_active_power.property_Z_value, smart_meter_state.current_active_power.property_R_value, unit_to_string(smart_meter_state.current_active_power.property_unit));
        ctime(smart_meter_state.current_active_power.last_update, data+index, 64-index);
        REST.notify_subscribers(r->url, 1, pwr_phases_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(pwr_phase1, METHOD_GET, "pwr/phase1/w", "title=\"eMETER-COAP System: Power Phase 1\";rt=\"ipso:pwr-w\"",PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void pwr_phase1_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        int32_t val_Z = smart_meter_state.active_power_l1.property_Z_value;
        uint32_t val_R = smart_meter_state.active_power_l1.property_R_value;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data, "%ld.%lu %s ", val_Z, val_R, unit_to_string(smart_meter_state.active_power_l1.property_unit));
        ctime(smart_meter_state.active_power_l1.last_update, data+index, 64-index);
        REST.set_response_payload(response,(uint8_t*) data, strlen(data));
    #endif
}
static double pwr_phase1_periodic_margin = 5;
static double pwr_phase1_old_val = 0;
static int32_t pwr_phase1_periodic_i = 0;
int16_t pwr_phase1_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.active_power_l1.property_value;
    if(d_abs(pwr_phase1_old_val - cur_val) > pwr_phase1_periodic_margin)
    {
        pwr_phase1_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data,"%ld.%lu %s ", smart_meter_state.active_power_l1.property_Z_value, smart_meter_state.active_power_l1.property_R_value, unit_to_string(smart_meter_state.active_power_l1.property_unit));
        ctime(smart_meter_state.active_power_l1.last_update, data+index, 64-index);
        REST.notify_subscribers(r->url, 1, pwr_phase1_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(pwr_phase2, METHOD_GET, "pwr/phase2/w", "title=\"eMETER-COAP System: Power Phase 2\";rt=\"ipso:pwr-w\"", PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void pwr_phase2_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        int32_t val_Z = smart_meter_state.active_power_l2.property_Z_value;
        uint32_t val_R = smart_meter_state.active_power_l2.property_R_value;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data, "%ld.%lu %s ", val_Z, val_R, unit_to_string(smart_meter_state.active_power_l2.property_unit));
        ctime(smart_meter_state.active_power_l2.last_update, data+index, 64-index);
        REST.set_response_payload(response,(uint8_t*) data, strlen(data));
    #endif
}
static double pwr_phase2_periodic_margin = 5;
static double pwr_phase2_old_val = 0;
static int32_t pwr_phase2_periodic_i = 0;
int16_t pwr_phase2_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.active_power_l2.property_value;
    if(d_abs(pwr_phase2_old_val - cur_val) > pwr_phase2_periodic_margin)
    {
        pwr_phase2_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data,"%ld.%lu %s ", smart_meter_state.active_power_l2.property_Z_value, smart_meter_state.active_power_l2.property_R_value, unit_to_string(smart_meter_state.active_power_l2.property_unit));
        ctime(smart_meter_state.active_power_l2.last_update, data+index, 64-index);
        REST.notify_subscribers(r->url, 1, pwr_phase2_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

PERIODIC_RESOURCE(pwr_phase3, METHOD_GET, "pwr/phase3/w", "title=\"eMETER-COAP System: Power Phase 3\";rt=\"ipso:pwr-w\"", PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void pwr_phase3_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        int32_t val_Z = smart_meter_state.active_power_l3.property_Z_value;
        uint32_t val_R = smart_meter_state.active_power_l3.property_R_value;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data, "%ld.%lu %s ", val_Z, val_R, unit_to_string(smart_meter_state.active_power_l3.property_unit));
        ctime(smart_meter_state.active_power_l3.last_update, data+index, 64-index);
        REST.set_response_payload(response,(uint8_t*) data, strlen(data));
    #endif
}
static double pwr_phase3_periodic_margin = 5;
static double pwr_phase3_old_val = 0;
static int32_t pwr_phase3_periodic_i = 0;
int16_t pwr_phase3_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.active_power_l3.property_value;
    if(d_abs(pwr_phase3_old_val - cur_val) > pwr_phase3_periodic_margin)
    {
        pwr_phase3_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data,"%ld.%lu %s ", smart_meter_state.active_power_l3.property_Z_value, smart_meter_state.active_power_l3.property_R_value, unit_to_string(smart_meter_state.active_power_l3.property_unit));
        ctime(smart_meter_state.active_power_l3.last_update, data+index, 64-index);
        REST.notify_subscribers(r->url, 1, pwr_phase3_periodic_i++, (uint8_t *)data, strlen(data));   
    }
    return 0;
}

// /nrg
PERIODIC_RESOURCE(pwr_nrgvalues, METHOD_GET, "pwr/nrgvalues/kwh", "title=\"eMETER-COAP System: Energy\";rt=\"ipso:pwr-kwh\"", PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void pwr_nrgvalues_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        int32_t val_Z = smart_meter_state.energy_value_total.property_Z_value;
        uint32_t val_R = smart_meter_state.energy_value_total.property_R_value;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data, "%ld.%lu %s ", val_Z, val_R, unit_to_string(smart_meter_state.energy_value_total.property_unit));
        ctime(smart_meter_state.energy_value_total.last_update, data+index, 64-index);
        REST.set_response_payload(response,(uint8_t*) data, strlen(data));
    #endif
}
static double pwr_nrgvalues_periodic_margin = 0.0001;
static double pwr_nrgvalues_old_val = 0;
static int32_t pwr_nrgvalues_periodic_i = 0;
int16_t pwr_nrgvalues_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.energy_value_total.property_value;
    if(d_abs(pwr_nrgvalues_old_val - cur_val) > pwr_nrgvalues_periodic_margin)
    {
        pwr_nrgvalues_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data,"%ld.%lu %s ", smart_meter_state.energy_value_total.property_Z_value, smart_meter_state.energy_value_total.property_R_value, unit_to_string(smart_meter_state.energy_value_total.property_unit));
        ctime(smart_meter_state.energy_value_total.last_update, data+index, 64-index);
        REST.notify_subscribers(r->url, 1, pwr_nrgvalues_periodic_i++, (uint8_t *)data, strlen(data));  
    }
    return 0;
}

PERIODIC_RESOURCE(pwr_nrgvalue1, METHOD_GET, "pwr/nrgvalue1/kwh", "title=\"eMETER-COAP System: Energy Scale 1\";rt=\"ipso:pwr-kwh\"", PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void pwr_nrgvalue1_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        int32_t val_Z = smart_meter_state.energy_value1.property_Z_value;
        uint32_t val_R = smart_meter_state.energy_value1.property_R_value;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data, "%ld.%lu %s ", val_Z, val_R, unit_to_string(smart_meter_state.energy_value1.property_unit));
        ctime(smart_meter_state.energy_value1.last_update, data+index, 64-index);
        REST.set_response_payload(response,(uint8_t*) data, strlen(data));
    #endif
}
static double pwr_nrgvalue1_periodic_margin = 0.0001;
static double pwr_nrgvalue1_old_val = 0;
static int32_t pwr_nrgvalue1_periodic_i = 0;
int16_t pwr_nrgvalue1_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.energy_value1.property_value;
    if(d_abs(pwr_nrgvalue1_old_val - cur_val) > pwr_nrgvalue1_periodic_margin)
    {
        pwr_nrgvalue1_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data,"%ld.%lu %s ", smart_meter_state.energy_value1.property_Z_value, smart_meter_state.energy_value1.property_R_value, unit_to_string(smart_meter_state.energy_value1.property_unit));
        ctime(smart_meter_state.energy_value1.last_update, data+index, 64-index);
        REST.notify_subscribers(r->url, 1, pwr_nrgvalue1_periodic_i++, (uint8_t *)data, strlen(data));  
    }
    return 0;
}

PERIODIC_RESOURCE(pwr_nrgvalue2, METHOD_GET, "pwr/nrgvalue2/kwh", "title=\"eMETER-COAP System: Energy Scale 2\";rt=\"ipso:pwr-kwh\"", PERIODIC_RESOURCE_INTERVAL*CLOCK_SECOND);
void pwr_nrgvalue2_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    #ifdef _EHZ363ZA_
        int32_t val_Z = smart_meter_state.energy_value2.property_Z_value;
        uint32_t val_R = smart_meter_state.energy_value2.property_R_value;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data, "%ld.%lu %s ", val_Z, val_R, unit_to_string(smart_meter_state.energy_value2.property_unit));
        ctime(smart_meter_state.energy_value2.last_update, data+index, 64-index);
        REST.set_response_payload(response,(uint8_t*) data, strlen(data));
    #endif
}
static double pwr_nrgvalue2_periodic_margin = 0.0001;
static double pwr_nrgvalue2_old_val = 0;
static int32_t pwr_nrgvalue2_periodic_i = 0;
int16_t pwr_nrgvalue2_periodic_handler(resource_t *r)
{
    double cur_val = smart_meter_state.energy_value2.property_value;
    if(d_abs(pwr_nrgvalue2_old_val - cur_val) > pwr_nrgvalue2_periodic_margin)
    {
        pwr_nrgvalue2_old_val = cur_val;
        char data[64];
        memset(data,0,64);
        int32_t index = sprintf(data,"%ld.%lu %s ", smart_meter_state.energy_value2.property_Z_value, smart_meter_state.energy_value2.property_R_value, unit_to_string(smart_meter_state.energy_value2.property_unit));
        ctime(smart_meter_state.energy_value2.last_update,data+index, 64-index);
        REST.notify_subscribers(r->url, 1, pwr_nrgvalue2_periodic_i++, (uint8_t *)data, strlen(data));  
    }
    return 0;
}


// to init the resources
rest_activate_resource(&resource_msg);
    rest_activate_resource(&resource_dev);
    rest_activate_resource(&resource_pwr);
    
    rest_activate_resource(&resource_msg_status);
    
    rest_activate_resource(&resource_dev_mfg);
    rest_activate_resource(&resource_dev_mdl);
    rest_activate_resource(&resource_dev_pkey);
    
    rest_activate_resource(&resource_pwr_phases);
    rest_activate_periodic_resource(&periodic_resource_pwr_phases);
    rest_activate_resource(&resource_pwr_phase1);
    rest_activate_periodic_resource(&periodic_resource_pwr_phase1);
    rest_activate_resource(&resource_pwr_phase2);
    rest_activate_periodic_resource(&periodic_resource_pwr_phase2);
    rest_activate_resource(&resource_pwr_phase3);
    rest_activate_periodic_resource(&periodic_resource_pwr_phase3);
    
    rest_activate_resource(&resource_pwr_nrgvalues);
    rest_activate_periodic_resource(&periodic_resource_pwr_nrgvalues);
    rest_activate_resource(&resource_pwr_nrgvalue1);
    rest_activate_periodic_resource(&periodic_resource_pwr_nrgvalue1);
    rest_activate_resource(&resource_pwr_nrgvalue2);
    rest_activate_periodic_resource(&periodic_resource_pwr_nrgvalue2);
    
    
// smlparser.c : with a buffer
    
    /* smlparser.c */
#include "smlparser.h"
#include "rs232.h"
#include <stdlib.h>
#include <string.h>

// initial FCS value
#define PPPINITFCS16 0xffff     

// table taken from DIN EN 62056-46
static uint16_t fcstab [256] = { 0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48,
0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c,
0x75b7, 0x643e, 0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 0x2102, 0x308b, 0x0210,
0x1399, 0x6726, 0x76af, 0x4434, 0x55bd, 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef,
0xea66, 0xd8fd, 0xc974, 0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 0xce4c, 0xdfc5,
0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3,
0x263a, 0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 0x6306, 0x728f, 0x4014, 0x519d,
0x2522, 0x34ab, 0x0630, 0x17b9, 0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 0x7387,
0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862,
0x9af9, 0x8b70, 0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 0x0840, 0x19c9, 0x2b52,
0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e,
0xf2a7, 0xc03c, 0xd1b5, 0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 0xb58b, 0xa402,
0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5,
0x4d7c, 0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 0x4a44, 0x5bcd, 0x6956, 0x78df,
0x0c60, 0x1de9, 0x2f72, 0x3efb, 0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 0x5ac5,
0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3,
0x8238, 0x93b1, 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 0xf78f, 0xe606, 0xd49d,
0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

#ifdef _EHZ363ZA_
static static_octet_string smart_meter_password;
#endif

static static_octet_string parser_client_id;

static void (*receiver)(unsigned char *buffer, size_t buffer_len);
static sml_read_info r_info;
static unsigned char buffer[MC_SML_BUFFER_LEN];
static unsigned char request_buffer[MC_SML_BUFFER_LEN];
static int32_t max_read_len = MC_SML_BUFFER_LEN;
static sml_buffer sml_file_buffer;

unit int_to_unit(int32_t val)
{
    switch(val)
    {
        case 27:
            return W;
        case 30:
            return Wh;
        case 35:
            return V;
        case 33:
            return A;
        case 9:
            return C;
        default:
            return UNKNOWN;
    }
}

int64_t octet_string_to_int(unsigned char *data, int32_t length)
{
    int64_t result = 0;
    int32_t i;
    int64_t base = 1;
    for(i=length-1;i>=0;i--)
    {
        int64_t val = (int64_t) data[i];
        result += val * base;
        base *= 256;
    }
    return result;
}

uint16_t sml_crc16_calculate(unsigned char *cp, int32_t len) 
{
    uint16_t fcs = PPPINITFCS16;
    
    while (len--) {
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
    }
    
    fcs ^= 0xffff;
    fcs = ((fcs & 0xff) << 8) | ((fcs & 0xff00) >> 8);
    
    return fcs;
}

int16_t octet_string_to_string(unsigned char *data, int32_t data_len, char *str, int32_t str_len)
{
    memset(str,0,str_len);
    int32_t i,j;
    j = 0;
    for(i=0;i<data_len && j < str_len-2;i++)
    {
        uint16_t val = data[i];
        if (val >= 32 && val <= 126)
        {
            str[j++] = (char) val;
        }
        else
        {
            sprintf(&str[j], "%02X", val);
            j += 2;
        }
    }
    return 0;
}

int16_t octet_string_to_hex_string(unsigned char *data, int32_t data_len, char *str, int32_t str_len)
{
    memset(str,0,str_len);
    int32_t i;
    int32_t j = 0;
    for(i=0;(i < data_len) && j < str_len-2;i++)
    {
        sprintf(&str[j],"%02X",(uint8_t)data[i]);
        j+=2;
    }
    return 0;
}

void update_octet_string_of_property(unsigned_char_property *p, unsigned char *data, int32_t data_length, uint64_t seconds)
{
    octet_string_to_string(data, data_length, (char *)&p->property_data, PROPERTY_DATA_LENGTH);
    p->last_update = seconds;
}

void update_octet_string_of_key_property(key_property *p, unsigned char *data, int32_t data_length, uint64_t seconds)
{
    octet_string_to_hex_string(data, data_length, (char*) &p->property_key, PROPERTY_KEY_LENGTH);
    p->last_update = seconds;
}

#if DEBUG
void update_status_message(char *msg)
{
    update_octet_string_of_property(&smart_meter_state.status_message, (unsigned char *)msg, strlen(msg), 0);
}
#endif

void update_values_of_property(double_property *p, double val, unit u, int64_t seconds)
{
    p->property_value = val;
    p->property_unit = u;
    p->last_update = seconds;
}

int32_t reset_read_state()
{
    memset(buffer,0,MC_SML_BUFFER_LEN);
    r_info.esc = 0;
    r_info.start = 0;
    r_info.end = 0;
    r_info.i = 0;
    r_info.done = 0;
    return 0;
}

int16_t sml_read(unsigned char byte)
{
    if(r_info.i < max_read_len)
    {
        if (!r_info.i) 
        { 
            // read until escaped start sequence;
            if (r_info.esc == 4) 
            {
                if (byte == 0x01) 
                {
                    buffer[r_info.esc + r_info.start++] = byte;
                    if (r_info.start == 4) 
                    {
                        r_info.i = r_info.esc + r_info.start;
                        r_info.esc = 0;
                        r_info.start = 0;
                    }
                }
                else 
                {
                    // no start sequence
                    r_info.esc = 0;
                }
            }
            else 
            {
                if (byte == 0x1b)
                {
                    buffer[r_info.esc++] = byte;
                }
                else
                {
                    // no escape sequence
                    r_info.esc = 0;
                }
            }
        }
        else 
        { 
            // read the message
            buffer[r_info.i] = byte;
            if (r_info.esc == 4) 
            {
                if (r_info.end) 
                {
                    r_info.end++;
                    if (r_info.end == 4) 
                    {
                        r_info.done = 1;
                        r_info.i++;
                    }
                }
                else 
                {
                    if (buffer[r_info.i] == 0x1a) 
                    {
                        r_info.end++;
                    }
                    else 
                    {
                        // dont read other escaped sequences yet
                        r_info.esc = 0;
                    }
                }
            }
            else 
            {
                if (buffer[r_info.i] == 0x1b) 
                {
                    r_info.esc++;
                }
                else 
                {
                    r_info.esc = 0;
                }
            }
            if(!r_info.done)
                r_info.i++;
        }
        if(r_info.done)
        {
            if(r_info.i > 0)
            {
                receiver(buffer,r_info.i);
                reset_read_state();
                return 0;
            }
            reset_read_state();
            return 1;
        }
    }
    else
    {
        #if DEBUG
            printf("ESIZE\n");
        #endif
        reset_read_state();
        return 1;
    }
    return 0;
}

int32_t sml_buf_has_errors(sml_buffer *buf) 
{
    return buf->error != 0;
}

void sml_buf_update_bytes_read(sml_buffer *buf, int32_t bytes) 
{
    buf->cursor += bytes;
}

unsigned char sml_buf_get_current_byte(sml_buffer *buf)
{
    return buf->buffer[buf->cursor];
}

unsigned char *sml_buf_get_current_buf(sml_buffer *buf)
{
    return &(buf->buffer[buf->cursor]);
}

int32_t sml_buf_get_next_type(sml_buffer *buf) 
{
    return (buf->buffer[buf->cursor] & SML_TYPE_FIELD);
}

int32_t sml_buf_optional_is_skipped(sml_buffer *buf) 
{
    if (sml_buf_get_current_byte(buf) == SML_OPTIONAL_SKIPPED) 
    {
        sml_buf_update_bytes_read(buf, 1);
        return 1;
    }
    
    return 0;
}

int32_t sml_buf_get_next_length(sml_buffer *buf) 
{
    int32_t length = 0;
    unsigned char byte = sml_buf_get_current_byte(buf);
    int32_t list = ((byte & SML_TYPE_FIELD) == SML_TYPE_LIST) ? 0 : -1;
    
    for (;buf->cursor < buf->buffer_len;) 
    {
        byte = sml_buf_get_current_byte(buf);
        length <<= 4;
        length |= (byte & SML_LENGTH_FIELD);
        
        if ((byte & SML_ANOTHER_TL) != SML_ANOTHER_TL) 
        {
            break;
        }
        sml_buf_update_bytes_read(buf, 1);
        if(list) 
        {
            list += -1;
        }
    }
    sml_buf_update_bytes_read(buf, 1);
    return length + list;
}

void sml_buf_optional_write(sml_buffer *buf) 
{
    buf->buffer[buf->cursor] = SML_OPTIONAL_SKIPPED;
    buf->cursor++;
}

void sml_buf_set_type_and_length(sml_buffer *buf, uint32_t type, uint32_t l) {
    // set the type
    buf->buffer[buf->cursor] = type;
    
    if (type != SML_TYPE_LIST) 
    {
        l++;
    }
    
    if (l > SML_LENGTH_FIELD) 
    {
        
        // how much shifts are necessary
        int32_t mask_pos = (sizeof(uint32_t) * 2) - 1;
        // the 4 most significant bits of l (1111 0000 0000 ...)
        uint32_t mask = ((uint32_t) 0xF0) << (8 * (sizeof(uint32_t) - 1));
        
        // select the next 4 most significant bits with a bit set until there 
        // is something
        while (!(mask & l)) 
        {
            mask >>= 4;
            mask_pos--;
        }
        
        l += mask_pos; // for every TL-field
        
        if ((0x0F << (4 * (mask_pos + 1))) & l) 
        {
            // for the rare case that the addition of the number of TL-fields
            // result in another TL-field.
            mask_pos++;
            l++;
        }
        
        // copy 4 bits of the number to the buffer
        while (mask > SML_LENGTH_FIELD) 
        {
            buf->buffer[buf->cursor] |= SML_ANOTHER_TL;
            buf->buffer[buf->cursor] |= ((mask & l) >> (4 * mask_pos));
            mask >>= 4;
            mask_pos--;
            buf->cursor++;
        }
    }
    
    buf->buffer[buf->cursor] |= (l & SML_LENGTH_FIELD);
    buf->cursor++; 
}

int32_t sml_number_endian() 
{
    int32_t i = 1;
    char *p = (char *)&i;
    
    if (p[0] == 1)
        return SML_LITTLE_ENDIAN;
    else
        return SML_BIG_ENDIAN;
}

void sml_number_byte_swap(unsigned char *bytes, int32_t bytes_len) 
{
    int32_t i;
    unsigned char ob[bytes_len];
    memcpy(&ob, bytes, bytes_len);
    
    for (i = 0; i < bytes_len; i++) 
    {
        bytes[i] = ob[bytes_len - (i + 1)];
    }
}

void sml_number_write(sml_buffer *buf, void *np, unsigned char type, int32_t size) 
{
    if (np == 0) 
    {
        sml_buf_optional_write(buf);
        return;
    }
    
    sml_buf_set_type_and_length(buf, type, size);
    memcpy(sml_buf_get_current_buf(buf), np, size);
    
    if (!(sml_number_endian() == SML_BIG_ENDIAN)) 
    {
        sml_number_byte_swap(sml_buf_get_current_buf(buf), size);
    }
    
    sml_buf_update_bytes_read(buf, size);
}

void sml_number_parse(sml_buffer *buf, uint8_t type, int32_t max_size, unsigned char *np, int32_t ignore) 
{
    if (sml_buf_optional_is_skipped(buf)) 
    {
        return;
    }
    int32_t l, i;
    unsigned char b;
    short negative_int = 0;
    
    if (sml_buf_get_next_type(buf) != type) 
    {
        buf->error = 1;
        return;
    }
    l = sml_buf_get_next_length(buf);
    
    if (l < 0 || l > max_size) 
    {
        buf->error = 1;
        return;
    }
    
    if(!ignore)
    {
        #if DEBUG
            if(max_size > sizeof(uint64_t))
                printf("EMEM!\n");
        #endif
        memset(np, 0, max_size);
    }
    b = sml_buf_get_current_byte(buf);
    if (type == SML_TYPE_INTEGER && (b & 128))
    {
        negative_int = 1;
    }
    
    int32_t missing_bytes = max_size - l;
    if(!ignore)
    {
        memcpy(&(np[missing_bytes]), sml_buf_get_current_buf(buf), l);
    }
    
    if (negative_int && !ignore) 
    {
        for (i = 0; i < missing_bytes; i++) 
        {
            np[i] = 0xFF;
        }
    }
    
    
    if (!(sml_number_endian() == SML_BIG_ENDIAN) && !ignore) 
    {
        sml_number_byte_swap(np, max_size);
    }
    
    sml_buf_update_bytes_read(buf, l);
}

void sml_number_init(uint64_t number, unsigned char type, int32_t size, unsigned char *np) 
{
    memset(np, 0, size);
    memcpy(np, &number, size);
    return;
}

void sml_octet_string_parse(sml_buffer *buf, unsigned char *data, int32_t *length, int32_t ignore) 
{
    if (sml_buf_optional_is_skipped(buf)) 
    {
        return;
    }
    
    int32_t l;
    if (sml_buf_get_next_type(buf) != SML_TYPE_OCTET_STRING) 
    {
        buf->error = 1;
        return;
    }
    
    l = sml_buf_get_next_length(buf);
    if (l < 0) 
    {
        buf->error = 1;
        return;
    }
    
    if(!ignore)
    {
        if (l > 0) 
        {
            #if DEBUG
                if(l > *length)
                    printf("ELENGTH\n");
            #endif
            memcpy(data, sml_buf_get_current_buf(buf), min(l,*length));
        }
        *length = min(*length,l);
    }
    sml_buf_update_bytes_read(buf, l);
}

unsigned char c2ptoi(unsigned char *bytes)
{
    return (unsigned char) ((uint32_t)bytes[0]*16+(uint32_t)bytes[1]);
}

void sml_octet_string_init_from_hex(unsigned char *str, int32_t len, unsigned char *bytes) 
{
    int32_t i;
    if (len % 2 != 0) 
    {
        return;
    }
    for (i = 0; i < (len / 2); i++) 
    {
        bytes[i] = c2ptoi(&(str[i * 2]));
    }
    return;
}

void sml_octet_string_init_from_int(unsigned char *str, int32_t int_size, unsigned char *bytes)
{
    int32_t i;
    for (i=0; i < int_size;i++) 
    {
        bytes[i] = str[(int_size-1)-i];
    }
    return;
}

int16_t generate_random_ascii(unsigned char *bytes, int32_t length)
{
    int32_t i;
    for(i=0;i<length;i++)
    {
        bytes[i] = rand() % 0x80;
    }
    return 0;
}

void sml_octet_string_generate_uuid(unsigned char *bytes) 
{
    int32_t i;
    for(i = 0; i < 16; i++) {
        bytes[i] = rand() % 0xFF;
    }
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // set version
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // set reserved bits
}


void sml_octet_string_write(sml_buffer *buf, unsigned char *data, int32_t length) 
{
    if (data == 0) 
    {
        sml_buf_optional_write(buf);
        return;
    }
    sml_buf_set_type_and_length(buf, SML_TYPE_OCTET_STRING, (uint32_t) length);
    memcpy(sml_buf_get_current_buf(buf), data, length);
    buf->cursor += length;
}

void sml_time_parse(sml_buffer *buf, uint8_t *tag, uint64_t *data, int32_t ignore)
{
    if (sml_buf_optional_is_skipped(buf)) {
        return;
    }
    
    if (sml_buf_get_next_type(buf) != SML_TYPE_LIST) 
    {
        buf->error = 1;
        goto error;
    }
    
    if (sml_buf_get_next_length(buf) != 2) 
    {
        buf->error = 1;
        goto error;
    }
    
    sml_uint8_t_parse(buf,tag,ignore);
    if (sml_buf_has_errors(buf)) goto error;
    
    sml_uint32_t_parse(buf,(unsigned char *)data,ignore);
    if (sml_buf_has_errors(buf)) goto error;
    
    error:
    return;
}

void sml_status_parse(sml_buffer *buf, uint8_t *type, uint64_t *status_data, int32_t ignore) 
{
    if (sml_buf_optional_is_skipped(buf))
    {
        return;
    }
    
    int32_t max = 1;
    *type = sml_buf_get_next_type(buf);
    unsigned char byte = sml_buf_get_current_byte(buf);
    
    switch (*type) 
    {
        case SML_TYPE_UNSIGNED:
            // get maximal size, if not all bytes are used (example: only 6 bytes for a uint64_t)
            while (max < ((byte & SML_LENGTH_FIELD) - 1)) 
            {
                max <<= 1;
            }
            sml_number_parse(buf, (unsigned char) *type, max, (unsigned char*) status_data, ignore);
            *type |= max;
            break;
        default:
            buf->error = 1;
            break;
    }
}

void sml_value_parse(sml_buffer *buf, uint8_t *type, unsigned char *octet_string_data, int32_t *octet_string_length, uint8_t *boolean_data, uint64_t *int_data, int32_t ignore) 
{
    if (sml_buf_optional_is_skipped(buf)) 
    {
        return;
    }
   
    int32_t max = 1;
    *type = sml_buf_get_next_type(buf);
    unsigned char byte = sml_buf_get_current_byte(buf);
    
    switch (*type) 
    {
        case SML_TYPE_OCTET_STRING:
            sml_octet_string_parse(buf, octet_string_data, octet_string_length,ignore);
            break;
        case SML_TYPE_BOOLEAN:
            sml_uint8_t_parse(buf,boolean_data,ignore);
            break;
        case SML_TYPE_UNSIGNED:    
        case SML_TYPE_INTEGER:
            // get maximal size, if not all bytes are used (example: only 6 bytes for a uint64_t)
            while (max < ((byte & SML_LENGTH_FIELD) - 1)) {
                max <<= 1;
            }
            sml_number_parse(buf, *type, max, (unsigned char*) int_data, ignore);
            *type |= max;
            break;
        default:
            buf->error = 1;
            break;
    }
}

void sml_list_entry_parse(sml_buffer *buf, unsigned char *obj_name, int32_t *obj_name_length, uint8_t *status_type, uint64_t *status_data, uint8_t *time_tag, uint64_t *time_data, 
                          uint8_t *unit_tag, int8_t *scaler, uint8_t *value_type, unsigned char *value_octet_string_data, int32_t *value_octet_string_length, uint8_t *value_boolean_data, uint64_t *value_int_data , int32_t ignore) 
{
    if (sml_buf_get_next_type(buf) != SML_TYPE_LIST) 
    {
        buf->error = 1;
        goto error;
    }
    
    if (sml_buf_get_next_length(buf) != 7) 
    {
        buf->error = 1;
        goto error;
    }
    
    // sml obj name: important
    sml_octet_string_parse(buf,obj_name,obj_name_length,ignore);
    if (sml_buf_has_errors(buf)) goto error;
    
    // sml status: +-important
    sml_status_parse(buf,status_type,status_data,ignore);
    if (sml_buf_has_errors(buf)) goto error;
   
    //sml val time: important
    sml_time_parse(buf,time_tag,time_data,ignore);
    if (sml_buf_has_errors(buf)) goto error;
    
    //sml unit: important
    sml_uint8_t_parse(buf,unit_tag,ignore);
    if (sml_buf_has_errors(buf)) goto error;
    
    //sml scaler: important
    sml_int8_t_parse(buf,(unsigned char*) scaler,ignore);
    if (sml_buf_has_errors(buf)) goto error;
    
    //sml value: important
    sml_value_parse(buf, value_type, value_octet_string_data, value_octet_string_length, value_boolean_data, value_int_data, ignore);
    if (sml_buf_has_errors(buf)) goto error;
    
    // sml signature: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    return;
    error:
    buf->error = 1;
    return;
}

int32_t sml_send(sml_buffer *buf) 
{
    unsigned char start_seq[] = {0x1b, 0x1b, 0x1b, 0x1b, 0x01, 0x01, 0x01, 0x01};
    int32_t end_cursor = buf->cursor;
    
    buf->cursor = 0;
    // add start sequence
    memcpy(sml_buf_get_current_buf(buf), start_seq, 8);
    
    //add padding
    buf->cursor = end_cursor;
    // add padding bytes
    int32_t padding = (buf->cursor % 4) ? (4 - buf->cursor % 4) : 0;
    if (padding) 
    {
        // write zeroed bytes
        memset(sml_buf_get_current_buf(buf), 0, padding);
        buf->cursor += padding;
    }
    
    // begin end sequence
    memcpy(sml_buf_get_current_buf(buf), start_seq, 4);
    buf->cursor += 4;
    *sml_buf_get_current_buf(buf) = 0x1a;
    buf->cursor++;
    
    // add padding info
    buf->buffer[buf->cursor++] = (unsigned char) padding;
    
    // add crc checksum
    uint16_t crc = sml_crc16_calculate(buf->buffer, buf->cursor);
    buf->buffer[buf->cursor++] = (unsigned char) ((crc & 0xFF00) >> 8);
    buf->buffer[buf->cursor++] = (unsigned char) (crc & 0x00FF);
    
    return fwrite(buf->buffer,sizeof(unsigned char),buf->cursor, stdout);
}

void sml_open_request_write(sml_buffer *buf, unsigned char *client_id, unsigned char *req_file_id) 
{
    sml_buf_set_type_and_length(buf, SML_TYPE_LIST, 7);
    
    // codepage: not important
    sml_octet_string_write(buf,NULL,16);
    // client id: important
    sml_octet_string_write(buf,client_id,CLIENT_ID_LENGTH);
    // req file id: important
    sml_octet_string_write(buf,req_file_id,REQUEST_FILE_ID_LENGTH);
    // server id: not important
    sml_octet_string_write(buf,NULL,16);
    // username: not important
    sml_octet_string_write(buf,NULL,16);
    // password: not important
    sml_octet_string_write(buf,NULL,16);
    //sml version: not important
    sml_uint8_t_write(buf,NULL);
}

void sml_close_request_write(sml_buffer *buf) 
{
    sml_buf_set_type_and_length(buf, SML_TYPE_LIST, 1);
    // not important
    sml_octet_string_write(buf,NULL,16);
}

void sml_tree_path_write(sml_buffer *buf, int32_t n_path_entries, unsigned char **path_entries, int32_t *path_entries_length) 
{
    if ((n_path_entries > 0) && path_entries && path_entries_length) 
    {
        sml_buf_set_type_and_length(buf, SML_TYPE_LIST, n_path_entries);
        
        int32_t i;
        for (i = 0; i < n_path_entries; i++) 
        {
            sml_octet_string_write(buf,path_entries[i],path_entries_length[i]);
        }
    }
}

void sml_get_proc_parameter_request_write(sml_buffer *buf, int32_t n_path_entries, unsigned char **path_entries, int32_t *path_entries_length) 
{
    sml_buf_set_type_and_length(buf, SML_TYPE_LIST, 5);
    
    //server id: not important
    sml_octet_string_write(buf,NULL,16);
    //username: not important
    sml_octet_string_write(buf,NULL,16);
    //password: not important
    sml_octet_string_write(buf,NULL,16);
    
    //tree path: important
    sml_tree_path_write(buf, n_path_entries, path_entries, path_entries_length);
    
    //attribute: not important
    sml_octet_string_write(buf,NULL,16);
}

void sml_message_write(sml_buffer *buf, unsigned char *transaction_id, uint8_t group_id, uint8_t abort_on_error, uint32_t msg_body_tag, int32_t n_path_entries, unsigned char **path_entries, int32_t *path_entries_length) 
{
    int32_t msg_start = buf->cursor;
    
    sml_buf_set_type_and_length(buf, SML_TYPE_LIST, 6);
    sml_octet_string_write(buf,transaction_id,TRANSACTION_ID_LENGTH);
    sml_uint8_t_write(buf,&group_id);
    sml_uint8_t_write(buf,&abort_on_error);
    
    // sml message body write
    sml_buf_set_type_and_length(buf, SML_TYPE_LIST, 2);
    sml_uint32_t_write(buf,&msg_body_tag);
    
    switch (msg_body_tag) 
    {
        case SML_MESSAGE_OPEN_REQUEST:
        {
            unsigned char req_file_id[REQUEST_FILE_ID_LENGTH];
            generate_random_ascii(req_file_id, REQUEST_FILE_ID_LENGTH);
            sml_open_request_write(buf, (unsigned char*) &parser_client_id.data, req_file_id);
            break;
        }
        case SML_MESSAGE_CLOSE_REQUEST:
            sml_close_request_write(buf);
            break;
            
        case SML_MESSAGE_GET_PROC_PARAMETER_REQUEST:
            sml_get_proc_parameter_request_write(buf, n_path_entries, path_entries, path_entries_length);
            break;
            //         case SML_MESSAGE_GET_PROFILE_PACK_REQUEST:
            //             sml_get_profile_pack_request_write((sml_get_profile_pack_request *) message_body->data, buf);
            //             break;
            //         case SML_MESSAGE_GET_PROFILE_LIST_REQUEST:
            //             sml_get_profile_list_request_write((sml_get_profile_list_request *) message_body->data, buf);
            //             break;
            //         case SML_MESSAGE_SET_PROC_PARAMETER_REQUEST:
            //             sml_set_proc_parameter_request_write((sml_set_proc_parameter_request *) message_body->data, buf);
            //             break;
            //         case SML_MESSAGE_GET_LIST_REQUEST:
            //             sml_get_list_request_write((sml_get_list_request *)message_body->data, buf);
            //             break;
            //         case SML_MESSAGE_GET_LIST_RESPONSE:
            //             sml_get_list_response_write((sml_get_list_response *) message_body->data, buf);
            //             break;
        default:
            #if DEBUG
                printf("ENIY\n");
            #endif
            break;
    }
    
    // done sml message body write
    uint16_t crc;
    sml_uint16_t_init(sml_crc16_calculate(&(buf->buffer[msg_start]), buf->cursor - msg_start), (unsigned char *) &crc);
    sml_uint16_t_write(buf,&crc);
    
    // end of message
    buf->buffer[buf->cursor] = 0x0;
    buf->cursor++;
}

int32_t sml_request_file_init(sml_buffer *buf, uint8_t *group_id)
{
    unsigned char transaction_id[TRANSACTION_ID_LENGTH];
    uint8_t abort_on_error = 0;
    buf->cursor = 8;
    buf->error = 0;
    memset(buf->buffer,0,buf->buffer_len);
    generate_random_ascii(transaction_id,TRANSACTION_ID_LENGTH);
    *group_id = 0;
    sml_message_write(buf, transaction_id, *group_id, abort_on_error, SML_MESSAGE_OPEN_REQUEST, 0, NULL, NULL);
    return 0;
}

int32_t sml_request_file_finalize(sml_buffer *buf, uint8_t *group_id)
{
    unsigned char transaction_id[TRANSACTION_ID_LENGTH];
    uint8_t abort_on_error = 0;
    generate_random_ascii(transaction_id,TRANSACTION_ID_LENGTH);
    
    sml_message_write(buf,transaction_id, ++(*group_id),abort_on_error, SML_MESSAGE_CLOSE_REQUEST, 0, NULL, NULL); 
    return 0;
}

int32_t send_sml_message_get_proc_parameter_request(int64_t obis_key)
{
    uint8_t group_id;
    uint8_t abort_on_error;
    int32_t obis_key_length = OBIS_KEY_LENGTH;
    unsigned char transaction_id[TRANSACTION_ID_LENGTH];
    unsigned char obis_data[OBIS_KEY_LENGTH];
    unsigned char *obis_byte_key = obis_data;
    
    sml_buffer send_buffer;
    
    send_buffer.buffer = request_buffer;
    send_buffer.buffer_len = MC_SML_BUFFER_LEN;
    sml_request_file_init(&send_buffer, &group_id);
    generate_random_ascii(transaction_id,TRANSACTION_ID_LENGTH);
    abort_on_error = 0;
    
    sml_octet_string_init_from_int((unsigned char*) &obis_key,OBIS_KEY_LENGTH,obis_byte_key);
    sml_message_write(&send_buffer, transaction_id, group_id++, abort_on_error , SML_MESSAGE_GET_PROC_PARAMETER_REQUEST, 1, &obis_byte_key , &obis_key_length); 
    
    sml_request_file_finalize(&send_buffer, &group_id);
    //TODO
    //sml_send(&send_buffer);
    return 0;
}

void parser_send_requests()
{
    send_sml_message_get_proc_parameter_request(CURRENT_L1);  
}

void update_property(unsigned char *obj_name, int32_t obj_name_length, uint8_t status_type, uint64_t status_data, uint8_t time_tag, uint64_t time_data, 
                     uint8_t unit_tag, int8_t scaler, uint8_t value_type, unsigned char *value_octet_string_data, int32_t value_octet_string_length, uint8_t value_boolean_data, uint64_t value_int_data)
{
    int64_t obis_key = octet_string_to_int(obj_name,obj_name_length);
    uint64_t seconds = (uint64_t) time_data;
                
    switch(obis_key)
    {
        case PRODUCT_IDENTIFICATION:
            update_octet_string_of_property(&smart_meter_state.product_identification, value_octet_string_data, value_octet_string_length, seconds);
            break;
        case PRODUCT_SINGLE_IDENTIFICATION:  
            update_octet_string_of_property(&smart_meter_state.product_single_identification, value_octet_string_data, value_octet_string_length, seconds);
            break;
        case PUBLIC_KEY:
            update_octet_string_of_key_property(&smart_meter_state.public_key, value_octet_string_data, value_octet_string_length, seconds);
            break;
        case ENERGY_VALUE_TOTAL:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            unit u = int_to_unit(unit_tag);
            if(u == Wh)
            {
                result /= 1000;
                u = kWh;
            }
            update_values_of_property(&smart_meter_state.energy_value_total, result, u, seconds);
            break;
        }
        case ENERGY_VALUE_1:
        {
            double result = ((double) value_int_data) * my_pow(10, (double)scaler);
            unit u = int_to_unit(unit_tag);
            if(u == Wh)
            {
                result /= 1000;
                u = kWh;
            }
            update_values_of_property(&smart_meter_state.energy_value1, result, u, seconds);
            break;
        }
        case ENERGY_VALUE_2:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            unit u = int_to_unit(unit_tag);
            if(u == Wh)
            {
                result /= 1000;
                u = kWh;
            }
            update_values_of_property(&smart_meter_state.energy_value2, result, u, seconds);
            break;
        }
        case CURRENT_ACTIVE_POWER:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_active_power, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case ACTIVE_POWER_L1:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.active_power_l1, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case ACTIVE_POWER_L2:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.active_power_l2, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case ACTIVE_POWER_L3:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.active_power_l3, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_L1:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_l1, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_L2:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_l2, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_L3:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_l3, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_L1:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_l1, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_L2:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_l2, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_L3:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_l3, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_MINIMUM:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_minimum, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_MAXIMUM:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_maximum, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_chip_temperature, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case MINIMAL_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.minimal_chip_temperature, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case MAXIMAL_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.maximal_chip_temperature, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case AVERAGED_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * my_pow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.averaged_chip_temperature, result, int_to_unit(unit_tag), seconds);
            break;
        }
        default:
        {
            #if DEBUG
                int32_t index=0;
                int32_t j;
                char h_buf[64];
                memset(h_buf,0,64);
                sprintf(h_buf,"EID: 0x");
                index += strlen(h_buf);
                for(j=0;j<obj_name_length && index < 62;j++)
                {
                    sprintf(h_buf+index, "%02X", obj_name[j]);
                    index += 2;
                }
                printf(h_buf);
            #endif
            break;
        }
    }
}

void handle_sml_message_get_profile_list_response(sml_buffer *buf)
{
//     
}

void handle_sml_message_get_profile_pack_response(sml_buffer *buf)
{
//     
}

void handle_sml_message_open_response(sml_buffer *buf)
{
    if (sml_buf_get_next_type(buf) != SML_TYPE_LIST) 
    {
        buf->error = 1;
        goto error;
    }
    
    if (sml_buf_get_next_length(buf) != 6) 
    {
        buf->error = 1;
        goto error;
    }
    
    //msg codepage: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg client id: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg req file id: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg server id: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg ref time: not important
    sml_time_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg sml version: not important
    sml_uint8_t_parse(buf,NULL,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    error:
    return;
}

void handle_sml_message_close_response(sml_buffer *buf)
{
    if (sml_buf_get_next_type(buf) != SML_TYPE_LIST) 
    {
        buf->error = 1;
        goto error;
    }
    
    if (sml_buf_get_next_length(buf) != 1) 
    {
        buf->error = 1;
        goto error;
    }
    
    //msg global signature: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    error:
    return;
}

void handle_sml_message_get_proc_parameter_response(sml_buffer *buf)
{
    printf("YES!\n");
    exit(0);
}

void handle_sml_message_get_list_response(sml_buffer *buf)
{
    int32_t elems;
    int32_t obj_name_length = 32;
    unsigned char obj_name[obj_name_length];
    uint8_t status_type;
    uint64_t status_data;
    uint8_t list_entry_time_tag;
    uint64_t list_entry_time_data;
    uint8_t unit_tag;
    int8_t scaler;
    uint8_t value_type;
    int32_t value_octet_string_length = 64;
    unsigned char value_octet_string_data[value_octet_string_length];
    uint8_t value_boolean_data;
    uint64_t value_int_data;
    
    if (sml_buf_get_next_type(buf) != SML_TYPE_LIST) 
    {
        buf->error = 1;
        goto error;
    }
    
    if (sml_buf_get_next_length(buf) != 7) 
    {
        buf->error = 1;
        goto error;
    }
    
    //msg client id: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg server id: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg list name: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    // msg time: important
    uint8_t time_tag = 0;
    uint64_t time_data = 0;
    sml_time_parse(buf,&time_tag, &time_data, COPY);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg val_list parse part:
    if (sml_buf_optional_is_skipped(buf)) 
    {
        return;
    }
    
    if (sml_buf_get_next_type(buf) != SML_TYPE_LIST) 
    {
        buf->error = 1;
        return;
    }
    
    elems = sml_buf_get_next_length(buf);
    if (elems > 0) 
    {
        status_type = 0;
        status_data = 0;
        list_entry_time_tag = 0;
        list_entry_time_data = 0;
        unit_tag = 0;
        scaler = 0;
        value_type = 0;
        value_boolean_data = 0;
        value_int_data = 0;
        
        sml_list_entry_parse(buf, obj_name, &obj_name_length, &status_type, &status_data, &list_entry_time_tag, &list_entry_time_data, 
                             &unit_tag, &scaler, &value_type, value_octet_string_data, &value_octet_string_length, &value_boolean_data, &value_int_data , COPY);
        
        if (!sml_buf_has_errors(buf))
        {
            update_property(obj_name,obj_name_length,status_type,status_data,time_tag,time_data,unit_tag,scaler,value_type,value_octet_string_data,value_octet_string_length,value_boolean_data,value_int_data);
            elems--;
        }
    }
    
    
    if(sml_buf_has_errors(buf)) goto error;
    
    while(elems > 0) 
    {
        status_type = 0;
        status_data = 0;
        list_entry_time_tag = 0;
        list_entry_time_data = 0;
        unit_tag = 0;
        scaler = 0;
        value_type = 0;
        value_boolean_data = 0;
        value_int_data = 0;
        obj_name_length = 32;
        value_octet_string_length = 64;
        
        sml_list_entry_parse(buf, obj_name, &obj_name_length, &status_type, &status_data, &list_entry_time_tag, &list_entry_time_data, 
                             &unit_tag, &scaler, &value_type, value_octet_string_data, &value_octet_string_length, &value_boolean_data, &value_int_data , COPY);
        
        if (sml_buf_has_errors(buf)) break;
        
        update_property(obj_name,obj_name_length,status_type,status_data,time_tag,time_data,unit_tag,scaler,value_type,value_octet_string_data,value_octet_string_length,value_boolean_data,value_int_data);
        elems--;
    }
          
    if(sml_buf_has_errors(buf)) goto error;
    //end of msg val_list parse part
    
    //msg list signature: not important
    sml_octet_string_parse(buf,NULL,0,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    //msg act gateway time: not important
    sml_time_parse(buf,NULL,NULL,IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    
    error:
    return;
}

void handle_sml_message_attention_response(sml_buffer *buf)
{
    int32_t server_id_length = 16;
    int32_t attention_number_length = 16;
    int32_t attention_message_length = 16;
    
    unsigned char server_id_data[server_id_length];
    unsigned char attention_number_data[attention_number_length];
    unsigned char attention_message_data[attention_message_length];
    
    if (sml_buf_get_next_type(buf) != SML_TYPE_LIST) 
    {
                buf->error = 1;
                goto error;
    }
    
    if (sml_buf_get_next_length(buf) != 4) 
    {
        buf->error = 1;
        goto error;
    }

    //server id
    sml_octet_string_parse(buf, server_id_data, &server_id_length, COPY);
    if (sml_buf_has_errors(buf)) goto error;

    //attention number
    sml_octet_string_parse(buf, attention_number_data, &attention_number_length, COPY);
    if (sml_buf_has_errors(buf)) goto error;

    //attention_message
    sml_octet_string_parse(buf, attention_message_data, &attention_message_length, COPY);
    if (sml_buf_has_errors(buf)) goto error;

//     sml_tree_parse(buf, &type, octet_string_data, &octet_string_length, &boolean_data, &int_data, &time_tag, &time_data, IGNORE);
    if (sml_buf_has_errors(buf)) goto error;
    char str_att_num[16];
    char str_att_msg[16];
    octet_string_to_string(attention_number_data, attention_number_length, str_att_num, 16);
    octet_string_to_string(attention_message_data, attention_message_length, str_att_msg, 16);
    
    error:
    #if DEBUG
        switch(octet_string_to_int(attention_number_data, attention_number_length))
        {
            case SML_ATTENTION_NUMBER_UNPARSEABLE:
                update_status_message("EPARS");
                break;
            default:
                update_status_message("ENDEF");
        }
    #endif  
    return;
}

int32_t handle_raw_sml_file(unsigned char *buffer, int32_t length)
{
    sml_file_buffer.buffer = buffer+8;
    sml_file_buffer.buffer_len = length - 16;
    sml_file_buffer.cursor = 0;
    sml_file_buffer.error = 0;
    
    // parsing all messages
    
    for (; sml_file_buffer.cursor < sml_file_buffer.buffer_len;) 
    {
        if(sml_buf_get_current_byte(&sml_file_buffer) == SML_MESSAGE_END) 
        {
            sml_buf_update_bytes_read(&sml_file_buffer, 1);
            continue;
        }
        
        // message parsing part
        if (sml_buf_get_next_type(&sml_file_buffer) != SML_TYPE_LIST) 
        {
            sml_file_buffer.error = 1;
            goto error;
        }
        
        if (sml_buf_get_next_length(&sml_file_buffer) != 6) 
        {
            sml_file_buffer.error = 1;
            goto error;
        }
        
        // msg transaction id: not important for our purpose
        sml_octet_string_parse(&sml_file_buffer, NULL, 0, IGNORE);
        if (sml_buf_has_errors(&sml_file_buffer)) goto error;
        
        //msg group id: not important for our purpose
        sml_uint8_t_parse(&sml_file_buffer, NULL, 1);
        if (sml_buf_has_errors(&sml_file_buffer)) goto error;
        
        //msg abort on error: not important for our purpose
        sml_uint8_t_parse(&sml_file_buffer, NULL, 1);
        if (sml_buf_has_errors(&sml_file_buffer)) goto error;
        
        //important stuff:
        if (sml_buf_get_next_type(&sml_file_buffer) != SML_TYPE_LIST) 
        {
            sml_file_buffer.error = 1;
            goto error;
        }
        
        if (sml_buf_get_next_length(&sml_file_buffer) != 2) 
        {
            sml_file_buffer.error = 1;
            goto error;
        }
        
        uint32_t tag;
        sml_uint32_t_parse(&sml_file_buffer,(unsigned char*)&tag,0);
        
        if (sml_buf_has_errors(&sml_file_buffer)) goto error;
        
        switch (tag) 
        {
            case SML_MESSAGE_OPEN_RESPONSE:
                handle_sml_message_open_response(&sml_file_buffer);
                break;
             case SML_MESSAGE_CLOSE_RESPONSE:
                 handle_sml_message_close_response(&sml_file_buffer);
                break;
            case SML_MESSAGE_GET_PROFILE_PACK_RESPONSE:
                handle_sml_message_get_profile_pack_response(&sml_file_buffer);
                break;
            case SML_MESSAGE_GET_PROFILE_LIST_RESPONSE:
                handle_sml_message_get_profile_list_response(&sml_file_buffer);
                break;
            case SML_MESSAGE_GET_PROC_PARAMETER_RESPONSE:
                handle_sml_message_get_proc_parameter_response(&sml_file_buffer);
                break;
            case SML_MESSAGE_GET_LIST_RESPONSE:
                handle_sml_message_get_list_response(&sml_file_buffer);
                break;
            case SML_MESSAGE_ATTENTION_RESPONSE:
                handle_sml_message_attention_response(&sml_file_buffer);
                break;
             default:
                #if DEBUG
                    printf("ETYPE %lX\n", tag);
                #endif
                break;
        }
        // Done parsing message body
        if (sml_buf_has_errors(&sml_file_buffer)) goto error;
        
        
        //msg crc: not important
        sml_uint16_t_parse(&sml_file_buffer,NULL,IGNORE);
        if (sml_buf_has_errors(&sml_file_buffer)) goto error;
        
        if (sml_buf_get_current_byte(&sml_file_buffer) == SML_MESSAGE_END) 
        {
            sml_buf_update_bytes_read(&sml_file_buffer, 1);
        }
        
        //whole message parsed and handled, check for errors:
        
error:
        if (sml_buf_has_errors(&sml_file_buffer)) 
        {
            #if DEBUG
                printf("EREAD\n");
            #endif
            break;
        }
    }
    return 0;
}

void transport_receiver(unsigned char *buffer, size_t buffer_len) 
{
    // the buffer contains the whole message, with transport escape sequences.
    
    handle_raw_sml_file(buffer, buffer_len);
}

void sml_listen() 
{    
    //setup rs232 
    receiver = transport_receiver;
    max_read_len = MC_SML_BUFFER_LEN;
    
    reset_read_state();
    rs232_set_input(RS232_PORT_0, sml_read);
}

void setup_parser()
{
    int32_t i;
    memset(parser_client_id.data,0,STATIC_OCTET_STRING_LENGTH);
    memset(smart_meter_password.data,0,STATIC_OCTET_STRING_LENGTH);
    for(i=0;i<CLIENT_ID_LENGTH;i++)
    {
        parser_client_id.data[i] = rand()%256;
    }
    parser_client_id.len = CLIENT_ID_LENGTH;
    #ifdef _EHZ363ZA_
    smart_meter_password.data[0] = 0;
    smart_meter_password.data[1] = 0;
    smart_meter_password.data[2] = 0;
    smart_meter_password.data[3] = 0;
    smart_meter_password.len = 4;
    #endif    
}