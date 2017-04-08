BINDIR = /usr/lib/clight
BINNAME = clightd
BUSCONFDIR = /etc/dbus-1/system.d/
BUSCONFNAME = org.clight.backlight.conf
BUSSERVICEDIR = /usr/share/dbus-1/system-services/
BUSSERVICENAME = org.clight.backlight.service
EXTRADIR = Script
RM = rm -f
INSTALL = install -p
INSTALL_PROGRAM = $(INSTALL) -m755
INSTALL_DATA = $(INSTALL) -m644
INSTALL_DIR = $(INSTALL) -d
SRCDIR = src/
LIBS = -lm $(shell pkg-config --libs libsystemd libudev)
CFLAGS = $(shell pkg-config --cflags libsystemd libudev)

ifeq (,$(findstring $(MAKECMDGOALS),"clean install uninstall"))

SYSTEMD_VERSION=$(shell pkg-config --modversion --silence-errors systemd || echo 0)
ifneq ($(shell test $(SYSTEMD_VERSION) -ge 221; echo $$?), 0)
$(error systemd minimum required version 221.)
endif

ifneq ("$(DISABLE_FRAME_CAPTURES)","1")
CFLAGS+=$(shell pkg-config --cflags libturbojpeg)
LIBS+=$(shell pkg-config --libs libturbojpeg)
$(info Frames capturing support enabled.)
else
CFLAGS+=-DDISABLE_FRAME_CAPTURES
$(info Frames capturing support disabled.)
endif

ifneq ("$(DISABLE_GAMMA)","1")
CFLAGS+=$(shell pkg-config --cflags x11 xrandr)
LIBS+=$(shell pkg-config --libs x11 xrandr)
$(info Gamma support enabled.)
else
CFLAGS+=-DDISABLE_GAMMA
$(info Gamma support disabled.)
endif

endif

all: clightd clean

debug: clightd-debug clean

objects:
	@cd $(SRCDIR); $(CC) -c *.c $(CFLAGS)

objects-debug:
	@cd $(SRCDIR); $(CC) -c *.c -Wall $(CFLAGS) -Wshadow -Wtype-limits -Wstrict-overflow -fno-strict-aliasing -Wformat -Wformat-security -g

clightd: objects
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

clightd-debug: objects-debug
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

clean:
	@cd $(SRCDIR); $(RM) *.o

install:
	$(info installing bin.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BINDIR)"
	@$(INSTALL_PROGRAM) $(BINNAME) "$(DESTDIR)$(BINDIR)"
	
	$(info installing dbus conf file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BUSCONFDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(BUSCONFNAME) "$(DESTDIR)$(BUSCONFDIR)"
	
	$(info installing dbus service file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BUSSERVICEDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(BUSSERVICENAME) "$(DESTDIR)$(BUSSERVICEDIR)"

uninstall:
	$(info uninstalling bin.)
	@$(RM) "$(DESTDIR)$(BINDIR)/$(BINNAME)"
	
	$(info uninstalling dbus conf file.)
	@$(RM) "$(DESTDIR)$(BUSCONFDIR)/$(BUSCONFNAME)"
	
	$(info uninstalling dbus service file.)
	@$(RM) "$(DESTDIR)$(BUSSERVICEDIR)/$(BUSSERVICENAME)"
