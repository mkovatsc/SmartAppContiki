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


MEMB(transactions_memb, coap_transaction_t, COAP_MAX_OPEN_TRANSACTIONS);

LIST(restful_periodic_services);


static struct uip_udp_conn *server_conn;

static uint16_t current_tid;

static service_callback service_cbk = NULL;

void
set_service_callback(service_callback callback)
{
  service_cbk = callback;
}

void
coap_activate_periodic_resource(struct periodic_resource_t* periodic_resource) {
  list_add(restful_periodic_services, periodic_resource);
}

/*
static void
fill_error_packet(coap_packet_t* packet, int error, uint16_t tid)
{
  packet->ver=1;
  packet->option_count=0;
  packet->url=NULL;
  packet->options=NULL;
  switch (error){
    case MEMORY_ALLOC_ERR:
      packet->code=INTERNAL_SERVER_ERROR_500;
      packet->tid=tid;
      packet->type=MESSAGE_TYPE_ACK;
      break;
    default:
      break;
  }
}

static void
init_response(coap_packet_t* request, coap_packet_t* response)
{
  init_packet(response);
  if(request->type == MESSAGE_TYPE_CON) {
    response->code = OK_200;
    response->tid = request->tid;
    response->type = MESSAGE_TYPE_ACK;
  }
}

static void send_request(coap_packet_t* request, struct uip_udp_conn *client_conn)
{
  char buf[MAX_PAYLOAD_LEN];
  int data_size = 0;

  //data_size = serialize_packet(request, buf);

  PRINTF("Created a connection with the server ");
  PRINT6ADDR(&client_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
      uip_htons(client_conn->lport), uip_htons(client_conn->rport));

  PRINTF("Sending to: ");
  PRINT6ADDR(&client_conn->ripaddr);
  uip_udp_packet_send(client_conn, buf, data_size);
}
*/

/*-----------------------------------------------------------------------------------*/
static int
handle_incoming_data(void)
{
  int error = NO_ERROR;

  char error_buf[20] = {0};
  int error_size = 9;

  char *send_buf = NULL;
  int send_size = 0;

  PRINTF("handle_incoming_data(): received uip_datalen=%u \n",(uint16_t)uip_datalen());

  char* data = uip_appdata + uip_ext_len;
  uint16_t data_len = uip_datalen() - uip_ext_len;

  if (uip_newdata()) {

    PRINTF("-receiving UDP datagram-\n From: ");
    PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
    PRINTF(":%u\n Length: %u\n Data: ", uip_htons(UIP_UDP_BUF->srcport), data_len );

    PRINTBITS(data, data_len);

    PRINTF("\n -----------------------\n");

    coap_packet_t request[1] = {{0}};

    coap_message_parse(request, data, data_len);

#if DEBUG
    PRINTF("Parsed: v %u, t %u, oc %u, c %u, tid %u\n", request->header->version, request->header->type, request->header->oc, request->header->code, request->header->tid);

    if (IS_OPTION(request->options, COAP_OPTION_CONTENT_TYPE)) { PRINTF("Content-Type: %u\n", request->content_type); }
    if (IS_OPTION(request->options, COAP_OPTION_MAX_AGE)) { PRINTF("Max-Age: %lu\n", request->max_age); }
    if (IS_OPTION(request->options, COAP_OPTION_ETAG)) { PRINTF("ETag: %lX\n", request->etag); }
    if (IS_OPTION(request->options, COAP_OPTION_URI_HOST)) { PRINTF("Uri-Auth: %.*s\n", request->uri_host_len, request->uri_host); }
    if (IS_OPTION(request->options, COAP_OPTION_LOCATION_PATH)) { PRINTF("Location: %.*s\n", request->location_path_len, request->location_path); }
    if (IS_OPTION(request->options, COAP_OPTION_URI_PATH)) { PRINTF("Uri-Path: %.*s\n", request->uri_path_len, request->uri_path); }
    if (IS_OPTION(request->options, COAP_OPTION_OBSERVE)) { PRINTF("Observe: %lu\n", request->observe); }
    if (IS_OPTION(request->options, COAP_OPTION_TOKEN)) { PRINTF("Token: %0X\n", request->token); }
    if (IS_OPTION(request->options, COAP_OPTION_BLOCK)) { PRINTF("Block: %lu%s (%u B/blk)\n", request->block_num, request->block_more ? "+" : "", request->block_size); }
    if (IS_OPTION(request->options, COAP_OPTION_URI_QUERY)) { PRINTF("Uri-Query: %.*s\n", request->uri_query_len, request->uri_query); }

    PRINTF("URL: %.*s\n", request->url_len, request->url);
    PRINTF("Payload: %.*s\n", request->payload_len, request->payload);
#endif

    coap_transaction_t *transaction = NULL;

    if ( (transaction = memb_alloc(&transactions_memb)) )
    {
        /* save client address */
        uip_ipaddr_copy(&transaction->addr, &UIP_IP_BUF->srcipaddr);
        transaction->port = UIP_UDP_BUF->srcport;

        /* prepare response */
        coap_packet_t response[1];
        coap_message_init(response, transaction->packet, COAP_TYPE_ACK, OK_200, request->header->tid);

        /* call application-specific handler */
        if (service_cbk) {
          service_cbk(request, response);
        }

        transaction->packet_len = coap_message_serialize(response);

        send_buf = transaction->packet;
        send_size = transaction->packet_len;

    } else {
        PRINTF("Memory allocation error\n");
        error = MEMORY_ALLOC_ERR;
    }

    if (error!=NO_ERROR) {
        ((coap_header_t *)error_buf)->version = 1;
        ((coap_header_t *)error_buf)->type = COAP_TYPE_ACK;
        ((coap_header_t *)error_buf)->oc = 0;
        ((coap_header_t *)error_buf)->code = INTERNAL_SERVER_ERROR_500;
        ((coap_header_t *)error_buf)->tid = request->header->tid;

        error_size = sizeof(coap_header_t) + sprintf(error_buf + sizeof(coap_header_t), "memb_alloc %u", error);

        send_buf = error_buf;
        send_size = error_size;
    }

    /*configure connection to reply to client*/
    uip_ipaddr_copy(&server_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
    server_conn->rport = UIP_UDP_BUF->srcport;

    uip_udp_packet_send(server_conn, send_buf, send_size);
    PRINTF("-sent UDP datagram------\n Length: %u\n -----------------------\n", send_size);

    /* Restore server connection to allow data from any node */
    memset(&server_conn->ripaddr, 0, sizeof(server_conn->ripaddr));
    server_conn->rport = 0;

    if (transaction)
    {
      memb_free(&transactions_memb, transaction);
    }
  }

  return error;
}
/*-----------------------------------------------------------------------------------*/
static void
resource_changed(struct periodic_resource_t* resource)
{
  PRINTF("resource_changed_event \n");

  /*
   * send response generated by periodic_request_generator
   * FIXME get rid of this buffer

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
  */
}

void
coap_default_post_handler(coap_packet_t* request, coap_packet_t* response) {
	/*FIXME Need to move somewhere else; maybe post_handler, as request must be successful for subscriptions*/
	/*
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
	*/
}


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

  /* new connection with remote host */
  server_conn = udp_new(NULL, uip_htons(0), NULL);
  udp_bind(server_conn, uip_htons(SERVER_LISTEN_PORT));
  PRINTF("Local/remote port %u/%u\n", uip_htons(server_conn->lport), uip_htons(server_conn->rport));

  /*Periodic resources are only available to COAP implementation*/
  /*set event timers for all periodic resources*/
  periodic_resource_t* periodic_resource = NULL;
  for (periodic_resource = (periodic_resource_t*)list_head(restful_periodic_services); periodic_resource; periodic_resource = periodic_resource->next) {
    if (periodic_resource->period) {
      PRINTF("Set timer for Res: %s to %lu\n", periodic_resource->resource->url, periodic_resource->period);
      etimer_set(periodic_resource->handler_cb_timer, periodic_resource->period);
    }
  }

  while(1) {
    PROCESS_YIELD();

    if(ev == tcpip_event) {
      handle_incoming_data();
    } else if (ev == PROCESS_EVENT_TIMER) {
	  /*find resource whose timer expired*/
	  for (periodic_resource = (periodic_resource_t*)list_head(restful_periodic_services);periodic_resource;periodic_resource = periodic_resource->next) {
		if (periodic_resource->period && etimer_expired(periodic_resource->handler_cb_timer)) {
		  PRINTF("Etimer expired for %s (period:%lu life:%lu)\n", periodic_resource->resource->url, periodic_resource->period, periodic_resource->lifetime);
		  /*call the periodic handler function if exists*/
		  if (periodic_resource->periodic_handler) {
			if ((periodic_resource->periodic_handler)(periodic_resource->resource)) {
			  PRINTF("RES CHANGE\n");
			  if (!stimer_expired(periodic_resource->lifetime_timer)) {
				PRINTF("TIMER NOT EXPIRED\n");
				resource_changed(periodic_resource);
				periodic_resource->lifetime = stimer_remaining(periodic_resource->lifetime_timer);
			  } else {
				periodic_resource->lifetime = 0;
			  }
			}

			PRINTF("%s lifetime %lu (%lu) expired %d\n", periodic_resource->resource->url, stimer_remaining(periodic_resource->lifetime_timer), periodic_resource->lifetime, stimer_expired(periodic_resource->lifetime_timer));
		  }
		  etimer_reset(periodic_resource->handler_cb_timer);
		}
	  }
	} /* if (ev) */
  } /* while (1) */

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
