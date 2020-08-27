PREFIX = /usr/local

CDEBUGFLAGS = -Os -g -Wall

DEFINES = $(PLATFORM_DEFINES)

CFLAGS = $(CDEBUGFLAGS) $(DEFINES) $(EXTRA_DEFINES)

SRCS = mesh.c timers.c networks.c errorcheck.c application.c

OBJS = mesh.o timers.o networks.o errorcheck.o application.o

LDLIBS = -lrt -lwiringPi

mesh: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o mesh $(OBJS) $(LDLIBS)

all: mesh

install: mesh
	install -m 775 -d $(PREFIX)/bin
	install -m 775 mesh $(PREFIX)/bin/mesh
ifeq ($(wildcard /etc/mesh.conf),)
	install -m 666 scripts/mesh.conf /etc/mesh.conf
endif
ifeq ($(wildcard /etc/systemd/system/mesh.service),)
	install -m 755 scripts/mesh.service /etc/systemd/system/mesh.service
endif

clear:
	rm -f /etc/mesh.conf
	rm -f /etc/systemd/system/mesh.service
	rm -f /var/log/mesh.log
	rm -f *.o *~ core TAGS gmon.out
