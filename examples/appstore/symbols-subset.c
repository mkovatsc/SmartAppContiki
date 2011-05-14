#include "loader/symbols.h"

int printf(const char *, ...);
int puts();
int sprintf(char *, const char *, ...);
int etimer_expired();
int etimer_restart();
int etimer_set();
int etimer_stop();
int packetbuf_copyfrom();
int rimeaddr_cmp();
int rimeaddr_copy();
int rimeaddr_node_addr();
int rimeaddr_null();
int rimeaddr_set_node_addr();
int unicast_close();
int unicast_open();
int unicast_send();
int udp_new();
int uip_udp_packet_send();
void leds_on(unsigned char leds);
void leds_off(unsigned char leds);
extern int *node_id;
extern struct uip_udp_conn *collect_conn;
extern const struct sensors_sensor light_sensor;
extern const struct sensors_sensor sht11_sensor;

const int symbols_nelts = 14;
const struct symbols symbols[] = {
{ "collect_conn", (void *)&collect_conn },
{ "etimer_expired", (void *)&etimer_expired },
{ "etimer_restart", (void *)&etimer_restart },
{ "etimer_set", (void *)&etimer_set },
{ "etimer_stop", (void *)&etimer_stop },
{ "leds_off", (void *)&leds_off },
{ "leds_on", (void *)&leds_on },
{ "light_sensor", (void *)&light_sensor },
{ "node_id", (void *)&node_id },
//{ "packetbuf_copyfrom", (void *)&packetbuf_copyfrom },
{ "printf", (void *)&printf },
{ "puts", (void *)&puts },
//{ "rimeaddr_cmp", (void *)&rimeaddr_cmp },
//{ "rimeaddr_node_addr", (void *)&rimeaddr_node_addr },
{ "sht11_sensor", (void *)&sht11_sensor },
//{ "sprintf", (void *)&sprintf },
{ "udp_new", (void *)&udp_new },
{ "uip_udp_packet_send", (void *)&uip_udp_packet_send },
//{ "unicast_close", (void *)&unicast_close },
//{ "unicast_open", (void *)&unicast_open },
//{ "unicast_send", (void *)&unicast_send },
{ (const char *)0, (void *)0} };
