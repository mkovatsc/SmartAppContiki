all: rest-server-example coap-client-example

CONTIKI=../..

# useless, delete?
#WITH_UIP6=1

# variable for root Makefile.include
UIP_CONF_IPV6=1

# variable for this Makefile
WITH_COAP=1

# radio configuration
CFLAGS += -DRF_CHANNEL=25
CFLAGS += -DIEEE802154_CONF_PANID=0xBEEF

# must be CFLAGS not variables, maybe put into project-conf.h?
CFLAGS += -DUIP_CONF_IPV6_RPL=1
CFLAGS += -DUIP_CONF_TCP=0

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# linker optimizations
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

ifeq ($(WITH_COAP), 1)
CFLAGS += -DWITH_COAP
APPS += rest-coap
else
CFLAGS += -DWITH_HTTP
APPS += rest-http
endif

include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64
