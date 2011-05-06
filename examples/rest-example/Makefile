all: rest-server-example coap-client-example rest-server-example.co

CONTIKI=../..

# variable for root Makefile.include
UIP_CONF_IPV6=1

# variable for this Makefile
WITH_COAP=3

# radio configuration
CFLAGS += -DRF_CHANNEL=21
CFLAGS += -DIEEE802154_CONF_PANID=0xBEEF

# must be CFLAGS not variables
CFLAGS += -DUIP_CONF_IPV6_RPL=1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# linker optimizations
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

 

ifeq ($(WITH_COAP), 3)
${info INFO: compiling with CoAP-03}
CFLAGS += -DWITH_COAP=3
CFLAGS += -DUIP_CONF_TCP=0
APPS += rest-coap-03
else ifeq ($(WITH_COAP), 1)
${info INFO: compiling with CoAP}
CFLAGS += -DWITH_COAP
CFLAGS += -DUIP_CONF_TCP=0
APPS += rest-coap
else
${info INFO: compiling with HTTP}
CFLAGS += -DWITH_HTTP
CFLAGS += -DUIP_CONF_TCP=1
APPS += rest-http
endif

include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64
