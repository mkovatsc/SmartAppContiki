all: border-router

CONTIKI=../../..

# variable for root Makefile.include
UIP_CONF_IPV6=1

# Override inclusion of internal webserver with make WITH_WEBSERVER=0
WITH_WEBSERVER=1

# radio configuration
CFLAGS += -DRF_CHANNEL=21
CFLAGS += -DIEEE802154_CONF_PANID=0xBEEF

# must be CFLAGS not variables
CFLAGS += -DUIP_CONF_IPV6_RPL=1

# Enables DMA for the MSP430 to fix the problem of losing bytes with much traffic over SLIP
ifdef DEPLOY
${info INFO: enabling DMA for deployed border-router}
CFLAGS += -DCONF_DEPLOY_BR=$(DEPLOY)
endif

# linker optimizations
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

ifeq ($(WITH_WEBSERVER), 0)
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECT_SOURCEFILES += slip-bridge.c
else
APPS += webserver
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\" -DWEBSERVER
CFLAGS += -DUIP_CONF_TCP=1
PROJECT_SOURCEFILES += slip-bridge.c httpd-simple.c
endif

ifeq ($(TAG_PORT), )
TAG_PORT = 1
endif

include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64

connect-router-tag:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -s /dev/ttyUSB$(TAG_PORT) aaaa::1/64

