/*
 * coap.h
 *
 *  Created on: Aug 25, 2010
 *      Author: dogan
 */

#ifndef COAP_COMMON_H_
#define COAP_COMMON_H_

#include "contiki-net.h"


#ifndef COAP_MAX_OPEN_TRANSACTIONS
#define COAP_MAX_OPEN_TRANSACTIONS  1
#endif /* COAP_MAX_OPEN_TRANSACTIONS */

#ifndef COAP_MAX_PACKET_SIZE /*                       0/14          48 for IPv6 (28 for IPv4) */
#define COAP_MAX_PACKET_SIZE  (UIP_CONF_BUFFER_SIZE - UIP_LLH_LEN - UIP_IPUDPH_LEN) // 132
#endif /* COAP_MAX_PACKET_SIZE */

/*
 * Conservative size limit, as not all options have to be set at the same time.
 */
#ifndef COAP_MAX_PAYLOAD_SIZE /*                      Hdr CoT Age Tag Obs Tok Blo Loc*/
#define COAP_MAX_PAYLOAD_SIZE  (COAP_MAX_PACKET_SIZE - 4 - 2 - 5 - 5 - 5 - 5 - 4 - 0) // 102
#endif /* COAP_MAX_PAYLOAD_SIZE */                  /* 30 + Location */


/*COAP method types*/
typedef enum {
  COAP_GET = 1,
  COAP_POST,
  COAP_PUT,
  COAP_DELETE
} coap_method_t;

typedef enum {
  COAP_TYPE_CON,
  COAP_TYPE_NON,
  COAP_TYPE_ACK,
  COAP_TYPE_RST
} message_type;

typedef enum {
  OK_200 = 80,
  CREATED_201 = 81,
  NOT_MODIFIED_304 = 124,
  BAD_REQUEST_400 = 160,
  NOT_FOUND_404 = 164,
  METHOD_NOT_ALLOWED_405 = 165,
  UNSUPPORTED_MADIA_TYPE_415 = 175,
  INTERNAL_SERVER_ERROR_500 = 200,
  BAD_GATEWAY_502 = 202,
  GATEWAY_TIMEOUT_504 = 204
} status_code_t;

typedef enum {
  COAP_OPTION_CONTENT_TYPE = 1,  /* 1 B */
  COAP_OPTION_MAX_AGE = 2,       /* 1-4 B */
  COAP_OPTION_ETAG = 4,          /* 1-4 B */
  COAP_OPTION_URI_HOST = 5,      /* 1-270 B */
  COAP_OPTION_LOCATION_PATH = 6, /* 1-270 B */
  COAP_OPTION_URI_PATH = 9,      /* 1-270 B */
  COAP_OPTION_OBSERVE = 10,      /* 0-2 B (formerly 0-4) */
  COAP_OPTION_TOKEN = 11,        /* 1-2 B */
  COAP_OPTION_BLOCK = 13,        /* 1-3 B */
  COAP_OPTION_NOOP = 14,         /* 0 B */
  COAP_OPTION_URI_QUERY = 15     /* 1-270 B */
} coap_option_t;

typedef enum {
  TEXT_PLAIN = 0,
  TEXT_XML = 1,
  TEXT_CSV = 2,
  TEXT_HTML = 3,
  IMAGE_GIF = 21,
  IMAGE_JPEG = 22,
  IMAGE_PNG = 23,
  IMAGE_TIFF = 24,
  AUDIO_RAW = 25,
  VIDEO_RAW = 26,
  APPLICATION_LINK_FORMAT = 40,
  APPLICATION_XML = 41,
  APPLICATION_OCTET_STREAM = 42,
  APPLICATION_RDF_XML = 43,
  APPLICATION_SOAP_XML = 44,
  APPLICATION_ATOM_XML = 45,
  APPLICATION_XMPP_XML = 46,
  APPLICATION_EXI = 47,
  APPLICATION_X_BXML = 48,
  APPLICATION_FASTINFOSET = 49,
  APPLICATION_SOAP_FASTINFOSET = 50,
  APPLICATION_JSON = 51
} content_type_t;

#define REQUEST_BUFFER_SIZE 200

#define DEFAULT_CONTENT_TYPE 0
#define DEFAULT_MAX_AGE 60
#define DEFAULT_URI_HOST ""
#define DEFAULT_URI_PATH ""

#define BYTES2INT(var,bytes,len) { \
    int i = 0; \
    var = 0; \
    for (i=0; i<len; ++i) { \
      var <<= 8; \
      var |= 0xFF & bytes[i]; \
    } \
  }

#define SET_OPTION(field, opt) field |= 1<<opt
#define IS_OPTION(field, opt) field & 1<<opt

//keep open requests and their xactid

typedef union {
  struct { /* 0--14 bytes options */
    uint8_t length:4; /* option length in bytes (15 indicates long option format) */
    uint8_t delta:4;  /* option delta */
    /* 0--14 bytes options */
  } s;
  struct { /* 15-270 bytes options */
    uint8_t flag:4;   /* must be 15 */
    uint8_t delta:4;  /* option delta */
    uint8_t length;   /* length - 15 */
  } l;
} coap_header_option_t;

typedef struct {
  uint8_t oc:4;      /* Option count following the header */
  uint8_t type:2;    /* Message type  */
  uint8_t version:2; /* CoAP version */
  uint8_t code;      /* Request method (1-10) or response code (40-255) */
  uint16_t tid;      /* Transaction id */
} coap_header_t;

typedef struct {
  coap_header_t *header; /* CoAP header, also pointing to the memory to serialize packet */

  uint16_t options;  /* Bitmap to check if option is set */

  content_type_t content_type; /* Parse options once and store; allows setting options in random order  */
  uint32_t max_age;
  uint32_t etag;
  uint32_t observe; /* 0-4 bytes for coap-03 */
  uint16_t token;
  uint32_t block_num;
  uint16_t block_size;
  uint8_t block_more;

  uint8_t uri_host_len;
  char *uri_host;

  uint8_t location_path_len;
  char *location_path;

  uint8_t uri_path_len;
  char *uri_path;

  uint8_t uri_query_len;
  char *uri_query;

  uint16_t payload_len;
  char *payload;

  uint16_t url_len;
  char *url; /* for the REST framework */
} coap_packet_t;

/*error definitions*/
typedef enum
{
  NO_ERROR,

  /*Memory errors*/
  MEMORY_ALLOC_ERR,
  MEMORY_BOUNDARY_EXCEEDED
} error_t;


void coap_message_init(coap_packet_t* packet, char *buffer, uint8_t type, uint8_t code, uint16_t tid);
int coap_message_serialize(coap_packet_t* packet);
void coap_message_parse(coap_packet_t *request, char *data, uint16_t data_len);

coap_method_t coap_get_method(coap_packet_t* packet);
void coap_set_method(coap_packet_t* packet, coap_method_t method);

void coap_set_code(coap_packet_t* packet, status_code_t code);

int coap_get_query_variable(coap_packet_t* packet, const char *name, char* output, uint16_t output_size);
int coap_get_post_variable(coap_packet_t* packet, const char *name, char* output, uint16_t output_size);

content_type_t coap_get_header_content_type(coap_packet_t *packet);
int coap_set_header_content_type(coap_packet_t* packet, content_type_t content_type);

int coap_get_header_max_age(coap_packet_t* packet, uint32_t *age);
int coap_set_header_max_age(coap_packet_t* packet, uint32_t age);

int coap_get_header_etag(coap_packet_t* packet, uint32_t *etag); // FIXME included for debugging, remove as only for responses
int coap_set_header_etag(coap_packet_t* packet, uint32_t etag);

int coap_get_header_uri_host(coap_packet_t* packet, const char **host); // in-place host might not be 0-terminated

int coap_get_header_location(coap_packet_t* packet, const char **uri); // FIXME included for debugging, remove as only for responses
int coap_set_header_location(coap_packet_t* packet, char *uri);

//int coap_set_header_uri(coap_packet_t* packet, char *uri, uint16_t len); // use tokens

int coap_get_header_observe(coap_packet_t* packet, uint32_t *observe);
int coap_set_header_observe(coap_packet_t* packet, uint32_t observe);

int coap_get_header_token(coap_packet_t* packet, uint16_t *token);
int coap_set_header_token(coap_packet_t* packet, uint16_t token);

int coap_get_header_block(coap_packet_t* packet, uint32_t *num, uint8_t *more, uint16_t *size);
int coap_set_header_block(coap_packet_t* packet, uint32_t num, uint8_t more, uint16_t size);

int coap_get_payload(coap_packet_t* packet, uint8_t** payload);
int coap_set_payload(coap_packet_t* packet, uint8_t* payload, uint16_t size);

#endif /* COAP_COMMON_H_ */
