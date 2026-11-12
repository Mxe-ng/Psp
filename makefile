TARGET = NinjaTormenta
OBJS = main.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspdisplay -lpspge -lpspctrl -lpspaudio -lpsppower -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Ninja Tormenta

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
  200 audio.inc
  120 gfx.inc
  320 total
ok
