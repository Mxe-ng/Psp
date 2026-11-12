TARGET = PSPTeca
OBJS = main.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSPTeca

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
