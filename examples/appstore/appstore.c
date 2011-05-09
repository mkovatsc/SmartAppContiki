#include <stdio.h>
#include <stdlib.h>
#include "contiki.h"
#include "contiki-net.h"
#include "rest.h"
#include "loader/elfloader.h"
#include "loader/symbols.h"
#include "dev/leds.h"

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

static char elf_filename[100] = {0};
#define MARKET_PORT 2222
#define NOTIF_PORT 3333
#define COLLECT_PORT 6666
static process_event_t loading_requested_event;
static int fd;
static int file_len;
static uip_ipaddr_t host_ipaddr;
static struct uip_udp_conn *client_conn;
uip_ipaddr_t market_ipaddr;

static uip_ipaddr_t collect_ipaddr;
struct uip_udp_conn *collect_conn;

/* Resources are defined by RESOURCE macro, signature: resource name, the http methods it handles and its url*/
RESOURCE(loader, METHOD_GET, "loader");

/* For each resource defined, there corresponds an handler method which should be defined too.
 * Name of the handler method should be [resource name]_handler
 * */
/*---------------------------------------------------------------------------*/
void loader_handler(REQUEST* request, RESPONSE* response) {
  char temp[100];

  /* store the filename as a C string */
  memcpy(elf_filename, request->payload, request->payload_len);
  elf_filename[request->payload_len] = '\0';

  PRINTF("Loader request received. Payload: %s\n", elf_filename);

  /* store the host address  */
  memcpy(&host_ipaddr, &request->addr, sizeof(uip_ipaddr_t));

  /* send response */
  sprintf(temp,"I'm on it.");
  rest_set_header_content_type(response, TEXT_PLAIN);
  rest_set_response_payload(response, temp, strlen(temp));
  process_post(&appstore_example, loading_requested_event, NULL);
}

/*---------------------------------------------------------------------------*/
static void load_elf() {
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
}

/*---------------------------------------------------------------------------*/
static void write_to_coffee(uint16_t bloc, uint8_t *payload, uint16_t len) {
  static int max_bloc;
  if(payload) {
    if(file_len == 0) {
      max_bloc = 0;
    }
    if(bloc > max_bloc) {
      max_bloc = bloc;
      file_len = 64 * bloc + len;
    }
    PRINTF("cfs write %d %d %d\n", bloc, 64*bloc, len);
    cfs_seek(fd, 64*bloc, CFS_SEEK_SET);
    cfs_write(fd, payload, len);
  }
}

/*---------------------------------------------------------------------------*/
static void appstore_final(uint16_t bloc, uint8_t *payload, uint16_t len) {
  uint16_t i;
  PRINTF("Notification acknowledged: ");
  for(i=0; i<len; i++) {
    PRINTF("%c", payload[i]);
  }
  PRINTF("\n");
}

/*---------------------------------------------------------------------------*/
static PT_THREAD(tcp_market_request(struct psock *psock, char* payload, struct uip_conn *conn)) {
  int len = strlen(elf_filename);
  PSOCK_BEGIN(psock);
  PSOCK_WAIT_UNTIL(psock, uip_connected());
  elf_filename[len] = '\n';
  elf_filename[len+1] = '\0';
  PSOCK_SEND_STR(psock, elf_filename);
  elf_filename[len-1] = '\0';

  while(!uip_closed()) {
    PSOCK_WAIT_UNTIL(psock, PSOCK_NEWDATA(psock) || uip_closed());
    if(PSOCK_NEWDATA(psock)) {
      PSOCK_READBUF(psock);
      PRINTF("cfs write %d bytes\n", PSOCK_DATALEN(psock));
      cfs_write(fd, psock->bufptr, PSOCK_DATALEN(psock));
      file_len += PSOCK_DATALEN(psock);
    }
  }
  printf("closed.\n");
  PSOCK_CLOSE(psock);
  PSOCK_END(psock);
}

/*---------------------------------------------------------------------------*/
static void nothing() {}
/*---------------------------------------------------------------------------*/
const struct uip_fallback_interface fallback_interface = {
  nothing, nothing
};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(appstore_example, ev, data) {
  static struct uip_udp_conn *connection;
  static struct coap_client_t coap_client;
  static int process_started = 0;

#if COAP_MODE == COAP_TCP
  static struct psock market_sock;
  static struct uip_conn *tcp_conn;
  static char sock_buff[UIP_TCP_MSS];
#endif

  PROCESS_BEGIN();
  PRINTF("Appstore\n");

#if !UIP_CONF_IPV6_RPL
    set_global_address();
    configure_routing();
#endif /*!UIP_CONF_IPV6_RPL*/

  PRINTF("CFS format\n");
  cfs_coffee_format();
  PRINTF("Done\n");

  rest_init();
  rest_activate_resource(&resource_loader);
  uip_ip6addr(&market_ipaddr, 0xaaaa,0,0,0,0,0,0,0x1);

  loading_requested_event = process_alloc_event();

  /* connection used for collect */
  uip_ip6addr(&collect_ipaddr, 0xaaaa,0,0,0,0,0,0,0x1);
  collect_conn = udp_new(&collect_ipaddr, UIP_HTONS(COLLECT_PORT), NULL);

  while(1) {
    PROCESS_WAIT_UNTIL(ev == loading_requested_event);

    if(process_started) {
      PRINTF("Exit loaded processes\n");
      autostart_exit(elfloader_autostart_processes);
      process_started = 0;
      leds_off(LEDS_ALL);
    }

    PRINTF("Requesting ELF file %s\n", elf_filename);
    /* open file in which the elf will be stored */
    fd = cfs_open(elf_filename, CFS_WRITE);
    if(fd == -1) {
      PRINTF("Couldn't open CFS file\n");
      goto end;
    }
    file_len = 0;
    /* connect to the market */
    connection = udp_new(&market_ipaddr, UIP_HTONS(MARKET_PORT), NULL);
    if(connection == NULL) {
      PRINTF("Couldn't open UDP connection\n");
      goto end;
    }

    /* request the elf to the market and store it in CFS */
    PT_SPAWN(process_pt, &coap_client.pt, coap_client_request(&coap_client, COAP_GET, "market", elf_filename, connection, write_to_coffee));

    PRINTF("%s received and stored (%d bytes)\n", elf_filename, file_len);

#ifndef DONT_LOAD
    /* load the elf */
    load_elf();
    /* execute the program */
    if(elfloader_autostart_processes) {
      PRINTF("%s file loaded\n", elf_filename);
      autostart_start(elfloader_autostart_processes);
      PRINTF("%s process executed\n", elf_filename);
      process_started = 1;
    }
#endif

    /* connect to the host */
    uip_udp_remove(connection);
    connection = udp_new(&host_ipaddr, UIP_HTONS(NOTIF_PORT), NULL);
    if(connection == NULL) {
      PRINTF("Couldn't open UDP connection\n");
      goto end;
    }

    /* send a request to notify the end of the process */
    PT_SPAWN(process_pt, &coap_client.pt, coap_client_request(&coap_client, COAP_GET, "notify", elf_filename, connection, appstore_final));

end:
    if(connection) {
      uip_udp_remove(connection);
    }
    if(fd != -1) {
      cfs_close(fd);
    }
    /* remove the file from coffee */
    cfs_remove(elf_filename);
    PRINTF("End.\n");
  }

  PROCESS_END();
}
