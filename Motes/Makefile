CONTIKI_PROJECT = Data-Collector
all: $(CONTIKI_PROJECT)

CONTIKI_WITH_RIME = 1
TARGET_LIBFILES += -lm
APP += powertrace
MODULES += os/services/simple-energest
define ENERGEST_CONF_ON 1
endef


CONTIKI = ../..
include $(CONTIKI)/Makefile.include
