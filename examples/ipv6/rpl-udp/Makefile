all: udp-client udp-server
APPS=servreg-hack
CONTIKI=../../..

UIP_CONF_IPV6=1

# must be CFLAGS not variables
CFLAGS += -DUIP_CONF_IPV6_RPL=1

# radio configuration
CFLAGS += -DRF_CHANNEL=15
CFLAGS += -DIEEE802154_CONF_PANID=0xBEEF

include $(CONTIKI)/Makefile.include
