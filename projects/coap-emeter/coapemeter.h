/* coapemeter.h */

#include "contiki.h"
#include "settings.h"

// CoAP settings
#if WITH_COAP == 3
#include "er-coap-03.h"
#include "er-coap-03-transactions.h"
#include "er-coap-03-engine.h"
#elif WITH_COAP == 6
#include "er-coap-06.h"
#include "er-coap-06-transactions.h"
#include "er-coap-06-engine.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#include "er-coap-07-transactions.h"
#include "er-coap-07-engine.h"
#else
#error "CoAP version defined by WITH_COAP not implemented"
#endif

//eMeter settings
#define SMART_METER_ID                          1
#define SMART_METER_TOKEN                       "d0f77dd665b39dfafa681d89a0f9d24d" /* md5 hash of MAC address 00212EFFFF0003E1 */
#define EMETER_PUSH_INTERVAL                    4
//#define EMETER_SERVER_URL                     "http://webofenergy.inf.ethz.ch:8080/webofenergy/rest/measurement"
#define EMETER_SERVER_URL                       "http://sense.sics.se/streams/willist/test/"

#define COAP_HTTP_PROXY_SET_IPV6(ipaddr)        uip_ip6addr(ipaddr, 0x2001, 0x620, 0x8, 0x4985, 0x0, 0x0, 0x0, 0x2)
#define COAP_HTTP_PROXY_SERVER_PORT             UIP_HTONS(5684)

// erbium settings
#define PERIODIC_RESOURCE_INTERVAL              4
#define RESOURCE_TITLE                          "SCA"

// sml parser settings
#define SML_PARSER_UPDATE_INTERVAL              4

// system processes
PROCESS(sml_process, "psml");
PROCESS(coap_process, "pcoap");

AUTOSTART_PROCESSES(&sml_process, &coap_process);
