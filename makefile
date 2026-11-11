TARGET = minijuego
OBJS = main.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

# Bibliotecas gráficas
LIBS = -lpspgu -lpspgum -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Naruto Battle 3D
PSP_EBOOT_ICON = icon0.png
PSP_EBOOT_PIC1 = pic1.png

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
