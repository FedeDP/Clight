BINDIR = /usr/bin
BINNAME = clight
SYSTEMDDIR = /usr/lib/systemd/user/
SYSTEMDUNIT = clight.service
EXTRADIR = Extra
RM = rm -f
INSTALL = install -p
INSTALL_PROGRAM = $(INSTALL) -m755
INSTALL_DATA = $(INSTALL) -m644
INSTALL_DIR = $(INSTALL) -d
SRCDIR = src/
LIBS =-lv4l2 -lm $(shell pkg-config --libs libturbojpeg xcb xcb-dpms libconfig)
CFLAGS = $(shell pkg-config --cflags libturbojpeg xcb xcb-dpms libconfig)

ifeq (,$(findstring $(MAKECMDGOALS),"clean install uninstall"))

LIBSYSTEMD=$(shell pkg-config --silence-errors --libs libsystemd)
ifneq ("$(LIBSYSTEMD)","")
LIBS+=$(LIBSYSTEMD)
CFLAGS+=-DSYSTEMD_PRESENT $(shell pkg-config --silence-errors --cflags libsystemd)
$(info systemd-journal support enabled.)
endif

endif

all: objects clight clean

debug: objects-debug clight-debug clean

objects:
	@cd $(SRCDIR); $(CC) -c *.c $(CFLAGS)

objects-debug:
	@cd $(SRCDIR); $(CC) -c *.c -Wall $(CFLAGS) -DDEBUG=1 -Wshadow -Wstrict-overflow -fno-strict-aliasing -Wformat -Wformat-security -g

clight: objects
	@cd $(SRCDIR); $(CC) -o ../clight *.o $(LIBS)

clight-debug: objects-debug
	@cd $(SRCDIR); $(CC) -o ../clight *.o $(LIBS)

clean:
	@cd $(SRCDIR); $(RM) *.o

install:
	$(info installing bin.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BINDIR)"
	@$(INSTALL_PROGRAM) $(BINNAME) "$(DESTDIR)$(BINDIR)"
	
	$(info installing systemd unit.)
	@$(INSTALL_DIR) "$(DESTDIR)$(SYSTEMDDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(SYSTEMDUNIT) "$(DESTDIR)$(SYSTEMDDIR)"

uninstall:
	$(info uninstalling bin.)
	@$(RM) "$(DESTDIR)$(BINDIR)/$(BINNAME)"
	
	$(info uninstalling systemd unit.)
	@$(RM) "$(DESTDIR)$(SYSTEMDDIR)/$(SYSTEMDUNIT)"
