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

static sml_read_info r_info;
static unsigned char request_buffer[MC_SML_BUFFER_LEN];
static parser_progress_stack pps;
int16_t handle_received_byte(unsigned char byte, int32_t skip);

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
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
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
            double result = ((double) value_int_data) * zpow(10, (double)scaler);
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
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
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
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_active_power, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case ACTIVE_POWER_L1:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.active_power_l1, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case ACTIVE_POWER_L2:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.active_power_l2, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case ACTIVE_POWER_L3:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.active_power_l3, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_L1:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_l1, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_L2:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_l2, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_L3:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_l3, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_L1:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_l1, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_L2:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_l2, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_L3:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_l3, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_MINIMUM:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_minimum, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case VOLTAGE_MAXIMUM:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.voltage_maximum, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case CURRENT_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.current_chip_temperature, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case MINIMAL_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.minimal_chip_temperature, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case MAXIMAL_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
            update_values_of_property(&smart_meter_state.maximal_chip_temperature, result, int_to_unit(unit_tag), seconds);
            break;
        }
        case AVERAGED_CHIP_TEMPERATURE:
        {
            double result = ((double) value_int_data) * zpow(10,(double)scaler);
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
            printf("%s\n",h_buf);
            #endif
            break;
        }
    }
}

int16_t init_pps()
{ 
    pps.stack_pointer = 1;
    pps.stack[0].state = STATE_ERROR;
    pps.stack[0].step = 0;
    pps.stack[1].state = STATE_READY;
    pps.stack[1].step = 0;
    return 0;
}

int16_t ps_call(parser_state s, unsigned char byte)
{
    if(pps.stack_pointer < MAX_STACK_SIZE)
    {
        pps.stack[pps.stack_pointer].step++;
        pps.stack[++pps.stack_pointer].state = s;
        pps.stack[pps.stack_pointer].step = 0;
        handle_received_byte(byte, NOSKIP);
        return 0;
    }
    else
    {
        #if DEBUG
        printf("ESS\n");
        #endif
        return 1;
    }
}

int16_t ps_return(unsigned char byte)
{
    if(pps.stack_pointer > 0)
    {
        pps.stack_pointer--;
        handle_received_byte(byte,NOSKIP);
        return 0;
    }
    else
    {
        #if DEBUG
        printf("ESP\n");
        #endif
        return 1;
    }
}

int16_t ps_next_step(unsigned char byte, int32_t skip)
{
    pps.stack[pps.stack_pointer].step++;
    handle_received_byte(byte,skip);
    return 0;
}

parser_state ps_get_state()
{
    return pps.stack[pps.stack_pointer].state;
}

uint16_t ps_get_step()
{
    return pps.stack[pps.stack_pointer].step;
}

int32_t reset_read_state()
{
    r_info.esc = 0;
    r_info.start = 0;
    r_info.end = 0;
    r_info.i = 0;
    r_info.done = 0;
    init_pps();
    return 0;
}

int8_t sml_get_next_type(unsigned char byte)
{
    return byte & SML_TYPE_FIELD;
}

int8_t sml_byte_optional_is_skipped(unsigned char byte) 
{
    if (byte == SML_OPTIONAL_SKIPPED) 
    {
        return 0;
    }
    return 1;
}

static uint8_t sml_tmp_type;
static uint8_t sml_updated;
static uint8_t sml_error;
static uint8_t sml_tag;
static uint8_t sml_type;
static uint8_t sml_negative_int;
static int32_t sml_list_elements;
static int32_t sml_index;
static int32_t sml_length;
static int32_t sml_list;
static uint32_t sml_octet_string_length;
static uint32_t sml_max_size;
static uint32_t sml_tmp_max;
static uint32_t sml_missing_bytes;
static int64_t sml_int64_data;
static uint64_t sml_uint64_data;
static unsigned char sml_octet_string_data[MAX_OCTET_STRING_DATA_LENGTH];

// values for updating parser state
static uint8_t sml_list_entry_unit_tag;
static uint8_t sml_list_entry_value_type;
static uint8_t sml_list_time_tag;
static uint8_t sml_list_entry_status_type;
static uint8_t sml_list_entry_scaler;
static uint8_t sml_list_entry_value_boolean_data;
static int32_t sml_list_entry_obj_name_length;
static int32_t sml_list_entry_value_octet_string_length;
static uint64_t sml_list_entry_value_int_data;
static uint64_t sml_list_time_data;
static uint64_t sml_list_entry_status_data;
static unsigned char sml_list_entry_obj_name_data[MAX_OCTET_STRING_DATA_LENGTH];
static unsigned char sml_list_entry_value_octet_string_data[MAX_OCTET_STRING_DATA_LENGTH];

int16_t handle_received_byte(unsigned char byte, int32_t skip)
{
    if(skip)
    {
        return 0;
    }
    
    switch(ps_get_state())
    {
        case STATE_READY:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(!r_info.i)
                    {
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 1:
                {
                    ps_call(STATE_SML_FILE,byte);
                    break;
                }
                default:
                    reset_read_state();
                    break;
            }
            break;
        }
        case STATE_SML_FILE:
        {
            switch(ps_get_state())
            {
                case 0:
                {
                    sml_error = 0;
                }
                default:
                {
                    if(!r_info.done)
                    {
                        if(sml_error)
                        {
                            ps_call(STATE_ERROR, SML_MESSAGE_END);
                            break;
                        }
                        if(byte == SML_MESSAGE_END || byte == 0x1b || byte == 0x1a || r_info.end)
                        {
                            break;
                        }
                        else
                        {
                            ps_call(STATE_SML_MESSAGE,byte);
                        }
                    }
                    else
                    {
                        ps_return(byte);
                    }
                    break;
                }
            }
            break;
        }
        case STATE_SML_MESSAGE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(sml_get_next_type(byte) != SML_TYPE_LIST) 
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 1:
                {
                    if(sml_length != 6)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 2:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 3:
                {
                    //extract transaction id here
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 4:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8);
                    break;
                }
                case 5:
                {
                    //extract group id here
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 6:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8);
                    break;
                }
                case 7:
                {
                    //extract abort on error here
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 8:
                {
                    if(sml_get_next_type(byte) != SML_TYPE_LIST)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 9:
                {
                    if(sml_length != 2)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 10:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_32);
                    break;
                }
                case 11:
                {
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 12:
                {
                    switch(sml_int64_data)
                    {
                        case SML_MESSAGE_OPEN_RESPONSE:
                        {
                            ps_call(STATE_SML_MESSAGE_OPEN_RESPONSE,byte);
                            break;
                        }
                        case SML_MESSAGE_CLOSE_RESPONSE:
                        {
                            ps_call(STATE_SML_MESSAGE_CLOSE_RESPONSE,byte);
                            break;
                        }
                        case SML_MESSAGE_GET_LIST_RESPONSE:
                        {
                            ps_call(STATE_SML_MESSAGE_GET_LIST_RESPONSE,byte);
                            break;
                        }
                        default:
                        {
                            #if DEBUG
                                printf("ETAG\n");
                            #endif
                            sml_error = 1;
                            ps_return(SML_MESSAGE_END);
                            break;
                        }
                    }
                    break;
                }
                case 13:
                {
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 14:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_16);
                    break;
                }
                case 15:
                {
                    // extract sml crc here
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 16:
                {
                    if(byte == SML_MESSAGE_END)
                    {
                        break;
                    }
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_MESSAGE_OPEN_RESPONSE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(sml_get_next_type(byte) != SML_TYPE_LIST)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break; 
                }
                case 1:
                {
                    if(sml_length != 6)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 2:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 3:
                {
                    // extract codepage here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 4:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 5:
                {
                    //extract client id here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 6:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 7:
                {
                    // extract req file id here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 8:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 9:
                {
                    // extract server id here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 10:
                {
                    SML_TIME_PARSE(byte);
                    break;
                }
                case 11:
                {
                    // extract time tag and data here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 12:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8);
                    break;
                }
                case 13:
                {
                    //extract sml version here
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_MESSAGE_CLOSE_RESPONSE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(sml_get_next_type(byte) != SML_TYPE_LIST)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 1:
                {
                    if(sml_length != 1)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 2:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 3:
                {
                    // extract global signature here
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_MESSAGE_GET_LIST_RESPONSE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(sml_get_next_type(byte) != SML_TYPE_LIST)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 1:
                {
                    if(sml_length != 7)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 2:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 3:
                {
                    //extract client_id here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 4:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 5:
                {
                    //extract server id here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 6:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 7:
                {
                    //extract msg list name here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 8:
                {
                    sml_list_time_tag = 0;
                    sml_list_time_data = 0;
                    SML_TIME_PARSE(byte);
                    break;
                }
                case 9:
                {
                    //extract msg time here
                    sml_list_time_data = sml_tag;
                    sml_list_time_data = sml_uint64_data;
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 10:
                {
                    if(!sml_byte_optional_is_skipped(byte))
                    {
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    if(sml_get_next_type(byte) != SML_TYPE_LIST)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,NOSKIP);
                    break;
                }
                case 11:
                {
                    sml_list_elements = 0;
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 12:
                {
                    sml_list_elements = sml_length;
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 13:
                {
                    if(sml_list_elements > 0)
                    {
                        sml_updated = 1;
                        ps_call(STATE_SML_LIST_ENTRY_LOOP,byte);
                        break;
                    }
                    else
                    {
                        ps_next_step(byte,NOSKIP);
                    }
                    break;
                }
                case 14:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 15:
                {
                    //extract list signature here
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 16:
                {
                    SML_TIME_PARSE(byte);
                    break;
                }
                case 17:
                {
                    //extract gateway time here
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_LIST_ENTRY_LOOP:
        {
            switch(ps_get_step())
            {
                default:
                {
                    if(sml_list_elements > 0)
                    {
                        if(sml_updated)
                        {
                            sml_list_entry_status_type = 0;
                            sml_list_entry_status_data = 0;
                            sml_list_entry_unit_tag = 0;
                            sml_list_entry_scaler = 0;
                            sml_list_entry_obj_name_length = 0;
                            memset(sml_list_entry_obj_name_data,0,MAX_OCTET_STRING_DATA_LENGTH);
                            sml_list_entry_value_type = 0;
                            sml_list_entry_value_octet_string_length = 0; 
                            memset(sml_list_entry_value_octet_string_data, 0, MAX_OCTET_STRING_DATA_LENGTH);
                            sml_list_entry_value_boolean_data = 0;
                            sml_list_entry_value_int_data = 0;
                            sml_updated = 0;
                            ps_call(STATE_SML_LIST_ENTRY_PARSE,byte);
                            break;
                        }
                        else
                        {
                            update_property(sml_list_entry_obj_name_data,sml_list_entry_obj_name_length,
                                        sml_list_entry_status_type,sml_list_entry_status_data,
                                        sml_list_time_tag,sml_list_time_data,sml_list_entry_unit_tag,
                                        sml_list_entry_scaler,sml_list_entry_value_type,
                                        sml_list_entry_value_octet_string_data,sml_list_entry_value_octet_string_length,
                                        sml_list_entry_value_boolean_data,sml_list_entry_value_int_data);
                            sml_updated = 1;
                            sml_list_elements--;
                            ps_next_step(byte,SKIP);
                            break;
                        }
                    }
                    else
                    {
                        ps_return(byte);
                        break;
                    }
                }
            }
            break;
        }
        case STATE_SML_LIST_ENTRY_PARSE:
        {
           switch(ps_get_step())
            {
                case 0:
                {
                     if(sml_get_next_type(byte) != SML_TYPE_LIST)
                     {
                         sml_error = 1;
                         ps_return(SML_MESSAGE_END);
                         break;
                     }
                     SML_GET_NEXT_LENGTH_PARSE(byte);
                     break;
                }
                case 1:
                {
                    if(sml_length != 7)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 2:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 3:
                {
                    //extract obj name here
                    sml_list_entry_obj_name_length = sml_octet_string_length;
                    memcpy(sml_list_entry_obj_name_data,sml_octet_string_data, sml_octet_string_length);
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 4:
                {
                    SML_STATUS_PARSE(byte);
                    break;
                }
                case 5:
                {
                    //extract status data here
                    sml_list_entry_status_type = sml_type;
                    sml_list_entry_status_data = (uint64_t) sml_int64_data;
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 6:
                {
                    SML_TIME_PARSE(byte);
                    break;
                }
                case 7:
                {
                    //extract time data here
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 8:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8);
                    break;
                }
                case 9:
                {
                    //extract unit tag here
                    sml_list_entry_unit_tag = (uint8_t) sml_int64_data;
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 10:
                {
                    SML_NUMBER_PARSE(SML_TYPE_INTEGER, SML_TYPE_NUMBER_8);
                    break;
                }
                case 11:
                {
                    //extract scaler here
                    sml_list_entry_scaler = (int8_t) sml_int64_data;
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 12:
                {
                    sml_tmp_type = 0;
                    SML_VALUE_PARSE(byte);
                    break;
                }
                case 13:
                {
                    //extract value here
                    sml_list_entry_value_type = sml_type;
                    sml_list_entry_value_octet_string_length = sml_octet_string_length; 
                    memcpy(sml_list_entry_value_octet_string_data, sml_octet_string_data, sml_octet_string_length);
                    sml_list_entry_value_boolean_data = (uint8_t) sml_int64_data;
                    sml_list_entry_value_int_data = (uint64_t)sml_int64_data;
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 14:
                {
                    SML_OCTET_STRING_PARSE(byte);
                    break;
                }
                case 15:
                {
                    //extract sml signature here
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_VALUE_PARSE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(!sml_byte_optional_is_skipped(byte))
                    {
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,NOSKIP);
                    break;
                }
                case 1:
                {
                    sml_tmp_max = 1;
                    sml_tmp_type = sml_get_next_type(byte);
                    switch(sml_tmp_type)
                    {
                        case SML_TYPE_OCTET_STRING:
                        {
                            SML_OCTET_STRING_PARSE(byte);
                            break;
                        }
                        case SML_TYPE_BOOLEAN:
                        {
                            SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8);
                            break;
                        }
                        case SML_TYPE_UNSIGNED:
                        {
                        }
                        case SML_TYPE_INTEGER:
                        {
                            while (sml_tmp_max < ((byte & SML_LENGTH_FIELD) - 1)) {
                                sml_tmp_max <<= 1;
                            }       
                            SML_NUMBER_PARSE(sml_tmp_type,sml_tmp_max);
                            break;
                        }
                        default:
                        {
                            sml_error = 1;
                            ps_return(SML_MESSAGE_END);
                            break;
                        }
                    }
                    break;
                }
                case 2:
                {
                    if(sml_tmp_type == SML_TYPE_UNSIGNED || sml_tmp_type == SML_TYPE_INTEGER)
                    {
                        sml_tmp_type |= sml_tmp_max;
                    }    
                    sml_type = sml_tmp_type;
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_STATUS_PARSE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(!sml_byte_optional_is_skipped(byte))
                    {
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,NOSKIP);
                    break;
                }
                case 1:
                {
                    sml_tmp_max = 1;
                    sml_tmp_type = sml_get_next_type(byte);
                    switch(sml_tmp_type)
                    {
                        case SML_TYPE_UNSIGNED:
                        {
                            // get maximal size, if not all bytes are used (example: only 6 bytes for a uint64_t)
                            while (sml_tmp_max < ((byte & SML_LENGTH_FIELD) - 1)) 
                            {
                                    sml_tmp_max <<= 1;
                            }
                            SML_NUMBER_PARSE(sml_tmp_type,sml_tmp_max);
                            break;
                        }
                        default:
                        {
                            sml_error = 1;
                            ps_return(SML_MESSAGE_END);
                            break;
                        }
                    }
                    break;
                }
                case 2:
                {
                    sml_tmp_type |= sml_tmp_max;
                    sml_type = sml_tmp_type;
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_TIME_PARSE:
        {
            switch(ps_get_step())
            {
                case 0:
                { 
                    if(!sml_byte_optional_is_skipped(byte))
                    {
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte, NOSKIP);
                    break;
                }
                case 1:
                {
                    if(sml_get_next_type(byte) != SML_TYPE_LIST)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 2:
                {
                    if(sml_length != 2)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 3:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8);
                    break;
                }
                case 4:
                {
                    //extract time tag here
                    sml_tag = (uint8_t) sml_int64_data;
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 5:
                {
                    SML_NUMBER_PARSE(SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_32);
                    break;
                }
                case 6:
                {
                    sml_uint64_data = (uint64_t) sml_int64_data;
                    ps_return(byte);
                    break;
                }
            }
            break;
        }
        case STATE_SML_OCTET_STRING_PARSE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(!sml_byte_optional_is_skipped(byte))
                    {
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte, NOSKIP);
                    break;
                }
                case 1:
                {
                    if(sml_get_next_type(byte) != SML_TYPE_OCTET_STRING)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 2:
                {
                    if(sml_length < 0 || sml_length > MAX_OCTET_STRING_DATA_LENGTH)
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte, SKIP);
                    break;
                }
                case 3:
                {   
                    if(sml_octet_string_length < sml_length)
                    {
                        sml_octet_string_data[sml_octet_string_length++] = byte;
                    }
                    if(sml_octet_string_length == sml_length)
                    {
                        ps_return(byte);
                    }
                    break;
                }
            }
            break;
        }
        case STATE_SML_NUMBER_PARSE:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    if(!sml_byte_optional_is_skipped(byte))
                    {
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    ps_next_step(byte, NOSKIP);
                    break;
                }
                case 1:
                {
                    
                    if(sml_get_next_type(byte) != sml_type) 
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    SML_GET_NEXT_LENGTH_PARSE(byte);
                    break;
                }
                case 2:
                {
                    ps_next_step(byte,SKIP);
                    break;
                }
                case 3:
                {
                    if (sml_length < 0 || sml_length > sml_max_size) 
                    {
                        sml_error = 1;
                        ps_return(SML_MESSAGE_END);
                        break;
                    }
                    
                    if (sml_type == SML_TYPE_INTEGER && (byte & 128))
                    {
                        sml_negative_int = 1;
                    }
                    
                    sml_missing_bytes = sml_max_size - sml_length;
                    ps_next_step(byte,NOSKIP);
                    break;
                }
                case 4:
                {
                    if(sml_index < sml_length)
                    {
                        ((unsigned char*) &sml_int64_data)[sml_missing_bytes + sml_index++] = byte;
                    }
                    if(sml_index == sml_length)
                    {
                        if (sml_negative_int) 
                        {
                            int8_t i;
                            for (i = 0; i < sml_missing_bytes; i++) 
                            {
                                ((unsigned char*) &sml_int64_data)[i] = 0xFF;
                            }
                        }
                        if (!(sml_number_endian() == SML_BIG_ENDIAN)) 
                        {
                            sml_number_byte_swap((unsigned char*)&sml_int64_data, sml_max_size);
                        }
                        ps_return(byte);
                    }
                    break;
                }
            }
            break;
        }
        case STATE_SML_GET_NEXT_LENGTH:
        {
            switch(ps_get_step())
            {
                case 0:
                {
                    sml_list = ((byte & SML_TYPE_FIELD) == SML_TYPE_LIST) ? 0 : -1;
                    ps_next_step(byte, NOSKIP);
                    break;
                }
                case 1:
                {
                    sml_length <<= 4;
                    sml_length |= (byte & SML_LENGTH_FIELD);
                    if ((byte & SML_ANOTHER_TL) != SML_ANOTHER_TL) 
                    {
                        sml_length += sml_list;
                        ps_return(byte);
                        break;
                    }
                    if(sml_list) 
                    {
                        sml_list += -1;
                    }
                    break;
                }
            }
            break;
        }
        case STATE_ERROR:
        {
            #if DEBUG
                printf("ERROR\n");
            #endif
            break;
        }
        default:
        {
            #if DEBUG
                printf("ESTATE\n");
            #endif
            break;
        }
    }
    return 0;
}

int16_t sml_read(unsigned char byte)
{
    int8_t handle = 0;
    
    if(r_info.i < MAX_READ_LENGTH)
    {
        if (!r_info.i) 
        { 
            // read until escaped start sequence;
            if (r_info.esc == 4) 
            {
                if (byte == 0x01) 
                {
                    handle = 1;
                    r_info.start++;
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
                    handle = 1;
                    r_info.esc++;
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
            handle = 1;
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
                    if(byte == 0x1a)
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
                if(byte == 0x1b)
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
        if(handle)
        {
            handle_received_byte(byte,NOSKIP);
        }
        if(r_info.done)
        {
            if(r_info.i > 0)
            {
                return 0;
            }
            reset_read_state();
            return 1;
        }
    }
    else
    {
        #if DEBUG
        printf("ESML\n");
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

unsigned char *sml_buf_get_current_buf(sml_buffer *buf)
{
    return &(buf->buffer[buf->cursor]);
}

void sml_buf_optional_write(sml_buffer *buf) 
{
    buf->buffer[buf->cursor] = SML_OPTIONAL_SKIPPED;
    buf->cursor++;
}

void sml_buf_set_type_and_length(sml_buffer *buf, uint32_t type, uint32_t l) 
{
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

void sml_number_init(uint64_t number, unsigned char type, int32_t size, unsigned char *np) 
{
    memset(np, 0, size);
    memcpy(np, &number, size);
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
    sml_send(&send_buffer);
    return 0;
}

void parser_send_requests()
{
    send_sml_message_get_proc_parameter_request(CURRENT_L1);  
}

void sml_listen() 
{    
    //setup rs232 
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