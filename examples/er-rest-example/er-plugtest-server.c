/*
 * Copyright (c) 2012, Matthias Kovatsch and other contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Server for the ETSI IoT CoAP Plugtests, Paris, France, 24 - 25 March 2012
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#define MAX_PLUGFEST_PAYLOAD 64+1 /* +1 for the terminating zero, which is not transmitted */

/* Define which resources to include to meet memory constraints. */
#define REST_RES_TEST 1
#define REST_RES_LONG 1
#define REST_RES_QUERY 1
#define REST_RES_SEPARATE 1
#define REST_RES_LARGE 1
#define REST_RES_LARGE_UPDATE 1
#define REST_RES_LARGE_CREATE 1
#define REST_RES_OBS 1

#define REST_RES_MIRROR 1



#if !defined (CONTIKI_TARGET_MINIMAL_NET)
#warning "Should only be compiled for minimal-net!"
#endif


#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET) && !defined (CONTIKI_TARGET_NATIVE)
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

#include "erbium.h"

/* For CoAP-specific example: not required for normal RESTful Web service. */
#if WITH_COAP==7
#include "er-coap-07.h"
#elif WITH_COAP == 12
#include "er-coap-12.h"
#else
#error "Plugtests server without CoAP"
#endif /* CoAP-specific example */

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif


#if REST_RES_TEST
/*
 * Default test resource
 */
RESOURCE(test, METHOD_GET|METHOD_POST|METHOD_PUT|METHOD_DELETE, "test", "title=\"Default test resource\"");

void
test_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_packet_t *const coap_req = (coap_packet_t *) request;

  uint8_t method = REST.get_method_type(request);

  PRINTF("/test           ");
  if (method & METHOD_GET)
  {
    PRINTF("GET ");
    /* Code 2.05 CONTENT is default. */
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, buffer, snprintf((char *)buffer, MAX_PLUGFEST_PAYLOAD, "Type: %u\nCode: %u\nMID: %u", coap_req->type, coap_req->code, coap_req->mid));
  }
  else if (method & METHOD_POST)
  {
    PRINTF("POST ");
    REST.set_response_status(response, REST.status.CREATED);
    REST.set_header_location(response, "/nirvana");
  }
  else if (method & METHOD_PUT)
  {
    PRINTF("PUT ");
    REST.set_response_status(response, REST.status.CHANGED);
  }
  else if (method & METHOD_DELETE)
  {
    PRINTF("DELETE ");
    REST.set_response_status(response, REST.status.DELETED);
  }

  PRINTF("(%s %u)\n", coap_req->type==COAP_TYPE_CON?"CON":"NON", coap_req->mid);
}
#endif

#if REST_RES_LONG
/*
 * Long path resource
 */
RESOURCE(longpath, METHOD_GET, "seg1/seg2/seg3", "title=\"Long path resource\"");

void
longpath_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_packet_t *const coap_req = (coap_packet_t *) request;

  uint8_t method = REST.get_method_type(request);

  PRINTF("/seg1/seg2/seg3 ");
  if (method & METHOD_GET)
  {
    PRINTF("GET ");
    /* Code 2.05 CONTENT is default. */
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, buffer, snprintf((char *)buffer, MAX_PLUGFEST_PAYLOAD, "Type: %u\nCode: %u\nMID: %u", coap_req->type, coap_req->code, coap_req->mid));
  }
  PRINTF("(%s %u)\n", coap_req->type==COAP_TYPE_CON?"CON":"NON", coap_req->mid);
}
#endif

#if REST_RES_QUERY
/*
 * Resource accepting query parameters
 */
RESOURCE(query, METHOD_GET, "query", "title=\"Resource accepting query parameters\"");

void
query_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_packet_t *const coap_req = (coap_packet_t *) request;
  int len = 0;
  const char *query = NULL;

  PRINTF("/query          GET (%s %u)\n", coap_req->type==COAP_TYPE_CON?"CON":"NON", coap_req->mid);

  if ((len = REST.get_query(request, &query)))
  {
    PRINTF("Query: %.*s\n", len, query);
  }

  /* Code 2.05 CONTENT is default. */
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_response_payload(response, buffer, snprintf((char *)buffer, MAX_PLUGFEST_PAYLOAD, "Type: %u\nCode: %u\nMID: %u\nQuery: %.*s", coap_req->type, coap_req->code, coap_req->mid, len, query));
}
#endif

#if REST_RES_SEPARATE
/* Required to manually (=not by the engine) handle the response transaction. */
#include "er-coap-12-separate.h"
#include "er-coap-12-transactions.h"
/*
 * Resource which cannot be served immediately and which cannot be acknowledged in a piggy-backed way
 */
PERIODIC_RESOURCE(separate, METHOD_GET, "separate", "title=\"Resource which cannot be served immediately and which cannot be acknowledged in a piggy-backed way\"", 3*CLOCK_SECOND);

/* A structure to store the required information */
typedef struct application_separate_store {
  /* Provided by Erbium to store generic request information such as remote address and token. */
  coap_separate_t request_metadata;
  /* Add fields for addition information to be stored for finalizing, e.g.: */
  char buffer[MAX_PLUGFEST_PAYLOAD];
} application_separate_store_t;

static uint8_t separate_active = 0;
static application_separate_store_t separate_store[1];

void
separate_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_packet_t *const coap_req = (coap_packet_t *) request;

  PRINTF("/separate       ");
  if (separate_active)
  {
    PRINTF("REJECTED ");
    coap_separate_reject();
  }
  else
  {
    PRINTF("STORED ");
    separate_active = 1;

    /* Take over and skip response by engine. */
    coap_separate_accept(request, &separate_store->request_metadata);
    /* Be aware to respect the Block2 option, which is also stored in the coap_separate_t. */

    snprintf(separate_store->buffer, MAX_PLUGFEST_PAYLOAD, "Type: %u\nCode: %u\nMID: %u", coap_req->type, coap_req->code, coap_req->mid);
  }

  PRINTF("(%s %u)\n", coap_req->type==COAP_TYPE_CON?"CON":"NON", coap_req->mid);
}

void
separate_periodic_handler(resource_t *resource)
{
  if (separate_active)
  {
    PRINTF("/separate       ");
    coap_transaction_t *transaction = NULL;
    if ( (transaction = coap_new_transaction(separate_store->request_metadata.mid, &separate_store->request_metadata.addr, separate_store->request_metadata.port)) )
    {
      PRINTF("RESPONSE (%s %u)\n", separate_store->request_metadata.type==COAP_TYPE_CON?"CON":"NON", separate_store->request_metadata.mid);

      coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */

      /* Restore the request information for the response. */
      coap_separate_resume(response, &separate_store->request_metadata, CONTENT_2_05);

      REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
      coap_set_payload(response, separate_store->buffer, strlen(separate_store->buffer));

      /*
       * Be aware to respect the Block2 option, which is also stored in the coap_separate_t.
       * As it is a critical option, this example resource pretends to handle it for compliance.
       */
      coap_set_header_block2(response, separate_store->request_metadata.block2_num, 0, separate_store->request_metadata.block2_size);

      /* Warning: No check for serialization error. */
      transaction->packet_len = coap_serialize_message(response, transaction->packet);
      coap_send_transaction(transaction);
      /* The engine will clear the transaction (right after send for NON, after acked for CON). */

      separate_active = 0;
    } else {
      PRINTF("ERROR (transaction)\n");
    }
  } /* if (separate_active) */
}
#endif

#if REST_RES_LARGE
/*
 * Large resource
 */
RESOURCE(large, METHOD_GET, "large", "title=\"Large resource\";rt=\"block\"");

#define CHUNKS_TOTAL    1280

void
large_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int32_t strpos = 0;

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    /* A block error message should not exceed the minimum block size (16). */

    const char *error_msg = "BlockOutOfScope";
    REST.set_response_payload(response, error_msg, strlen(error_msg));
    return;
  }

  /* Generate data until reaching CHUNKS_TOTAL. */
  while (strpos<preferred_size)
  {
    strpos += snprintf((char *)buffer+strpos, preferred_size-strpos+1, "|%ld|", *offset);
  }

  /* snprintf() does not adjust return value if truncated by size. */
  if (strpos > preferred_size)
  {
    strpos = preferred_size;
  }

  /* Truncate if above CHUNKS_TOTAL bytes. */
  if (*offset+(int32_t)strpos > CHUNKS_TOTAL)
  {
    strpos = CHUNKS_TOTAL - *offset;
  }

  REST.set_response_payload(response, buffer, strpos);
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

  /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
  *offset += strpos;

  /* Signal end of resource representation. */
  if (*offset>=CHUNKS_TOTAL)
  {
    *offset = -1;
  }
}
#endif

#if REST_RES_LARGE_UPDATE
/*
 * Large resource that can be updated using PUT method
 */
RESOURCE(large_update, METHOD_GET|METHOD_PUT, "large-update", "title=\"Large resource that can be updated using PUT method\";rt=\"block\"");

static int32_t large_update_size = 1280;
static uint8_t large_update_store[2048] = {0};
static unsigned int large_update_ct = -1;

void
large_update_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_packet_t *const coap_req = (coap_packet_t *) request;
  uint8_t method = REST.get_method_type(request);

  if (method & METHOD_GET)
  {
    /* Check the offset for boundaries of the resource data. */
    if (*offset>=large_update_size)
    {
      REST.set_response_status(response, REST.status.BAD_OPTION);
      /* A block error message should not exceed the minimum block size (16). */

      const char *error_msg = "BlockOutOfScope";
      REST.set_response_payload(response, error_msg, strlen(error_msg));
      return;
    }

    REST.set_response_payload(response, large_update_store+*offset, preferred_size);
    REST.set_header_content_type(response, large_update_ct);

    /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
    *offset += preferred_size;

    /* Signal end of resource representation. */
    if (*offset>=large_update_size)
    {
      *offset = -1;
    }
  } else {
    uint8_t *incoming = NULL;
    size_t len = 0;

    unsigned int ct = REST.get_header_content_type(request);
    if (ct==-1)
    {
      REST.set_response_status(response, REST.status.BAD_REQUEST);
      const char *error_msg = "NoContentType";
      REST.set_response_payload(response, error_msg, strlen(error_msg));
      return;
    }

    if ((len = REST.get_request_payload(request, &incoming)))
    {
      if (coap_req->block1_num*coap_req->block1_size+len <= sizeof(large_update_store))
      {
        memcpy(large_update_store+coap_req->block1_num*coap_req->block1_size, incoming, len);
        large_update_size = coap_req->block1_num*coap_req->block1_size+len;
        large_update_ct = REST.get_header_content_type(request);

        REST.set_response_status(response, REST.status.CHANGED);
        coap_set_header_block1(response, coap_req->block1_num, 0, coap_req->block1_size);
      }
      else
      {
        REST.set_response_status(response, REST.status.REQUEST_ENTITY_TOO_LARGE);
        REST.set_response_payload(response, buffer, snprintf((char *)buffer, MAX_PLUGFEST_PAYLOAD, "%uB max.", sizeof(large_update_store)));
        return;
      }
    }
    else
    {
      REST.set_response_status(response, REST.status.BAD_REQUEST);
      const char *error_msg = "NoPayload";
      REST.set_response_payload(response, error_msg, strlen(error_msg));
      return;
    }
  }
}
#endif

#if REST_RES_LARGE_CREATE
/*
 * Large resource that can be created using POST method
 */
RESOURCE(large_create, METHOD_POST, "large-create", "title=\"Large resource that can be created using POST method\";rt=\"block\"");

void
large_create_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_packet_t *const coap_req = (coap_packet_t *) request;

  uint8_t *incoming = NULL;
  size_t len = 0;

  unsigned int ct = REST.get_header_content_type(request);
  if (ct==-1)
  {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    const char *error_msg = "NoContentType";
    REST.set_response_payload(response, error_msg, strlen(error_msg));
    return;
  }

  if ((len = REST.get_request_payload(request, &incoming)))
  {
    if (coap_req->block1_num*coap_req->block1_size+len <= 2048)
    {
      REST.set_response_status(response, REST.status.CREATED);
      REST.set_header_location(response, "/nirvana");
      coap_set_header_block1(response, coap_req->block1_num, 0, coap_req->block1_size);
    }
    else
    {
      REST.set_response_status(response, REST.status.REQUEST_ENTITY_TOO_LARGE);
      const char *error_msg = "2048B max.";
      REST.set_response_payload(response, error_msg, strlen(error_msg));
      return;
    }
  }
  else
  {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    const char *error_msg = "NoPayload";
    REST.set_response_payload(response, error_msg, strlen(error_msg));
    return;
  }
}
#endif

#if REST_RES_OBS
/*
 * Observable resource which changes every 5 seconds
 */
PERIODIC_RESOURCE(obs, METHOD_GET, "obs", "title=\"Observable resource which changes every 5 seconds\";obs;rt=\"observe\"", 5*CLOCK_SECOND);

static uint16_t obs_counter = 0;
static char obs_content[16];

void
obs_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_max_age(response, 5);

  REST.set_response_payload(response, obs_content, snprintf(obs_content, MAX_PLUGFEST_PAYLOAD, "TICK %lu", obs_counter));

  /* A post_handler that handles subscriptions will be called for periodic resources by the REST framework. */
}

/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
void
obs_periodic_handler(resource_t *r)
{
  ++obs_counter;

  //PRINTF("TICK %u for /%s\n", obs_counter, r->url);

  /* Build notification. */
  /*TODO: REST.new_response() */
  coap_packet_t notification[1]; /* This way the packet can be treated as pointer as usual. */
  coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0 );

  /* Better use a generator function for both handlers that only takes *resonse. */
  obs_handler(NULL, notification, NULL, 0, NULL);

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r, obs_counter, notification);
}
#endif

#if REST_RES_MIRROR
/* This resource mirrors the incoming request. It shows how to access the options and how to set them for the response. */
RESOURCE(mirror, METHOD_GET | METHOD_POST | METHOD_PUT | METHOD_DELETE, "debug/mirror", "title=\"Returns your decoded message\";rt=\"Debug\"");

void
mirror_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* The ETag and Token is copied to the header. */
  uint8_t opaque[] = {0x0A, 0xBC, 0xDE};

  /* Strings are not copied, so use static string buffers or strings in .text memory (char *str = "string in .text";). */
  static char location[] = {'/','f','/','a','?','k','&','e', 0};

  /* Getter for the header option Content-Type. If the option is not set, text/plain is returned by default. */
  unsigned int content_type = REST.get_header_content_type(request);

  /* The other getters copy the value (or string/array pointer) to the given pointers and return 1 for success or the length of strings/arrays. */
  uint32_t max_age_and_size = 0;
  const char *str = NULL;
  uint32_t observe = 0;
  const uint8_t *bytes = NULL;
  const uint16_t *words = NULL;
  uint32_t block_num = 0;
  uint8_t block_more = 0;
  uint16_t block_size = 0;
  const char *query = "";
  int len = 0;

  /* Mirror the received header options in the response payload. Unsupported getters (e.g., rest_get_header_observe() with HTTP) will return 0. */

  int strpos = 0;
  /* snprintf() counts the terminating '\0' to the size parameter.
   * The additional byte is taken care of by allocating REST_MAX_CHUNK_SIZE+1 bytes in the REST framework.
   * Add +1 to fill the complete buffer, as the payload does not need a terminating '\0'. */
  
  
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_header_if_match(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "If-Match 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_header_host(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Uri-Host %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_etag(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "ETag 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && REST.get_header_if_none_match(request))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "If-None-Match\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_observe(request, &observe))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Observe %lu\n", observe);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Location-Path %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_uri_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Uri-Path %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && content_type!=-1)
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Content-Format %u\n", content_type);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && REST.get_header_max_age(request, &max_age_and_size))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Max-Age %lu\n", max_age_and_size);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_query(request, &query)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Uri-Query %.*s\n", len, query);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_header_accept(request, &words)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Accept ");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%u", words[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_token(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Token 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_query(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Location-Query %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block2(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Block2 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block1(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Block1 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && REST.get_header_length(request, &max_age_and_size))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Size %lu\n", max_age_and_size);
  }

  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_request_payload(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n%.*s", len, bytes);
  }

  if (strpos >= REST_MAX_CHUNK_SIZE)
  {
      buffer[REST_MAX_CHUNK_SIZE-1] = 0xBB; /* '»' to indicate truncation */
  }

  REST.set_response_payload(response, buffer, strpos);

  PRINTF("/mirror options received: %s\n", buffer);

  /* Set dummy header options for response. Like getters, some setters are not implemented for HTTP and have no effect. */
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_max_age(response, 17); /* For HTTP, browsers will not re-request the page for 17 seconds. */
  REST.set_header_etag(response, opaque, 2);
  REST.set_header_location(response, location); /* Initial slash is omitted by framework */
  REST.set_header_length(response, strpos); /* For HTTP, browsers will not re-request the page for 10 seconds. CoAP action depends on the client. */

/* CoAP-specific example: actions not required for normal RESTful Web service. */
  coap_set_header_uri_host(response, "Contiki");
  coap_set_header_observe(response, 10);
  coap_set_header_proxy_uri(response, "ftp://x");
  //coap_set_header_block2(response, 42, 0, 64);
  //coap_set_header_block1(response, 23, 0, 16);
  coap_set_header_accept(response, TEXT_PLAIN);
  coap_set_header_if_none_match(response);
}
#endif /* REST_RES_MIRROR */





PROCESS(plugtest_server, "PlugtestServer");
AUTOSTART_PROCESSES(&plugtest_server);

PROCESS_THREAD(plugtest_server, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("ETSI IoT CoAP Plugtests Server\n");

#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif

  PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  PRINTF("LL header: %u\n", UIP_LLH_LEN);
  PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
  PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);

/* if static routes are used rather than RPL */
#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
  set_global_address();
  configure_routing();
#endif

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */
#if REST_RES_TEST
  rest_activate_resource(&resource_test);
#endif
#if REST_RES_LONG
  rest_activate_resource(&resource_longpath);
#endif
#if REST_RES_QUERY
  rest_activate_resource(&resource_query);
#endif
#if REST_RES_SEPARATE
  rest_activate_periodic_resource(&periodic_resource_separate);
#endif
#if REST_RES_LARGE
  rest_activate_resource(&resource_large);
#endif
#if REST_RES_LARGE_UPDATE
  large_update_ct = REST.type.APPLICATION_OCTET_STREAM;
  rest_activate_resource(&resource_large_update);
#endif
#if REST_RES_LARGE_CREATE
  rest_activate_resource(&resource_large_create);
#endif
#if REST_RES_OBS
  rest_activate_periodic_resource(&periodic_resource_obs);
#endif

#if REST_RES_MIRROR
  rest_activate_resource(&resource_mirror);
#endif

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();

  } /* while (1) */

  PROCESS_END();
}
