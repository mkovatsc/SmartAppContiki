all: border-router rest-server

ifndef TARGET
TARGET=sky
endif

CONTIKI=../..

# variable for root Makefile.include
UIP_CONF_IPV6=1

# variable for this Makefile
WITH_COAP=6

# radio configuration
CFLAGS += -DRF_CHANNEL=25
CFLAGS += -DIEEE802154_CONF_PANID=0xBEBE

# must be CFLAGS not variables
#CFLAGS += -DUIP_CONF_IPV6_RPL=1

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

ifdef RDC
CFLAGS += -DNETSTACK_CONF_RDC=$(RDC)
endif


APPS+=powertrace serial-shell
PROJECT_SOURCEFILES += slip-bridge.c static-routing.c

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
APPS += rest-http
endif

APPS += rest-engine

include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64

java-client: java_tools/COAPClient/src/COAPClient.java java_tools/COAP/src/org/sics/coap/COAPPacket.java
	javac -classpath java_tools/bin -d java_tools/bin java_tools/COAP/src/org/sics/coap/*.java
	javac -classpath java_tools/bin -d java_tools/bin java_tools/COAPClient/src/*.java
	
