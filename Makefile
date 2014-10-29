PREFIX = /usr/local

CDEBUGFLAGS = -Os -g -Wall

DEFINES = $(PLATFORM_DEFINES)

CFLAGS = $(CDEBUGFLAGS) $(DEFINES) $(EXTRA_DEFINES)

SRCS = mesh.c timers.c networks.c

OBJS = mesh.o timers.o networks.o

LDLIBS = -lrt

mesh: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o mesh $(OBJS) $(LDLIBS)

.PHONY: all install.minimal install

all: mesh

install.minimal: all

install: all install.minimal

.PHONY: uninstall

uninstall:

.PHONY: clean

clean:
	-rm -f mesh
	-rm -f *.o *~ core TAGS gmon.out
