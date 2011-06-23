/*
 * static-routing.c
 *
 *  Created on: Oct 12, 2010
 *      Author: simonduq
 */

#include "static-routing.h"

#define DEBUG 0
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

#include "contiki-net.h"
#include "node-id.h"

#define NNODES 5
int node_rank;

struct id_to_addrs {
    int id;
    uint32_t addr;
};

const struct id_to_addrs motes_addrs[] = {
/*
  aaaa::212:7400:1160:f62d        sky1
  aaaa::212:7400:0da0:d748        sky2
  aaaa::212:7400:116e:c325        sky3
  aaaa::212:7400:116e:c444        sky4
  aaaa::212:7400:115e:b717        sky5
*/
    {1, 0x1160f62d},
    {2, 0x0da0d748},
    {3, 0x116ec325},
    {4, 0x116ec444},
    {5, 0x115eb717},
};
//const struct id_to_addrs motes_addrs[] = {
//    {1, 0x10585e65},
//    {2, 0x10582cfb},
//    {3, 0x10584b6c},
//    {4, 0x112b037f},
//    {5, 0x112b0ae9},
//    {6, 0x10584222},
//    {7, 0x10572325},
//    {8, 0x105849d3},
//    {9, 0x105820a7},
//    {10, 0x116ed7f2},
//    {11, 0x115eb567},
//    {12, 0x116ec96d},
//};


uint32_t get_mote_suffix(int rank) {
    int i;
//    for(i=0; i<(sizeof(motes_addrs)/sizeof(struct id_to_addrs)); i++) {
//        if(id == motes_addrs[i].id) {
//            return motes_addrs[i].addr;
//        }
//    }
    rank--;
    if(rank >=0 && rank<(sizeof(motes_addrs)/sizeof(struct id_to_addrs))) {
      return motes_addrs[rank].addr;
    }
    return 0;
}

int get_mote_id(uint32_t suffix) {
#if IN_COOJA
    return suffix & 0xff;
#else
    int i;
    for(i=0; i<(sizeof(motes_addrs)/sizeof(struct id_to_addrs)); i++) {
      if(suffix == motes_addrs[i].addr) {
        return motes_addrs[i].id;
      }
    }
    return 0;
#endif
}

void set_global_address(void) {
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}

static void add_route_ext(int dest, int next) {
  PRINTF("add route ext %d %d\n", dest, next);
    uip_ipaddr_t ipaddr_dest, ipaddr_next;
    uip_ip6addr(&ipaddr_dest, 0xaaaa, 0, 0, 0, 0, 0, 0, dest);
#if IN_COOJA
    uip_ip6addr(&ipaddr_next, 0xfe80, 0, 0, 0, 0x0212, 0x7400 | next, next, next<<8 | next);
#else
    uint32_t next_suffix = get_mote_suffix(next);
    uip_ip6addr(&ipaddr_next, 0xfe80, 0, 0, 0, 0x0212, 0x7400, (next_suffix >> 16) & 0xffff, next_suffix & 0xffff);
#endif
    uip_ds6_route_add(&ipaddr_dest, 128, &ipaddr_next, 0);
}

static void add_route(int dest, int next) {
  PRINTF("add route %d %d\n", dest, next);
    uip_ipaddr_t ipaddr_dest, ipaddr_next;
#if IN_COOJA
    uip_ip6addr(&ipaddr_dest, 0xaaaa, 0, 0, 0, 0x0212, 0x7400 | dest, dest, dest<<8 | dest);
    uip_ip6addr(&ipaddr_next, 0xfe80, 0, 0, 0, 0x0212, 0x7400 | next, next, next<<8 | next);
#else
    uint32_t dest_suffix = get_mote_suffix(dest);
    uint32_t next_suffix = get_mote_suffix(next);
    uip_ip6addr(&ipaddr_dest, 0xaaaa, 0, 0, 0, 0x0212, 0x7400, (dest_suffix >> 16) & 0xffff, dest_suffix & 0xffff);
    uip_ip6addr(&ipaddr_next, 0xfe80, 0, 0, 0, 0x0212, 0x7400, (next_suffix >> 16) & 0xffff, next_suffix & 0xffff);
#endif
    uip_ds6_route_add(&ipaddr_dest, 128, &ipaddr_next, 0);
}

void configure_routing(void) {
  int i;

  cc2420_set_channel(15);

#if IN_COOJA
  node_rank = node_id;
#else
  node_rank = -1;
  for(i=0; i<(sizeof(motes_addrs)/sizeof(struct id_to_addrs)); i++) {
    if(node_id == motes_addrs[i].id) {
      node_rank = i+1;
      break;
    }
  }

  if(node_rank == -1) {
    PRINTF("unable to configure routing, node_id=%d\n", node_id);
    return;
  }
#endif

  PRINTF("configure_routing, node_id=%d, node_rank %d\n", node_id, node_rank);

  if (node_rank == 1) { /* border router #1 */
    int i;
    add_route_ext(2, 2);
    for(i=2; i<=NNODES; i++) {
      add_route(i, 2);
    }
  } else if (node_rank < NNODES) { /* other node */
    int i;
    add_route_ext(1, node_rank-1);
    add_route_ext(2, node_rank+1);
    for(i=1; i<=NNODES; i++) {
      if(i<node_rank) {
        add_route(i, node_rank-1);
      } else if(i>node_rank) {
        add_route(i, node_rank+1);
      }
    }
  } else if (node_rank == NNODES) { /* 2nd border router */
    int i;
    add_route_ext(1, NNODES-1);
    for(i=1; i<=NNODES-1; i++) {
      add_route(i, NNODES-1);
    }
  }
}
