all: border-router

CONTIKI=../../..

# useless, delete?
#WITH_UIP6=1

# variable for root Makefile.include
UIP_CONF_IPV6=1

#Override inclusion of internal webserver with make WITH_WEBSERVER=0
WITH_WEBSERVER=1

# radio configuration
CFLAGS += -DRF_CHANNEL=25
CFLAGS += -DIEEE802154_CONF_PANID=0xBEEF

# must be CFLAGS not variables, maybe put into project-conf.h?
CFLAGS += -DUIP_CONF_IPV6_RPL=1
CFLAGS += -DUIP_CONF_TCP=1 # if not set, default is TCP enabled, see uipopt.h

# linker optimizations
CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__

ifeq ($(WITH_WEBSERVER), 0)
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECT_SOURCEFILES += slip-bridge.c
else
APPS += webserver
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\" -DWEBSERVER
PROJECT_SOURCEFILES += slip-bridge.c httpd-simple.c
endif

include $(CONTIKI)/Makefile.include

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64
