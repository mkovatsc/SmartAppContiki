all: appstore

ifndef TARGET
TARGET=sky
endif

CONTIKI=../..

PROJECT_SOURCEFILES += coap-client.c symbols-subset.c 

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

# REST framework, requires WITH_COAP
ifeq ($(WITH_COAP), 3)
${info INFO: compiling with CoAP-03}
CFLAGS += -DWITH_COAP=3
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += coap-03
else ifeq ($(WITH_COAP), 1)
${info INFO: compiling with rest-CoAP}
CFLAGS += -DWITH_COAP
CFLAGS += -DUIP_CONF_TCP=0
APPS += rest-coap
else
${info INFO: compiling with HTTP}
CFLAGS += -DWITH_HTTP
CFLAGS += -DUIP_CONF_TCP=1
APPS += rest-http
endif
APPS += rest-common

CUSTOM_RULE_C_TO_CO = 1

%.co: %.c
	$(CC) $(CFLAGS) $(XCFLAGS) -DAUTOSTART_ENABLE -c $< -o $@

include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64
	
run-appstore-server:
	java -cp java_tools/bin COAPServer market 2222 1

%.ce: %.c
	$(CC) $(CFLAGS) -DAUTOSTART_ENABLE -c $^ -o $@
	$(STRIP) --strip-unneeded -g -x $@

run-appstore-client: $(APP).ce
	ping6 $(DEST) -c 1 -w 120 && java -cp java_tools/bin COAPClient $(DEST) get loader $(APP).ce && java -cp java_tools/bin COAPServer notify 3333 0
