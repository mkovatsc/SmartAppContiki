#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"

#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
#include "static-routing.h"
#endif

#include "rest-engine.h"
#if WITH_COAP==3
#include "coap-03-rest-engine.h"
#elif WITH_COAP==6
#include "coap-06-rest-engine.h"
#endif

#ifndef CONTIKI_TARGET_MINIMAL_NET
#include "loader/elfloader.h"
#include "loader/symbols.h"
#include "dev/leds.h"
#endif

//#define DONT_LOAD /* to be used for files larger than 800 bytes, on which the loader fails */

PROCESS(appstore_example, "Appstore Example");
AUTOSTART_PROCESSES(&appstore_example);

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

char elf_filename[100] = {0};
#define MARKET_PORT UIP_HTONS(2222)
#define NOTIF_PORT UIP_HTONS(3333)
#define COLLECT_PORT UIP_HTONS(6666)
static process_event_t loading_requested_event;
static int fd;
static int file_len;
static uip_ipaddr_t host_ipaddr;
static uint16_t host_port;

uip_ipaddr_t market_ipaddr;

static uip_ipaddr_t collect_ipaddr;
struct uip_udp_conn *collect_conn;

/*---------------------------------------------------------------------------*/
RESOURCE(loader, METHOD_GET | METHOD_POST, "loader", "");

void loader_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {

  size_t payload_len;
  const uint8_t *payload;
  char response_txt[] = "I'm on it.";

  if ((payload_len = REST.get_request_payload(request, &payload))) {
    /* store the host address  */
    memcpy(&host_ipaddr, &UIP_IP_BUF->srcipaddr, sizeof(uip_ipaddr_t));
    host_port = UIP_UDP_BUF->srcport;
    /* store the filename as a C string */
    memcpy(elf_filename, payload, payload_len);
    elf_filename[payload_len] = '\0';
    PRINTF("Loader request received: %s\n", elf_filename);

    /* send response */
    REST.set_response_payload(response, (uint8_t *)response_txt, strlen(response_txt));
    process_post(&appstore_example, loading_requested_event, NULL);
  }
}

/*---------------------------------------------------------------------------*/
static void load_elf() {
#ifndef CONTIKI_TARGET_MINIMAL_NET
  fd = cfs_open(elf_filename, CFS_READ | CFS_WRITE);
  int ret;
  ret = elfloader_load(fd);

  char *print, *symbol = NULL;

  if(ret != ELFLOADER_OK) {
    switch(ret) {
    case ELFLOADER_BAD_ELF_HEADER:
      print = "Bad ELF header";
      break;
    case ELFLOADER_NO_SYMTAB:
      print = "No symbol table";
      break;
    case ELFLOADER_NO_STRTAB:
      print = "No string table";
      break;
    case ELFLOADER_NO_TEXT:
      print = "No text segment";
      break;
    case ELFLOADER_SYMBOL_NOT_FOUND:
      print = "Symbol not found: ";
      symbol = elfloader_unknown;
      break;
    case ELFLOADER_SEGMENT_NOT_FOUND:
      print = "Segment not found: ";
      symbol = elfloader_unknown;
      break;
    case ELFLOADER_NO_STARTPOINT:
      print = "No starting point";
      break;
    default:
      print = "Unknown return code from the ELF loader (internal bug)";
      break;
    }
    PRINTF("ELF loading error: %s, %s\n", print, symbol);
  }

  cfs_close(fd);
  cfs_remove(elf_filename);
#endif
}

/*---------------------------------------------------------------------------*/
static void write_to_coffee(void *response) {
  uint32_t offset;
  size_t payload_len;
  const uint8_t *payload;

#if WITH_COAP==3
  coap_get_header_block(response, NULL, NULL, NULL, &offset);
#elif WITH_COAP==6
  coap_get_header_block2(response, NULL, NULL, NULL, &offset);
#endif
  if ( (payload_len = coap_get_payload(response, &payload)) ) {
    file_len = offset + payload_len;
    PRINTF("cfs write %u bytes at %lu\n", payload_len, offset);
#ifndef CONTIKI_TARGET_MINIMAL_NET
    cfs_seek(fd, offset, CFS_SEEK_SET);
    cfs_write(fd, payload, payload_len);
#endif
  }
}

/*---------------------------------------------------------------------------*/
static void appstore_final(void *response) {
  size_t payload_len;
  const uint8_t *payload;

  payload_len = coap_get_payload(response, &payload);
  PRINTF("Notification acknowledged: %.*s\n", payload_len, (char *)payload);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(appstore_example, ev, data) {
  static int process_started = 0;

  PROCESS_BEGIN();
  PRINTF("Appstore\n");

  /* init */
#ifndef CONTIKI_TARGET_MINIMAL_NET
  uip_ip6addr(&market_ipaddr, 0xaaaa,0,0,0,0,0,0,0x1);
#else
  uip_ip6addr(&market_ipaddr, 0xcccc,0,0,0,0,0,0,0x1);
#endif
  loading_requested_event = process_alloc_event();

  /* static routing */
#if !UIP_CONF_IPV6_RPL && !defined(CONTIKI_TARGET_MINIMAL_NET)
    set_global_address();
    configure_routing();
#endif /*!UIP_CONF_IPV6_RPL*/

#if !IN_COOJA && !defined (CONTIKI_TARGET_MINIMAL_NET)
  /* motes need format */
  PRINTF("CFS format\n");
  cfs_coffee_format();
  PRINTF("Done\n");
#endif

  /* Initialize the REST framework. */
  rest_init_framework();

  /* Activate the resources. */
  rest_activate_resource(&resource_loader);

  /* connection used for collect */
  uip_ip6addr(&collect_ipaddr, 0xaaaa,0,0,0,0,0,0,0x1);
  collect_conn = udp_new(&collect_ipaddr, COLLECT_PORT, NULL);

  while (1) {
    static struct request_state_t request_state;
    static coap_packet_t request[1]; /* This way the packet can be treated as pointer as usual. */

    PRINTF("Waiting for loader request\n");
    PROCESS_YIELD_UNTIL(ev == loading_requested_event);

    PRINTF("Resumed process\n");

    if(process_started) {
    	PRINTF("Exit loaded processes\n");
#ifndef CONTIKI_TARGET_MINIMAL_NET
    	autostart_exit(elfloader_autostart_processes);
    	leds_off(LEDS_ALL);
#endif
        process_started = 0;
    }

    PRINTF("Requesting ELF file %s\n", elf_filename);
    /* open file in which the elf will be stored */
#ifndef CONTIKI_TARGET_MINIMAL_NET
    fd = cfs_open(elf_filename, CFS_WRITE);
    if(fd == -1) {
    	PRINTF("Couldn't open CFS file\n");
    	goto end;
    }
#endif
    file_len = 0;

    /* Issue blocking block-wise request. */
    coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
    coap_set_header_uri_path(request, "market");
    coap_set_payload(request, (uint8_t *)elf_filename, strlen(elf_filename));
    /* request the elf to the market and store it in CFS */
    PT_SPAWN(process_pt, &request_state.pt,
             coap_blocking_request(&request_state, ev,
                                   &market_ipaddr, MARKET_PORT,
                                   request, write_to_coffee)
    );
    PRINTF("%s received and stored (%d bytes)\n", elf_filename, file_len);

    // FIXME memory footprint
//#ifndef DONT_LOAD
    /* load the elf */
//    load_elf();
//    /* execute the program */
//    if(elfloader_autostart_processes) {
//      PRINTF("%s file loaded\n", elf_filename);
//      autostart_start(elfloader_autostart_processes);
//      PRINTF("%s process executed\n", elf_filename);
//      process_started = 1;
//    }
//#endif

    /* send a request to notify the end of the process */
    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "notify");
    coap_set_payload(request, (uint8_t *)elf_filename, strlen(elf_filename));
    /* request the elf to the market and store it in CFS */
    PT_SPAWN(process_pt, &request_state.pt,
             coap_blocking_request(&request_state, ev,
                                   &host_ipaddr, host_port,
                                   request, appstore_final)
    );

end:
#ifndef CONTIKI_TARGET_MINIMAL_NET
    if(fd != -1) {
      cfs_close(fd);
    }
    /* remove the file from coffee */
    cfs_remove(elf_filename);
#endif
    PRINTF("End.\n");
  }

  PROCESS_END();
}
