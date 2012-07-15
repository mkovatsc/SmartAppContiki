/* smlparser.h */

#include "settings.h"
#include "shared.h"
#include <stdio.h>

// SML DEFINITIONS

#define COPY                                            0
#define IGNORE                                          1

#define MAX_READ_LENGTH                                 4096
#define MC_SML_BUFFER_LEN                               256

#define CLIENT_ID_LENGTH                                8
#define REQUEST_FILE_ID_LENGTH                          8
#define TRANSACTION_ID_LENGTH                           8
#define STATIC_OCTET_STRING_LENGTH                      8 // >= upper three directives

#define SML_MESSAGE_OPEN_REQUEST                        0x00000100
#define SML_MESSAGE_OPEN_RESPONSE                       0x00000101
#define SML_MESSAGE_CLOSE_REQUEST                       0x00000200
#define SML_MESSAGE_CLOSE_RESPONSE                      0x00000201
#define SML_MESSAGE_GET_PROFILE_PACK_REQUEST            0x00000300
#define SML_MESSAGE_GET_PROFILE_PACK_RESPONSE           0x00000301
#define SML_MESSAGE_GET_PROFILE_LIST_REQUEST            0x00000400
#define SML_MESSAGE_GET_PROFILE_LIST_RESPONSE           0x00000401
#define SML_MESSAGE_GET_PROC_PARAMETER_REQUEST          0x00000500
#define SML_MESSAGE_GET_PROC_PARAMETER_RESPONSE         0x00000501
#define SML_MESSAGE_SET_PROC_PARAMETER_REQUEST          0x00000600
#define SML_MESSAGE_SET_PROC_PARAMETER_RESPONSE         0x00000601 /*This doesn't exist in the spec*/
#define SML_MESSAGE_GET_LIST_REQUEST                    0x00000700
#define SML_MESSAGE_GET_LIST_RESPONSE                   0x00000701
#define SML_MESSAGE_ATTENTION_RESPONSE                  0x0000FF01

#define SML_MESSAGE_END                                 0x0
#define SML_TYPE_FIELD                                  0x70
#define SML_LENGTH_FIELD                                0xF
#define SML_ANOTHER_TL                                  0x80
#define SML_TYPE_OCTET_STRING                           0x0
#define SML_TYPE_BOOLEAN                                0x40
#define SML_TYPE_INTEGER                                0x50
#define SML_TYPE_UNSIGNED                               0x60
#define SML_TYPE_LIST                                   0x70
#define SML_OPTIONAL_SKIPPED                            0x1

#define SML_PROC_PAR_VALUE_TAG_VALUE                    0x01
#define SML_PROC_PAR_VALUE_TAG_PERIOD_ENTRY             0x02
#define SML_PROC_PAR_VALUE_TAG_TUPEL_ENTRY              0x03
#define SML_PROC_PAR_VALUE_TAG_TIME                     0x04

#define SML_ATTENTION_NUMBER_UNPARSEABLE                0x8181C7C7FE08

#define SML_TYPE_NUMBER_8                               sizeof(uint8_t)
#define SML_TYPE_NUMBER_16                              sizeof(uint16_t)
#define SML_TYPE_NUMBER_32                              sizeof(uint32_t)
#define SML_TYPE_NUMBER_64                              sizeof(uint64_t)

#define SML_BIG_ENDIAN                                  1
#define SML_LITTLE_ENDIAN                               0

#define sml_uint8_t_parse(buf,np,ignore)                sml_number_parse(buf, SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8,np,ignore)
#define sml_uint16_t_parse(buf,np,ignore)               sml_number_parse(buf, SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_16,np,ignore)
#define sml_uint32_t_parse(buf,np,ignore)               sml_number_parse(buf, SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_32,np,ignore)
#define sml_int8_t_parse(buf,np,ignore)                 sml_number_parse(buf, SML_TYPE_INTEGER, SML_TYPE_NUMBER_8,np,ignore)
#define sml_uint8_t_write(buf,n)                        sml_number_write(buf, n, SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_8)
#define sml_uint16_t_write(buf,n)                       sml_number_write(buf, n, SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_16)
#define sml_uint32_t_write(buf,n)                       sml_number_write(buf, n, SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_32)
#define sml_uint16_t_init(n,np)                         sml_number_init(n, SML_TYPE_UNSIGNED, SML_TYPE_NUMBER_16,np)
// END SML DEFINITIONIS

// SML PARSER DEFINITIONS
#define NOSKIP  0
#define SKIP    1
#define MAX_STACK_SIZE  16
#define MAX_OCTET_STRING_DATA_LENGTH    64

#define SML_NUMBER_PARSE(type, max_size)        sml_type = type; \
                                                sml_max_size = max_size; \
                                                sml_index = 0; \
                                                sml_negative_int = 0; \
                                                sml_missing_bytes = 0; \
                                                sml_int64_data = 0; \
                                                ps_call(STATE_SML_NUMBER_PARSE,byte)

#define SML_GET_NEXT_LENGTH_PARSE(byte)         sml_length = 0; \
                                                sml_list = 0; \
                                                ps_call(STATE_SML_GET_NEXT_LENGTH, byte)

#define SML_OCTET_STRING_PARSE(byte)            memset(sml_octet_string_data,0,MAX_OCTET_STRING_DATA_LENGTH); \
                                                sml_octet_string_length = 0; \
                                                ps_call(STATE_SML_OCTET_STRING_PARSE,byte)
                                      
#define SML_TIME_PARSE(byte)                    sml_tag = 0; \
                                                sml_uint64_data = 0; \
                                                ps_call(STATE_SML_TIME_PARSE, byte)
                    
#define SML_STATUS_PARSE(byte)                  sml_type = 0; \
                                                sml_int64_data = 0; \
                                                sml_uint64_data = 0; \
                                                ps_call(STATE_SML_STATUS_PARSE, byte)

#define SML_VALUE_PARSE(byte)                   ps_call(STATE_SML_VALUE_PARSE,byte)

typedef struct 
{
    unsigned char *buffer;
    size_t buffer_len;
    int32_t cursor;
    int32_t error;
} sml_buffer;

typedef struct
{
    int32_t esc;
    int32_t start;
    int32_t i;
    int32_t end;
    int32_t done;
}sml_read_info;

typedef enum {
    STATE_READY, STATE_SML_FILE, STATE_ERROR, STATE_SML_MESSAGE, STATE_SML_GET_NEXT_LENGTH, STATE_SML_OCTET_STRING_PARSE, STATE_SML_MESSAGE_OPEN_RESPONSE, 
    STATE_SML_NUMBER_PARSE, STATE_SML_MESSAGE_CLOSE_RESPONSE, STATE_SML_MESSAGE_GET_LIST_RESPONSE, STATE_SML_TIME_PARSE,
    STATE_SML_LIST_ENTRY_PARSE, STATE_SML_STATUS_PARSE, STATE_SML_VALUE_PARSE, STATE_SML_LIST_ENTRY_LOOP
} parser_state;

typedef struct
{
    parser_state state;
    uint8_t step;
}parser_progress;

typedef struct
{
    parser_progress stack[MAX_STACK_SIZE+1];
    uint8_t stack_pointer;
}parser_progress_stack;

typedef struct
{
    unsigned char data[STATIC_OCTET_STRING_LENGTH];
    int32_t len;
} static_octet_string;
// END SML PARSER DEFINITIONS

// SMART METER STATE DEFINITIONS

#define PROPERTY_DATA_LENGTH                            18
#define PROPERTY_KEY_LENGTH                             98

typedef struct
{
    unsigned char property_data[PROPERTY_DATA_LENGTH];
    uint64_t last_update;
}unsigned_char_property;

typedef struct
{
    unsigned char property_key[PROPERTY_KEY_LENGTH];
    uint64_t last_update;
}key_property;

typedef struct
{
    double property_value;
    unit property_unit;
    int64_t last_update;
}double_property;

#ifdef _EHZ363ZA_
#define OBIS_KEY_LENGTH                                 6

//OBIS Key data EHZ363ZA
#define	PRODUCT_IDENTIFICATION 		                0x8181C78203FF	/*Hersteller Identifikation*/
#define	PRODUCT_SINGLE_IDENTIFICATION	                0x0100000009FF	/*Geräteeinzelidentifikation*/
#define	ENERGY_VALUE_TOTAL		                0x0100010800FF	/*Zählerstand Totalregister*/
#define	ENERGY_VALUE_1  		                0x0100010801FF	/*Zählerstand Tarif 1*/
#define	ENERGY_VALUE_2	        	                0x0100010802FF	/*Zählerstand Tarif 2*/
#define	CURRENT_ACTIVE_POWER		                0x01000F0700FF	/*aktuelle Wirkleistung*/
#define	ACTIVE_POWER_L1		                        0x0100150700FF	/*Wirkleistung L1*/
#define	ACTIVE_POWER_L2		                        0x0100290700FF	/*Wirkleistung L2*/
#define	ACTIVE_POWER_L3		                        0x01003D0700FF	/*Wirkleistung L3*/
#define	PUBLIC_KEY			                0x8181C78205FF	/*öffentlicher Schlüssel*/

//OBIS Optional Key data EHZ363ZA
#define	CURRENT_CHIP_TEMPERATURE		        0x010060320002	/*Aktuelle Chiptemperatur*/
#define	MINIMAL_CHIP_TEMPERATURE		        0x010060320003	/*Minimale Chiptemperatur*/
#define	MAXIMAL_CHIP_TEMPERATURE		        0x010060320004	/*Maximale Chiptemperatur*/
#define	AVERAGED_CHIP_TEMPERATURE		        0x010060320005	/*Gemittelte Chiptemperatur*/
#define	VOLTAGE_MINIMUM		                        0x010060320303	/*Spannungsminimum*/
#define	VOLTAGE_MAXIMUM		                        0x010060320304	/*Spannungsmaximum*/
#define	CURRENT_L1			                0x01001F0700FF	/*Strom L1*/
#define	VOLTAGE_L1			                0x0100200700FF	/*Spannung L1*/
#define	CURRENT_L2			                0x0100330700FF	/*Strom L2*/
#define	VOLTAGE_L2			                0x0100340700FF	/*Spannung L2*/
#define	CURRENT_L3			                0x0100470700FF	/*Strom L3*/
#define	VOLTAGE_L3			                0x0100480700FF	/*Spannung L3*/

typedef struct
{
    unsigned_char_property product_identification;
    unsigned_char_property product_single_identification;
    key_property public_key;
    
    #if DEBUG
        unsigned_char_property status_message;
    #endif
        
    double_property energy_value_total;
    double_property energy_value1;
    double_property energy_value2;
    double_property current_active_power;
    double_property active_power_l1;
    double_property active_power_l2;
    double_property active_power_l3;
    double_property current_chip_temperature;
    double_property maximal_chip_temperature;
    double_property minimal_chip_temperature;
    double_property averaged_chip_temperature;
    double_property voltage_minimum;
    double_property voltage_maximum;
    double_property current_l1;
    double_property current_l2;
    double_property current_l3;
    double_property voltage_l1;
    double_property voltage_l2;
    double_property voltage_l3;
} ehz363za_state;

// smart meter static data
ehz363za_state smart_meter_state;
#endif
// END SMART METER DEFINITIONS

// PUBLIC FUNCTIONS
void setup_parser();
void sml_listen(); 
void parser_send_requests();
// END PUBLIC FUNCTIONS