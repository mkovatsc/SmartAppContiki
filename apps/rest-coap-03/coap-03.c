/*
 * coap-03.h
 *
 *  Created on: 12 Apr 2011
 *      Author: Matthias Kovatsch, based on Dogan Yazar's work
 */

#ifdef CONTIKI_TARGET_NETSIM
  #include <stdio.h>
  #include <iostream>
  #include <cstring>
  #include <cstdlib>
  #include <unistd.h>
  #include <errno.h>
  #include <string.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#else
  #include "contiki.h"
  #include "contiki-net.h"
  #include <string.h>
  #include <stdio.h>
#endif

#include "rest-util.h"
#include "coap-03.h"
#include "coap-transactions.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif


static struct uip_udp_conn *udp_conn = NULL;

/*-----------------------------------------------------------------------------------*/
/*- LOCAL HELP FUNCTIONS ------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
static
uint32_t
bytes_2_uint32(uint8_t *bytes, uint16_t length)
{
  uint32_t var = 0;
  int i = 0;
  while (i<length)
  {
    var <<= 8;
    var |= 0xFF & bytes[i++];
  }
  return var;
}
/*-----------------------------------------------------------------------------------*/
static
int
uint16_2_bytes(uint8_t *bytes, uint16_t var)
{
  int i = 0;
  if (0xFF00 & var) bytes[i++] = var>>8;
  bytes[i++] = 0xFF & var;

  return i;
}
/*-----------------------------------------------------------------------------------*/
static
int
uint32_2_bytes(uint8_t *bytes, uint32_t var)
{
  int i = 0;
  if (0xFF000000 & var) bytes[i++] = var>>24;
  if (0xFF0000 & var) bytes[i++] = var>>16;
  if (0xFF00 & var) bytes[i++] = var>>8;
  bytes[i++] = 0xFF & var;

  return i;
}
/*-----------------------------------------------------------------------------------*/
/*- MEASSAGE SENDING ----------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
void
coap_init_connection(uint16_t port)
{
  /* new connection with remote host */
  udp_conn = udp_new(NULL, uip_htons(0), NULL);
  udp_bind(udp_conn, uip_htons(port));
  PRINTF("Local/remote port %u/%u\n", uip_htons(udp_conn->lport), uip_htons(udp_conn->rport));
}
/*-----------------------------------------------------------------------------------*/
void
coap_send_message(uip_ipaddr_t *addr, uint16_t port, uint8_t *data, uint16_t length)
{
  /*configure connection to reply to client*/
  uip_ipaddr_copy(&udp_conn->ripaddr, addr);
  udp_conn->rport = port;

  uip_udp_packet_send(udp_conn, data, length);
  PRINTF("-sent UDP datagram------\n Length: %u\n -----------------------\n", length);

  /* Restore server connection to allow data from any node */
  memset(&udp_conn->ripaddr, 0, sizeof(udp_conn->ripaddr));
  udp_conn->rport = 0;
}
/*-----------------------------------------------------------------------------------*/
/*- MEASSAGE PROCESSING -------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
void
coap_init_message(coap_packet_t *packet, uint8_t *buffer, coap_message_type_t type, uint8_t code, uint16_t tid)
{
  memset(packet, 0, sizeof(coap_packet_t));

  packet->header = buffer;

  packet->version = 1;
  packet->type = type;
  packet->option_count = 0;
  packet->code = code;
  packet->tid = tid;

  /* set header fields */
  /*
  packet->header[0]  = 0x00;
  packet->header[0] |= COAP_HEADER_VERSION_MASK & (packet->version)<<COAP_HEADER_VERSION_POSITION;
  packet->header[0] |= COAP_HEADER_TYPE_MASK & (packet->type)<<COAP_HEADER_TYPE_POSITION;
  packet->header[0] |= COAP_HEADER_OPTION_COUNT_MASK & (packet->option_count)<<COAP_HEADER_OPTION_COUNT_POSITION;
  packet->header[1] = packet->code;
  packet->header[2] = 0xFF & packet->tid;
  packet->header[3] = 0xFF & (packet->tid)>>8;
  */
}
/*-----------------------------------------------------------------------------------*/
int
coap_serialize_message(coap_packet_t *packet)
{
  /* serialize options */
  uint8_t *option = packet->header + COAP_HEADER_LEN;
  int option_len = 0;
  int index = 0;

  if (IS_OPTION(packet, COAP_OPTION_CONTENT_TYPE)) {
    ((coap_header_option_t *)option)->s.delta = 1;
    ((coap_header_option_t *)option)->s.length = 1;
    *(++option) = packet->content_type;
    PRINTF("OPTION %u (type %u, len 1, delta %u): ", packet->option_count, COAP_OPTION_CONTENT_TYPE, COAP_OPTION_CONTENT_TYPE - index);
    PRINTF("Content-Type [%u]\n", packet->content_type);
    index = COAP_OPTION_CONTENT_TYPE;
    option += 1;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_MAX_AGE)) {
    option_len = uint32_2_bytes(option+1, packet->max_age);
    ((coap_header_option_t *)option)->s.delta = COAP_OPTION_MAX_AGE - index;
    ((coap_header_option_t *)option)->s.length = option_len;
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_MAX_AGE, option_len, COAP_OPTION_MAX_AGE - index);
    PRINTF("Max-Age [%lu]\n", packet->max_age);
    index = COAP_OPTION_MAX_AGE;
    option += 1 + option_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_ETAG)) {
    ((coap_header_option_t *)option)->s.delta = COAP_OPTION_ETAG - index;
    ((coap_header_option_t *)option)->s.length = packet->etag_len;
    memcpy(++option, packet->etag, packet->etag_len);
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_ETAG, packet->etag_len, COAP_OPTION_ETAG - index);
    PRINTF("ETag %u [0x%02X", packet->etag_len, packet->etag[0]); // FIXME always prints 4 bytes...
    PRINTF("%02X", packet->etag[1]);
    PRINTF("%02X", packet->etag[2]);
    PRINTF("%02X", packet->etag[3]);
    PRINTF("]\n");
    index = COAP_OPTION_ETAG;
    option += packet->etag_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_URI_HOST)) {
    if (packet->uri_host_len<15) {
      ((coap_header_option_t *)option)->s.delta = COAP_OPTION_URI_HOST - index;
      ((coap_header_option_t *)option)->s.length = packet->uri_host_len;
      option += 1;
    } else {
      ((coap_header_option_t *)option)->l.delta = COAP_OPTION_URI_HOST - index;
      ((coap_header_option_t *)option)->l.length = packet->uri_host_len - 15;
      option += 2;
    }
    memcpy(option, packet->uri_host, packet->uri_host_len);
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_URI_HOST, packet->uri_host_len, COAP_OPTION_URI_HOST - index);
    PRINTF("Uri-Auth [%.*s]\n", packet->uri_host_len, packet->uri_host);
    index = COAP_OPTION_URI_HOST;
    option += packet->uri_host_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_LOCATION_PATH)) {
    if (packet->location_path_len<15) {
      ((coap_header_option_t *)option)->s.delta = COAP_OPTION_LOCATION_PATH - index;
      ((coap_header_option_t *)option)->s.length = packet->location_path_len;
      option += 1;
    } else {
      ((coap_header_option_t *)option)->l.delta = COAP_OPTION_LOCATION_PATH - index;
      ((coap_header_option_t *)option)->l.length = packet->location_path_len - 15;
      option += 2;
    }
    memcpy(option, packet->location_path, packet->location_path_len);
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_LOCATION_PATH, packet->location_path_len, COAP_OPTION_LOCATION_PATH - index);
    PRINTF("Location [%.*s]\n", packet->location_path_len, packet->location_path);
    index = COAP_OPTION_LOCATION_PATH;
    option += packet->location_path_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_URI_PATH)) {
    if (packet->uri_path_len<15) {
      ((coap_header_option_t *)option)->s.delta = COAP_OPTION_URI_PATH - index;
      ((coap_header_option_t *)option)->s.length = packet->uri_path_len;
      option += 1;
    } else {
      ((coap_header_option_t *)option)->l.delta = COAP_OPTION_URI_PATH - index;
      ((coap_header_option_t *)option)->l.length = packet->uri_path_len - 15;
      option += 2;
    }
    memcpy(option, packet->uri_path, packet->uri_path_len);
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_URI_PATH, packet->uri_path_len, COAP_OPTION_URI_PATH - index);
    PRINTF("Uri-Path [%.*s]\n", packet->uri_path_len, packet->uri_path);
    index = COAP_OPTION_URI_PATH;
    option += packet->uri_path_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_OBSERVE)) {
    option_len = uint32_2_bytes(option+1, packet->observe);
    ((coap_header_option_t *)option)->s.delta = COAP_OPTION_OBSERVE - index;
    ((coap_header_option_t *)option)->s.length = option_len;
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_OBSERVE, option_len, COAP_OPTION_OBSERVE - index);
    PRINTF("Observe [%lu]\n", packet->observe);
    index = COAP_OPTION_OBSERVE;
    option += 1 + option_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_TOKEN)) {
    ((coap_header_option_t *)option)->s.delta = COAP_OPTION_TOKEN - index;
    ((coap_header_option_t *)option)->s.length = packet->token_len;
    memcpy(++option, packet->token, packet->token_len);
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_TOKEN, packet->token_len, COAP_OPTION_TOKEN - index);
    PRINTF("Token %u [0x%02X%02X]\n", packet->token_len, packet->token[0], packet->token[1]); // FIXME always prints 2 bytes...
    index = COAP_OPTION_TOKEN;
    option += packet->token_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_BLOCK)) {
    uint32_t block = packet->block_num << 4;
    if (packet->block_more) block |= 0x8;
    block |= 0xF & log_2(packet->block_size/16);
    option_len = uint32_2_bytes(option+1, block);
    ((coap_header_option_t *)option)->s.delta = COAP_OPTION_BLOCK - index;
    ((coap_header_option_t *)option)->s.length = option_len;
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_BLOCK, option_len, COAP_OPTION_BLOCK - index);
    PRINTF("Block [%lu%s (%u B/blk)]\n", packet->block_num, packet->block_more ? "+" : "", packet->block_size);
    index = COAP_OPTION_BLOCK;
    option += 1 + option_len;
    ++(packet->option_count);
  }
  if (IS_OPTION(packet, COAP_OPTION_URI_QUERY)) {
    if (packet->uri_query_len<15) {
      ((coap_header_option_t *)option)->s.delta = COAP_OPTION_URI_QUERY - index;
      ((coap_header_option_t *)option)->s.length = packet->uri_query_len;
      option += 1;
    } else {
      ((coap_header_option_t *)option)->l.delta = COAP_OPTION_URI_QUERY - index;
      ((coap_header_option_t *)option)->l.length = packet->uri_query_len - 15;
      option += 2;
    }
    memcpy(option, packet->uri_query, packet->uri_query_len);
    PRINTF("OPTION %u (type %u, len %u, delta %u): ", packet->option_count, COAP_OPTION_URI_QUERY, packet->uri_query_len, COAP_OPTION_URI_QUERY - index);
    PRINTF("Uri-Query [%.*s]\n", packet->uri_query_len, packet->uri_query);
    index = COAP_OPTION_URI_QUERY;
    option += packet->uri_query_len;
    ++(packet->option_count);
  }

  /* pack payload */
  if ((option - packet->header)<=COAP_MAX_HEADER_SIZE)
  {
    memmove(option, packet->payload, packet->payload_len);
  }
  else
  {
    packet->code = INTERNAL_SERVER_ERROR_500;
    packet->payload_len = (uint32_t) snprintf((char *)packet->header + COAP_HEADER_LEN, REST_MAX_CHUNK_SIZE+1, "Header (4) and options (%u) exceed COAP_MAX_HEADER_SIZE", (option - packet->header));
  }

  /* set header fields */
  packet->header[0]  = 0x00;
  packet->header[0] |= COAP_HEADER_VERSION_MASK & (packet->version)<<COAP_HEADER_VERSION_POSITION;
  packet->header[0] |= COAP_HEADER_TYPE_MASK & (packet->type)<<COAP_HEADER_TYPE_POSITION;
  packet->header[0] |= COAP_HEADER_OPTION_COUNT_MASK & (packet->option_count)<<COAP_HEADER_OPTION_COUNT_POSITION;
  packet->header[1] = packet->code;
  packet->header[2] = 0xFF & (packet->tid)>>8;
  packet->header[3] = 0xFF & packet->tid;

  PRINTF("Serialized %u options, header len %u, payload len %u\n", packet->option_count, option - packet->header, packet->payload_len);

  return (option - packet->header) + packet->payload_len; /* packet length */
}
/*-----------------------------------------------------------------------------------*/
error_t
coap_parse_message(coap_packet_t *packet, uint8_t *data, uint16_t data_len)
{
  /* pointer to packet bytes */
  packet->header = data;

  /* parse header fields */
  packet->version = (COAP_HEADER_VERSION_MASK & packet->header[0])>>COAP_HEADER_VERSION_POSITION;
  packet->type = (COAP_HEADER_TYPE_MASK & packet->header[0])>>COAP_HEADER_TYPE_POSITION;
  packet->option_count = (COAP_HEADER_OPTION_COUNT_MASK & packet->header[0])>>COAP_HEADER_OPTION_COUNT_POSITION;
  packet->code = packet->header[1];
  packet->tid = packet->header[2]<<8 | packet->header[3];

  /* parse options */
  packet->options = 0x0000;
  coap_header_option_t *current_option = (coap_header_option_t *) (data + COAP_HEADER_LEN);

  if (packet->option_count) {
    uint8_t option_index = 0;
    uint8_t option_type = 0;

    uint16_t option_len = 0;
    uint8_t *option_data = NULL;

    uint8_t *last_option = NULL;

    for (option_index=0; option_index < packet->option_count; ++option_index) {

      option_type += current_option->s.delta;

      if (current_option->s.length<15) {
        option_len = current_option->s.length;
        option_data = ((uint8_t *) current_option) + 1;
      } else {
        option_len = current_option->l.length + 15;
        option_data = ((uint8_t *) current_option) + 2;
      }

      PRINTF("OPTION %u (type %u, len %u, delta %u): ", option_index, option_type, option_len, current_option->s.delta);

      SET_OPTION(packet, option_type);

      switch (option_type) {
        case COAP_OPTION_CONTENT_TYPE:
          packet->content_type = option_data[0];
          PRINTF("Content-Type [%u]\n", packet->content_type);
          break;
        case COAP_OPTION_MAX_AGE:
          packet->max_age = bytes_2_uint32(option_data, option_len);
          PRINTF("Max-Age [%lu]\n", packet->max_age);
          break;
        case COAP_OPTION_ETAG:
          packet->etag_len = MIN(COAP_ETAG_LEN, option_len);
          memcpy(packet->etag, option_data, packet->etag_len);
          PRINTF("ETag %u [0x%02X", packet->etag_len, packet->etag[0]); // FIXME always prints 4 bytes...
          PRINTF("%02X", packet->etag[1]);
          PRINTF("%02X", packet->etag[2]);
          PRINTF("%02X", packet->etag[3]);
          PRINTF("]\n");
          break;
        case COAP_OPTION_URI_HOST:
          packet->uri_host = (char *) option_data;
          packet->uri_host_len = option_len;
          PRINTF("Uri-Auth [%.*s]\n", packet->uri_host_len, packet->uri_host);
          break;
        case COAP_OPTION_LOCATION_PATH:
          packet->location_path = (char *) option_data;
          packet->location_path_len = option_len;
          PRINTF("Location [%.*s]\n", packet->location_path_len, packet->location_path);
          break;
        case COAP_OPTION_URI_PATH:
          packet->uri_path = (char *) option_data;
          packet->uri_path_len = option_len;
          PRINTF("Uri-Path [%.*s]\n", packet->uri_path_len, packet->uri_path);

          // REST framework uses ->url
          packet->url = packet->uri_path;
          packet->url_len = packet->uri_path_len;
          break;
        case COAP_OPTION_OBSERVE:
          packet->observe = bytes_2_uint32(option_data, option_len);
          PRINTF("Observe [%lu]\n", packet->observe);
          break;
        case COAP_OPTION_TOKEN:
          packet->token_len = MIN(COAP_TOKEN_LEN, option_len);
          memcpy(packet->token, option_data, packet->token_len);
          PRINTF("Token %u [0x%02X%02X]\n", packet->token_len, packet->token[0] packet->token[1]); // FIXME always prints 2 bytes...
          break;
        case COAP_OPTION_BLOCK:
          packet->block_num = bytes_2_uint32(option_data, option_len);
          packet->block_more = (packet->block_num & 0x08);
          packet->block_size = 16 << (packet->block_num & 0x07);
          packet->block_offset = (packet->block_num & ~0x0F)<<(packet->block_num & 0x07);
          packet->block_num >>= 4;
          PRINTF("Block [%lu%s (%u B/blk)]\n", packet->block_num, packet->block_more ? "+" : "", packet->block_size);
          break;
        case COAP_OPTION_NOOP:
          PRINTF("Noop-Fencepost\n");
          break;
        case COAP_OPTION_URI_QUERY:
          packet->uri_query = (char *) option_data;
          packet->uri_query_len = option_len;
          PRINTF("Uri-Query [%.*s]\n", packet->uri_query_len, packet->uri_query);
          break;
        default:
          PRINTF("unknown (%u)\n", option_type);
          if (option_type & 1)
          {
            return UNKNOWN_CRITICAL_OPTION;
          }
      }

      /* terminate strings where possible */
      if (last_option) {
        last_option[0] = 0x00;
      }

      last_option = (uint8_t *) current_option;
      current_option = (coap_header_option_t *) (option_data+option_len);
    } /* for () */
  } /* if (oc) */

  packet->payload = (uint8_t *) current_option;
  packet->payload_len = data_len - (packet->payload - data);

  return NO_ERROR;
}
/*-----------------------------------------------------------------------------------*/
/*- REST FRAMEWORK FUNCTIONS --------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
coap_method_t
coap_get_method(coap_packet_t *packet)
{
  return (coap_method_t) packet->code;
}

void
coap_set_method(coap_packet_t *packet, coap_method_t method)
{
  packet->code = (uint8_t)method;
}
/*-----------------------------------------------------------------------------------*/
void
coap_set_code(coap_packet_t *packet, status_code_t code)
{
  packet->code = (uint8_t)code;
}
/*-----------------------------------------------------------------------------------*/
// FIXME return in-place pointer to save memory
int
coap_get_query_variable(coap_packet_t *packet, const char *name, char *output, uint16_t output_size)
{
  if (IS_OPTION(packet, COAP_OPTION_URI_QUERY)) {
    return get_variable(name, packet->uri_query, packet->uri_query_len, output, output_size, 0);
  }

  return 0;
}

// FIXME return in-place pointer to save memory
int
coap_get_post_variable(coap_packet_t *packet, const char *name, char *output, uint16_t output_size)
{
  if (packet->payload_len) {
      return get_variable(name, (const char *)packet->payload, packet->payload_len, output, output_size, 0);
    }

    return 0;
}
/*-----------------------------------------------------------------------------------*/
/*- HEADER OPTION GETTERS AND SETTERS -----------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
content_type_t
coap_get_header_content_type(coap_packet_t *packet)
{
  return packet->content_type;
}

int
coap_set_header_content_type(coap_packet_t *packet, content_type_t content_type)
{
  packet->content_type = (uint8_t) content_type;
  SET_OPTION(packet, COAP_OPTION_CONTENT_TYPE);
  return 1;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_max_age(coap_packet_t *packet, uint32_t *age)
{
  if (!IS_OPTION(packet, COAP_OPTION_MAX_AGE)) {
    *age = COAP_DEFAULT_MAX_AGE;
  } else {
    *age = packet->max_age;
  }
  return 1;
}

int coap_set_header_max_age(coap_packet_t *packet, uint32_t age)
{
  packet->max_age = age;
  SET_OPTION(packet, COAP_OPTION_MAX_AGE);
  return 1;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_etag(coap_packet_t *packet, const uint8_t **etag)
{
  if (!IS_OPTION(packet, COAP_OPTION_ETAG)) return 0;

  *etag = packet->etag;
  return packet->etag_len;
}

int
coap_set_header_etag(coap_packet_t *packet, uint8_t *etag, uint8_t etag_len)
{
  packet->etag_len = MIN(COAP_ETAG_LEN, etag_len);
  memcpy(packet->etag, etag, packet->etag_len);

  SET_OPTION(packet, COAP_OPTION_ETAG);
  return packet->etag_len;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_uri_host(coap_packet_t *packet, const char **host) // in-place host might not be 0-terminated
{
  if (!IS_OPTION(packet, COAP_OPTION_URI_HOST)) return 0;

  *host = packet->uri_host;
  return packet->uri_host_len;
}

int
coap_set_header_uri_host(coap_packet_t *packet, char *uri)
{
  packet->uri_host = uri;
  packet->uri_host_len = strlen(uri);

  SET_OPTION(packet, COAP_OPTION_URI_HOST);
  return packet->uri_host_len;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_location(coap_packet_t *packet, const char **uri) // in-place uri might not be 0-terminated
{
  if (!IS_OPTION(packet, COAP_OPTION_LOCATION_PATH)) return 0;

  *uri = packet->location_path;
  return packet->location_path_len;
}

int
coap_set_header_location(coap_packet_t *packet, char *uri)
{
  while (uri[0]=='/') ++uri;

  packet->location_path = uri;
  packet->location_path_len = strlen(uri);

  SET_OPTION(packet, COAP_OPTION_LOCATION_PATH);
  return packet->location_path_len;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_uri_path(coap_packet_t *packet, const char **uri)
{
  if (!IS_OPTION(packet, COAP_OPTION_URI_PATH)) return 0;

  *uri = packet->uri_path;
  return packet->uri_path_len;
}

int
coap_set_header_uri_path(coap_packet_t *packet, char *uri)
{
  while (uri[0]=='/') ++uri;

  packet->uri_path = uri;
  packet->uri_path_len = strlen(uri);

  SET_OPTION(packet, COAP_OPTION_URI_PATH);
  return packet->uri_path_len;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_observe(coap_packet_t *packet, uint32_t *observe)
{
  if (!IS_OPTION(packet, COAP_OPTION_OBSERVE)) return 0;

  *observe = packet->observe;
  return 1;
}

int
coap_set_header_observe(coap_packet_t *packet, uint32_t observe)
{
  packet->observe = observe;
  SET_OPTION(packet, COAP_OPTION_OBSERVE);
  return 1;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_token(coap_packet_t *packet, const uint8_t **token)
{
  if (!IS_OPTION(packet, COAP_OPTION_TOKEN)) return 0;

  *token = packet->token;
  return packet->token_len;
}

int
coap_set_header_token(coap_packet_t *packet, uint8_t *token, uint8_t token_len)
{
  packet->token_len = MIN(COAP_TOKEN_LEN, token_len);
  memcpy(packet->token, token, packet->token_len);

  SET_OPTION(packet, COAP_OPTION_TOKEN);
  return packet->token_len;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_block(coap_packet_t *packet, uint32_t *num, uint8_t *more, uint16_t *size, uint32_t *offset)
{
  if (!IS_OPTION(packet, COAP_OPTION_BLOCK)) return 0;

  /* pointers may be NULL to get only specific block parameters */
  if (num!=NULL) *num = packet->block_num;
  if (more!=NULL) *more = packet->block_more;
  if (size!=NULL) *size = packet->block_size;
  if (offset!=NULL) *offset = packet->block_offset;

  return 1;
}

int
coap_set_header_block(coap_packet_t *packet, uint32_t num, uint8_t more, uint16_t size)
{
  if (size<16) return 0;
  if (size>2048) return 0;
  if (num>0x0FFFFF) return 0;

  packet->block_num = num;
  packet->block_more = more;
  packet->block_size = size;

  SET_OPTION(packet, COAP_OPTION_BLOCK);
  return 1;
}
/*-----------------------------------------------------------------------------------*/
int
coap_get_header_uri_query(coap_packet_t *packet, const char **query)
{
  if (!IS_OPTION(packet, COAP_OPTION_URI_QUERY)) return 0;

  *query = packet->uri_query;
  return packet->uri_query_len;
}

int
coap_set_header_uri_query(coap_packet_t *packet, char *query)
{
  while (query[0]=='?') ++query;

  packet->uri_query = query;
  packet->uri_query_len = strlen(query);

  SET_OPTION(packet, COAP_OPTION_URI_QUERY);
  return packet->uri_query_len;
}
/*-----------------------------------------------------------------------------------*/
/*- PAYLOAD -------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
int
coap_get_payload(coap_packet_t *packet, uint8_t **payload)
{
  if (packet->payload) {
    *payload = packet->payload;
    return packet->payload_len;
  } else {
    *payload = NULL;
    return 0;
  }
}

int
coap_set_payload(coap_packet_t *packet, uint8_t *payload, uint16_t size)
{
  packet->payload = payload;

  PRINTF("setting payload (%u/%u)\n", size, REST_MAX_CHUNK_SIZE);

  if (size <= REST_MAX_CHUNK_SIZE) {
    packet->payload_len = size;
  } else {
    packet->payload_len = REST_MAX_CHUNK_SIZE;
  }

  return packet->payload_len;
}
/*-----------------------------------------------------------------------------------*/
