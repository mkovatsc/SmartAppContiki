all: appstore temp.ce light.ce humidity.ce

ifndef TARGET
TARGET=sky
endif

CONTIKI=../..

PROJECT_SOURCEFILES += symbols-subset.c static-routing.c

# variable for root Makefile.include
UIP_CONF_IPV6=1

# variable for this Makefile
WITH_COAP=6

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
ifeq ($(WITH_COAP), 6)
${info INFO: compiling with CoAP-06}
CFLAGS += -DWITH_COAP=6
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += coap-06
else ifeq ($(WITH_COAP), 3)
${info INFO: compiling with CoAP-03}
CFLAGS += -DWITH_COAP=3
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS += coap-03
else
${info INFO: compiling with HTTP}
CFLAGS += -DWITH_HTTP
CFLAGS += -DUIP_CONF_TCP=1
APPS += rest-http-engine
endif

APPS += erbium

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

appstore-server: java_tools/COAPAppstoreServer/src/COAPAppstoreServer.java java_tools/COAP/src/org/sics/coap/COAPPacket.java
	javac -classpath java_tools/bin -d java_tools/bin java_tools/COAP/src/org/sics/coap/*.java
	javac -classpath java_tools/bin -d java_tools/bin java_tools/COAPAppstoreServer/src/*.java

run-appstore-server: appstore-server
	java -cp java_tools/bin COAPAppstoreServer $(WITH_COAP) market 2222 1

%.ce: %.c
	$(CC) $(CFLAGS) -DAUTOSTART_ENABLE -c $^ -o $@
	$(STRIP) --strip-unneeded -g -x $@

run-appstore-client: $(APP).ce
	ping6 $(DEST) -c 1 -w 120 && java -cp java_tools/bin COAPClient $(DEST) get loader $(APP).ce && java -cp java_tools/bin COAPServer notify 3333 0
