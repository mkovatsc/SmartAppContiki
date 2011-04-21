/*
 * coap-03.h
 *
 *  Created on: 12 Apr 2011
 *      Author: Matthias Kovatsch, based on Dogan Yazar's work
 */

#ifndef COAP_03_H_
#define COAP_03_H_

#if !defined(WITH_COAP) || WITH_COAP!=3
#error "### WITH_COAP MUST BE DEFINED: 3 ###"
#endif

#include "contiki-net.h"

/*
 * The number of concurrent messages that can be stored for retransmission in the transaction layer.
 */
#ifndef COAP_MAX_OPEN_TRANSACTIONS
#define COAP_MAX_OPEN_TRANSACTIONS  3
#endif /* COAP_MAX_OPEN_TRANSACTIONS */

#ifndef COAP_MAX_PACKET_SIZE /*                       0/14          48 for IPv6 (28 for IPv4) */
#define COAP_MAX_PACKET_SIZE  (UIP_BUFSIZE - UIP_LLH_LEN - UIP_IPUDPH_LEN) // 132 <- recalc on your own!
#endif /* COAP_MAX_PACKET_SIZE */

/*
 * Conservative size limit, as not all options have to be set at the same time.
 */
#ifndef COAP_MAX_PAYLOAD_SIZE /*                      Hdr CoT Age Tag Obs Tok Blo strings*/
#define COAP_MAX_PAYLOAD_SIZE  (COAP_MAX_PACKET_SIZE - 4 - 2 - 5 - 5 - 5 - 5 - 4 - 0) // 102 <- recalc on your own!
#endif /* COAP_MAX_PAYLOAD_SIZE */                  /* 30 + string options */

#define COAP_DEFAULT_MAX_AGE    60
#define COAP_RESPONSE_TIMEOUT   1
#define COAP_MAX_RETRANSMIT     5

#define COAP_DEFAULT_BLOCK_SIZE 16

#define COAP_HEADER_LEN         4 /* | oc:0xF0 type:0x0C version:0x03 | code | tid:0x00FF | tid:0xFF00 | */
#define COAP_ETAG_LEN           4 /* The maximum number of bytes for the ETag, which is 4 for coap-03 */

#define COAP_HEADER_VERSION_MASK             0xC0
#define COAP_HEADER_VERSION_POSITION         6
#define COAP_HEADER_TYPE_MASK                0x30
#define COAP_HEADER_TYPE_POSITION            4
#define COAP_HEADER_OPTION_COUNT_MASK        0x0F
#define COAP_HEADER_OPTION_COUNT_POSITION    0

#define COAP_HEADER_OPTION_DELTA_MASK        0xF0
#define COAP_HEADER_OPTION_SHORT_LENGTH_MASK 0x0F


#define SET_OPTION(packet, opt) packet->options |= 1<<opt
#define IS_OPTION(packet, opt) (packet->options & 1<<opt)


/* CoAP message types */
typedef enum {
  COAP_TYPE_CON, /* confirmables */
  COAP_TYPE_NON, /* non-confirmables */
  COAP_TYPE_ACK, /* acknowledgements */
  COAP_TYPE_RST  /* reset */
} coap_message_type_t;

/* CoAP request method codes */
typedef enum {
  COAP_GET = 1,
  COAP_POST,
  COAP_PUT,
  COAP_DELETE
} coap_method_t;

/* CoAP response codes */
typedef enum {
  CONTINUE_100 = 40,
  OK_200 = 80,
  CREATED_201 = 81,
  NOT_MODIFIED_304 = 124,
  BAD_REQUEST_400 = 160,
  NOT_FOUND_404 = 164,
  METHOD_NOT_ALLOWED_405 = 165,
  UNSUPPORTED_MADIA_TYPE_415 = 175,
  INTERNAL_SERVER_ERROR_500 = 200,
  BAD_GATEWAY_502 = 202,
  SERVICE_UNAVAILABLE_503 = 203,
  GATEWAY_TIMEOUT_504 = 204,
  TOKEN_OPTION_REQUIRED = 240,
  HOST_REQUIRED = 241,
  CRITICAL_OPTION_NOT_SUPPORTED = 242
} status_code_t;

/* CoAP header options */
typedef enum {
  COAP_OPTION_CONTENT_TYPE = 1,  /* 1 B */
  COAP_OPTION_MAX_AGE = 2,       /* 1-4 B */
  COAP_OPTION_ETAG = 4,          /* 1-4 B */
  COAP_OPTION_URI_HOST = 5,      /* 1-270 B */
  COAP_OPTION_LOCATION_PATH = 6, /* 1-270 B */
  COAP_OPTION_URI_PATH = 9,      /* 1-270 B */
  COAP_OPTION_OBSERVE = 10,      /* 0-4 B */
  COAP_OPTION_TOKEN = 11,        /* 1-2 B */
  COAP_OPTION_BLOCK = 13,        /* 1-3 B */
  COAP_OPTION_NOOP = 14,         /* 0 B */
  COAP_OPTION_URI_QUERY = 15     /* 1-270 B */
} coap_option_t;

/* CoAP content-types */
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

typedef union {
  struct { /* 0--14 bytes options */
    uint8_t length:4; /* option length in bytes (15 indicates long option format) */
    uint8_t delta:4;  /* option delta */
    /* 0--14 bytes options */
  } s;
  struct { /* 15-270 bytes options */
    uint16_t flag:4;   /* must be 15 */
    uint16_t delta:4;  /* option delta */
    uint16_t length:8;   /* length - 15 */
  } l;
} coap_header_option_t;

typedef struct {
  uint8_t *header; /* pointer to CoAP header / incoming packet buffer / memory to serialize packet */

  uint8_t version;
  coap_message_type_t type;
  uint8_t option_count;
  uint8_t code;
  uint16_t tid;

  uint16_t options;  /* Bitmap to check if option is set */

  content_type_t content_type; /* Parse options once and store; allows setting options in random order  */
  uint32_t max_age;
  uint8_t etag_len;
  uint8_t etag[COAP_ETAG_LEN];
  uint8_t uri_host_len;
  char *uri_host;
  uint8_t location_path_len;
  char *location_path;
  uint8_t uri_path_len;
  char *uri_path;
  uint32_t observe; /* 0-4 bytes for coap-03 */
  uint16_t token;
  uint32_t block_num;
  uint8_t block_more;
  uint16_t block_size;
  uint32_t block_offset;
  uint8_t uri_query_len;
  char *uri_query;

  uint16_t payload_len;
  uint8_t *payload;

  uint16_t url_len;
  char *url; /* for the REST framework */
} coap_packet_t;

/*error definitions*/
typedef enum
{
  NO_ERROR,

  /* Memory errors */
  MEMORY_ALLOC_ERR,
  MEMORY_BOUNDARY_EXCEEDED,

  /* CoAP errors */
  UNKNOWN_CRITICAL_OPTION
} error_t;

void coap_init_connection(uint16_t port);
void coap_send_message(uip_ipaddr_t *addr, uint16_t port, uint8_t *data, uint16_t length);

void coap_init_message(coap_packet_t *packet, uint8_t *buffer, coap_message_type_t type, uint8_t code, uint16_t tid);
int coap_serialize_message(coap_packet_t *packet);
error_t coap_parse_message(coap_packet_t *request, uint8_t *data, uint16_t data_len);

coap_method_t coap_get_method(coap_packet_t *packet);
void coap_set_method(coap_packet_t *packet, coap_method_t method);
void coap_set_code(coap_packet_t *packet, status_code_t code);

int coap_get_query_variable(coap_packet_t *packet, const char *name, char *output, uint16_t output_size);
int coap_get_post_variable(coap_packet_t *packet, const char *name, char *output, uint16_t output_size);

content_type_t coap_get_header_content_type(coap_packet_t *packet);
int coap_set_header_content_type(coap_packet_t *packet, content_type_t content_type);

int coap_get_header_max_age(coap_packet_t *packet, uint32_t *age);
int coap_set_header_max_age(coap_packet_t *packet, uint32_t age);

int coap_get_header_etag(coap_packet_t *packet, const uint8_t **etag);
int coap_set_header_etag(coap_packet_t *packet, uint8_t *etag, uint8_t etag_len);

int coap_get_header_uri_host(coap_packet_t *packet, const char **host); // in-place string might not be 0-terminated
int coap_set_header_uri_host(coap_packet_t *packet, char *host);

int coap_get_header_location(coap_packet_t *packet, const char **uri); // in-place string might not be 0-terminated
int coap_set_header_location(coap_packet_t *packet, char *uri);

int coap_get_header_uri_path(coap_packet_t *packet, const char **uri); // in-place string might not be 0-terminated
int coap_set_header_uri_path(coap_packet_t *packet, char *uri);

int coap_get_header_observe(coap_packet_t *packet, uint32_t *observe);
int coap_set_header_observe(coap_packet_t *packet, uint32_t observe);

int coap_get_header_token(coap_packet_t *packet, uint16_t *token);
int coap_set_header_token(coap_packet_t *packet, uint16_t token);

int coap_get_header_block(coap_packet_t *packet, uint32_t *num, uint8_t *more, uint16_t *size, uint32_t *offset);
int coap_set_header_block(coap_packet_t *packet, uint32_t num, uint8_t more, uint16_t size);

int coap_get_header_uri_query(coap_packet_t *packet, const char **query); // in-place string might not be 0-terminated
int coap_set_header_uri_query(coap_packet_t *packet, char *query);

int coap_get_payload(coap_packet_t *packet, uint8_t* *payload);
int coap_set_payload(coap_packet_t *packet, uint8_t *payload, uint16_t size);

#endif /* COAP_03_H_ */
