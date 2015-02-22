CONTIKI_PROJECT = coap-test
all: $(CONTIKI_PROJECT)

# Enable Over-The-Air upgrade
CONTIKI_WITH_OTA=1

# Network Mode
CONTIKI_WITH_XBEE_NODE=1

# Add DNS64 support
CONTIKI_WITH_DNS64=1

# Add REST implementation
REST=coap_rest_implementation

# Add CoAP engine
APPS += er-coap rest-engine

CONTIKI = ../..
include $(CONTIKI)/Makefile.include
