#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> /*for isxdigit*/
#include "contiki.h"
#include "contiki-net.h"

#include "coap-server.h"

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
#include "static-routing.h"
#endif

#define DEBUG 0
#if DEBUG
#include <stdio.h>
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
  "MEMORY_BOUNDARY_EXCEEDED",

  /* CoAP errors */
  "Request sent unknown critical option" //FIXME which one?
};
/*-----------------------------------------------------------------------------------*/

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

    PRINTF("-receiving UDP datagram-\n From: ");
    PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
    PRINTF(":%u\n Length: %u\n Data: ", uip_htons(UIP_UDP_BUF->srcport), data_len );

    PRINTBITS(data, data_len);

    PRINTF("\n -----------------------\n");

    coap_packet_t request[1] = {{0}};
    coap_transaction_t *transaction = NULL;

    error = coap_parse_message(request, data, data_len);

    if (error==NO_ERROR)
    {

      PRINTF("Parsed: v %u, t %u, oc %u, c %u, tid %u\n", request->version, request->type, request->option_count, request->code, request->tid);
      PRINTF("URL: %.*s\n", request->url_len, request->url);
      PRINTF("Payload: %.*s\n", request->payload_len, request->payload);

      if (request->type==COAP_TYPE_CON)
      {
        /* Use transaction buffer for response to confirmable request. */
        if ( (transaction = coap_new_transaction(request->tid, &UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport)) )
        {
            /* prepare response */
            coap_packet_t response[1];
            coap_init_message(response, transaction->packet, COAP_TYPE_ACK, OK_200, request->tid);

            /* resource handlers must take care of different handling (e.g., TOKEN_OPTION_REQUIRED_240) */
            if (IS_OPTION(request, COAP_OPTION_TOKEN))
            {
                coap_get_header_token(request, &response->token);
                SET_OPTION(response, COAP_OPTION_TOKEN);
            }

            /* call application-specific handler */
            if (service_cbk) {
              service_cbk(request, response);
            }

            transaction->packet_len = coap_serialize_message(response);

        } else {
            error = MEMORY_ALLOC_ERR;
        }
      }
      else if (request->type==COAP_TYPE_NON)
      {
        /* Call application-specific handler without response. */
        if (service_cbk) {
          service_cbk(request, NULL);
        }
      }
      else if (request->type==COAP_TYPE_ACK || request->type==COAP_TYPE_RST)
      {
        //TODO check for subscriptions or registered tokens
        PRINTF("Received ACK or RST\n");
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
      coap_set_payload(request, request->header + COAP_HEADER_LEN, sprintf((char *) (request->header + COAP_HEADER_LEN), "%s", error_messages[error]));
      coap_serialize_message(request);
      coap_send_message(&UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport, request->header, coap_serialize_message(request));
    }
  } /* if (new data) */

  return error;
}
/*-----------------------------------------------------------------------------------*/
void
coap_default_block_handler(coap_packet_t* request, coap_packet_t* response)
{
  PRINTF("coap_default_block_handler(): ");

  uint32_t block_num = 0;
  uint8_t block_more = 0;
  uint16_t block_size = 0;
  uint32_t offset = 0;

  if (coap_get_header_block(request, &block_num, &block_more, &block_size, &offset))
  {
    PRINTF("%lu (%u/%u), offset %lu\n", block_num, response->payload_len, block_size, offset);

    if (offset > response->payload_len)
    {
      coap_set_code(response, BAD_REQUEST_400);
      coap_set_payload(response, (uint8_t*)"Block out of scope", 18);
      return;
    }

    response->payload_len = response->payload_len - offset;
    block_more = response->payload_len > block_size;

    rest_set_header_block(response, block_num, block_more, block_size);
    rest_set_response_payload(response, response->payload+offset, response->payload_len<block_size ? response->payload_len : block_size);
  }
  else
  {
    PRINTF("no block option in request, using block size %u\n", COAP_DEFAULT_BLOCK_SIZE);

    rest_set_header_block(response, 0, response->payload_len>COAP_DEFAULT_BLOCK_SIZE, COAP_DEFAULT_BLOCK_SIZE);
    rest_set_response_payload(response, response->payload, response->payload_len<COAP_DEFAULT_BLOCK_SIZE ? response->payload_len : COAP_DEFAULT_BLOCK_SIZE);
  }
}
/*---------------------------------------------------------------------------*/
/*
void
default_observe_handler(REQUEST* request, RESPONSE* response)
{
  //FIXME Need to move somewhere else; maybe post_handler, as request must be successful for subscriptions

  uint32_t lifetime = 0;
  if (coap_get_header_subscription_lifetime(request, &lifetime)) {
    PRINTF("Lifetime %lu\n", lifetime);

    periodic_resource_t* periodic_resource = NULL;
    for (periodic_resource = (periodic_resource_t*)list_head(restful_periodic_services);
             periodic_resource;
             periodic_resource = periodic_resource->next) {
          if (periodic_resource->resource == resource) {
            PRINTF("Periodic Resource Found\n");
            PRINT6ADDR(&request->addr);
            periodic_resource->lifetime = lifetime;
            stimer_set(periodic_resource->lifetime_timer, lifetime);
            uip_ipaddr_copy(&periodic_resource->addr, &request->addr);
          }
    }
  }
}
*/
/*---------------------------------------------------------------------------*/

/*
 * send response generated by periodic_request_generator
 *
static void
resource_changed(periodic_resource_t* resource)
{
  PRINTF("resource_changed_event \n");


  //FIXME get rid of this buffer

  if (init_buffer(COAP_DATA_BUFF_SIZE)) {
        coap_packet_t* request = (coap_packet_t*)allocate_buffer(sizeof(coap_packet_t));
        init_packet(request);
        coap_set_code(request, COAP_GET);
        request->tid = current_tid++;
        coap_set_header_subscription_lifetime(request, resource->lifetime);
        coap_set_header_uri(request, (char *)resource->resource->url);
        if (resource->periodic_request_generator) {
          resource->periodic_request_generator(request);
        }

        if (!resource->client_conn) {
          //FIXME send port is fixed for now to 61616
          resource->client_conn = udp_new(&resource->addr, uip_htons(61616), NULL);
          udp_bind(resource->client_conn, uip_htons(MOTE_CLIENT_LISTEN_PORT));
        }

        if (resource->client_conn) {
                //FIXME should be a response/confirmable with code
          //send_request(request, resource->client_conn);
        }

        delete_buffer();
  }
}
*/
/*---------------------------------------------------------------------------*/

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
/*---------------------------------------------------------------------------*/
