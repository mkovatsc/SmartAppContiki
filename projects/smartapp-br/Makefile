CONTIKI_PROJECT=smartapp-br
all: $(CONTIKI_PROJECT) 

CONTIKI=../..

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
PROJECT_SOURCEFILES += slip-bridge.c

# variable for root Makefile.include
UIP_CONF_IPV6=1

# radio configuration
CFLAGS += -DRF_CHANNEL=21
CFLAGS += -DIEEE802154_CONF_PANID=0xCAFE
#CFLAGS += -DRF_CHANNEL=16
#CFLAGS += -DIEEE802154_CONF_PANID=0xBEEF

# must be CFLAGS not variables
CFLAGS += -DUIP_CONF_IPV6_RPL=1

# Enables DMA for the MSP430 to fix the problem of losing bytes with much traffic over SLIP
ifdef DEPLOY
${info INFO: enabling DMA for deployed border-router}
CFLAGS += -DCONF_DEPLOY_BR=$(DEPLOY)
endif

# linker optimizations
SMALL=1

#Simple built-in webserver is the default.
#Override with make WITH_WEBSERVER=0 for no webserver.
#WITH_WEBSERVER=webserver-name will use /apps/webserver-name if it can be
#found in the /apps, /platform/$(TARGET)/apps/, or current directory (in that order).
# WITH_WEBSERVER=webserver for /apps/webserver
# WITH_WEBSERVER=raven-webserver for /platform/avr-raven/apps/raven-webserver/
#make clean before changing webservers!

#Note /apps/webserver contains a 2500 byte style sheet which is a severe test
#of the slip connection. Large MSS together with low baud rates without flow
#control will overrun the transmit buffer when the style sheet is requested.

WITH_WEBSERVER = 1

ifeq ($(WITH_WEBSERVER),1)
CFLAGS += -DWEBSERVER=1
PROJECT_SOURCEFILES += httpd-simple.c
else ifneq ($(WITH_WEBSERVER), 0)
APPS += $(WITH_WEBSERVER)
CFLAGS += -DWEBSERVER=2
endif

ifeq ($(PORT), )
PORT = 1
endif

ifeq ($(DYN_CH), )
DYN_CH = 21
endif

include $(CONTIKI)/Makefile.include

tag.upload:
	mc1322x-load.pl -f smartapp-br_redbee-econotag.bin -t /dev/ttyUSB$(PORT)

tag.flash:
	mc1322x-load.pl -f /home/user/libmc1322x/tests/flasher_redbee-econotag.bin -s smartapp-br_redbee-econotag.bin -t /dev/ttyUSB$(PORT)

$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -c $(DYN_CH) -a 127.0.0.1 aaaa::1/64

connect-router-port:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -s /dev/ttyUSB$(PORT) aaaa::1/64

