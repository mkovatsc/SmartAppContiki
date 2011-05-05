#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#include "coap-server.h"

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
#include "static-routing.h"
#endif

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#define PRINTBITS(buf,len) { \
      int i,j=0; \
      for (i=0; i<len; ++i) { \
        for (j=7; j>=0; --j) { \
          PRINTF("%c", (((char *)buf)[i] & 1<<j) ? '1' : '0'); \
        } \
        PRINTF(" "); \
      } \
    }
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#define PRINTBITS(buf,len)
#endif

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[uip_l2_l3_hdr_len])

/*-----------------------------------------------------------------------------------*/
/*- Constants -----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------------*/
const char* error_messages[] = {
  "",

  /* Memory errors */
  "Transaction buffer allocation failed",
  "Memory boundary exceeded",

  /* CoAP errors */
  "Request has unknown critical option" //FIXME which one?
};
/*-----------------------------------------------------------------------------------*/

struct process *process_coap_server = NULL;

static uint16_t current_tid;
static service_callback service_cbk = NULL;

/*-----------------------------------------------------------------------------------*/
void
set_service_callback(service_callback callback)
{
  service_cbk = callback;
}
/*-----------------------------------------------------------------------------------*/
static
int
handle_incoming_data(void)
{
  int error = NO_ERROR;

  PRINTF("handle_incoming_data(): received uip_datalen=%u \n",(uint16_t)uip_datalen());

  uint8_t *data = uip_appdata + uip_ext_len;
  uint16_t data_len = uip_datalen() - uip_ext_len;

  if (uip_newdata()) {

    PRINTF("receiving UDP datagram from: ");
    PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
    PRINTF(":%u\n  Length: %u\n  Data: ", uip_htons(UIP_UDP_BUF->srcport), data_len );
    PRINTBITS(data, data_len);
    PRINTF("\n");

    coap_packet_t request[1] = {{0}};
    coap_transaction_t *transaction = NULL;

    error = coap_parse_message(request, data, data_len);

    if (error==NO_ERROR)
    {

      PRINTF("  Parsed: v %u, t %u, oc %u, c %u, tid %u\n", request->version, request->type, request->option_count, request->code, request->tid);
      PRINTF("  URL: %.*s\n", request->url_len, request->url);
      PRINTF("  Payload: %.*s\n", request->payload_len, request->payload);

      if (request->type==COAP_TYPE_CON)
      {
        /* Use transaction buffer for response to confirmable request. */
        if ( (transaction = coap_new_transaction(request->tid, &UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport)) )
        {
            uint32_t block_num = 0;
            uint16_t block_size = REST_MAX_CHUNK_SIZE;
            uint32_t block_offset = 0;
            int32_t new_offset = 0;

            /* prepare response */
            coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */
            coap_init_message(response, transaction->packet, COAP_TYPE_ACK, OK_200, request->tid);

            /* resource handlers must take care of different handling (e.g., TOKEN_OPTION_REQUIRED_240) */
            if (IS_OPTION(request, COAP_OPTION_TOKEN))
            {
                coap_set_header_token(response, request->token, request->token_len);
                SET_OPTION(response, COAP_OPTION_TOKEN);
            }

            /* get offset for blockwise transfers */
            if (coap_get_header_block(request, &block_num, NULL, &block_size, &block_offset))
            {
                PRINTF("Blockwise: block request %lu (%u/%u) @ %lu bytes\n", block_num, block_size, REST_MAX_CHUNK_SIZE, block_offset);
                block_size = MIN(block_size, REST_MAX_CHUNK_SIZE);
                new_offset = block_offset;
            }

            /*------------------------------------------*/
            /* call application-specific handler        */
            /*------------------------------------------*/
            if (service_cbk) {
              service_cbk(request, response, transaction->packet+COAP_MAX_HEADER_SIZE, block_size, &new_offset);
            }
            /*------------------------------------------*/


            /* apply blockwise transfers */
            if ( IS_OPTION(request, COAP_OPTION_BLOCK) ) //|| new_offset!=0 && new_offset!=block_offset
            {
              /* unchanged new_offset indicates that resource is unaware of blockwise transfer */
              if (new_offset==block_offset)
              {
                PRINTF("Blockwise: unaware resource with payload length %u/%u\n", response->payload_len, block_size);
                if (block_offset >= response->payload_len)
                {
                  coap_set_code(response, BAD_REQUEST_400);
                  coap_set_payload(response, (uint8_t*)"Block out of scope", 18);
                }
                else
                {
                  coap_set_header_block(response, block_num, response->payload_len - block_offset > block_size, block_size);
                  coap_set_payload(response, response->payload+block_offset, MIN(response->payload_len - block_offset, block_size));
                } /* if (valid offset) */
              }
              else
              {
                /* resource provides chunk-wise data */
                PRINTF("Blockwise: blockwise resource, new offset %ld\n", new_offset);
                coap_set_header_block(response, block_num, new_offset!=-1 || response->payload_len > block_size, block_size);
                if (response->payload_len > block_size) coap_set_payload(response, response->payload, block_size);
              } /* if (resource aware of blockwise) */
            }
            else if (new_offset!=0)
            {
              PRINTF("Blockwise: no block option for blockwise resource, using block size %u\n", REST_MAX_CHUNK_SIZE);

              rest_set_header_block(response, 0, new_offset!=-1, REST_MAX_CHUNK_SIZE);
              rest_set_response_payload(response, response->payload, MIN(response->payload_len, REST_MAX_CHUNK_SIZE));
            } /* if (blockwise request) */

            transaction->packet_len = coap_serialize_message(response);

        } else {
            error = MEMORY_ALLOC_ERR;
        }
      }
      else if (request->type==COAP_TYPE_NON)
      {
        /* Call application-specific handler without response. */
        if (service_cbk) {
          service_cbk(request, NULL, NULL, 0, 0);
        }
      }
      else if (request->type==COAP_TYPE_ACK)
      {
        PRINTF("Received ACK\n");
        coap_cancel_transaction_by_tid(request->tid);
      }
      else if (request->type==COAP_TYPE_RST)
      {
        PRINTF("Received RST\n");
        coap_cancel_transaction_by_tid(request->tid);
        if (IS_OPTION(request, COAP_OPTION_TOKEN))
        {
          PRINTF("  Token 0x%02X%02X\n", request->token[0], request->token[1]);
          coap_remove_observer_by_token(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, request->token, request->token_len);
        }
      }
    } /* if (parsed correctly) */

    if (error==NO_ERROR) {
      if (transaction) coap_send_transaction(transaction);
    }
    else
    {
      PRINTF("ERROR %u: %s\n", error, error_messages[error]);

      /* reuse input buffer */
      coap_init_message(request, request->header, COAP_TYPE_ACK, INTERNAL_SERVER_ERROR_500, request->tid);
      coap_set_payload(request, (uint8_t *) error_messages[error], strlen(error_messages[error]));
      coap_serialize_message(request);
      coap_send_message(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, request->header, coap_serialize_message(request));
    }
  } /* if (new data) */

  return error;
}
/*-----------------------------------------------------------------------------------*/
/* The discover resource should be included when using CoAP. */
RESOURCE(well_known_core, METHOD_GET, ".well-known/core", "");
void
well_known_core_handler(coap_packet_t* request, coap_packet_t* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* Response might be NULL for non-confirmable requests. */
  if (response)
  {
    int strpos = 0;
    int bufpos = 0;
    resource_t* resource = NULL;

    for (resource = (resource_t*)list_head(rest_get_resources()); resource; resource = resource->next)
    {
      strpos += snprintf(buffer + bufpos, REST_MAX_CHUNK_SIZE - bufpos + 1,
                         "</%s>%s%s%s",
                         resource->url,
                         resource->attributes[0] ? ";" : "",
                         resource->attributes,
                         resource->next ? "," : "" );

      PRINTF("discover: %s\n", resource->url);

      if (strpos <= *offset)
      {
        /* Discard output before current block */
        PRINTF("  if %d <= %ld B\n", strpos, *offset);
        PRINTF("  %s\n", buffer);
        bufpos = 0;
      }
      else /* (strpos > *offset) */
      {
        /* output partly in block */
        int len = MIN(strpos - *offset, preferred_size);

        PRINTF("  el %d/%d @ %ld B\n", len, preferred_size, *offset);

        /* Block might start in the middle of the output; align with buffer start. */
        if (bufpos == 0)
        {
          memmove(buffer, buffer+strlen(buffer)-strpos+*offset, len);
        }

        bufpos = len;
        PRINTF("  %s\n", buffer);

        if (bufpos >= preferred_size)
        {
          break;
        }
      }
    }

    rest_set_response_payload(response, buffer, bufpos );
    rest_set_header_content_type(response, APPLICATION_LINK_FORMAT);

    if (resource==NULL) {
        *offset = -1;
    }
    else
    {
      *offset += bufpos;
    }
  }
}
/*-----------------------------------------------------------------------------------*/
PROCESS(coap_server, "Coap Server");
PROCESS_THREAD(coap_server, ev, data)
{
  PROCESS_BEGIN();
  PRINTF("COAP SERVER\n");

/* if static routes are used rather than RPL */
#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
  set_global_address();
  configure_routing();
#endif

  process_coap_server = PROCESS_CURRENT();

  rest_activate_resource(&resource_well_known_core);

  current_tid = random_rand();

  coap_init_connection(SERVER_LISTEN_PORT);

  /*Periodic resources are only available to COAP implementation*/
  /*set event timers for all periodic resources*/
  /*
  periodic_resource_t* periodic_resource = NULL;
  for (periodic_resource = (periodic_resource_t*)list_head(restful_periodic_services); periodic_resource; periodic_resource = periodic_resource->next) {
    if (periodic_resource->period) {
      PRINTF("Set timer for Res: %s to %lu\n", periodic_resource->resource->url, periodic_resource->period);
      etimer_set(periodic_resource->handler_cb_timer, periodic_resource->period);
    }
  }
  */

  while(1) {
    PROCESS_YIELD();

    if(ev == tcpip_event) {
      handle_incoming_data();
    } else if (ev == PROCESS_EVENT_TIMER) {
      coap_check_transactions();
    }
  } /* while (1) */

  PROCESS_END();
}
/*-----------------------------------------------------------------------------------*/
