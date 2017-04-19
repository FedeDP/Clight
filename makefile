BINDIR = /usr/bin
BINNAME = clight
SYSTEMDDIR = /usr/lib/systemd/user/
SYSTEMDUNIT = clight.service
DEBIANDIR = ./Clight
DEBOUTPUTDIR = ./Debian
EXTRADIR = Extra
CONFDIR = /etc/default
CONFNAME = clight.conf
RM = rm -f
RMDIR = rm -rf
INSTALL = install -p
INSTALL_PROGRAM = $(INSTALL) -m755
INSTALL_DATA = $(INSTALL) -m644
INSTALL_DIR = $(INSTALL) -d
SRCDIR = src/
LIBS = -lm $(shell pkg-config --libs xcb xcb-dpms libsystemd popt libconfig)
CFLAGS = $(shell pkg-config --cflags xcb xcb-dpms libsystemd popt libconfig) -DCONFDIR=\"$(CONFDIR)\"

ifeq (,$(findstring $(MAKECMDGOALS),"clean install uninstall"))

ifneq ("$(shell pkg-config --atleast-version=221 systemd && echo yes)", "yes")
$(error systemd minimum required version 221.)
endif

endif

CLIGHT_VERSION = $(shell git describe --abbrev=0 --always --tags)
CFLAGS+=-DVERSION=\"$(CLIGHT_VERSION)\"

all: clight clean

debug: clight-debug clean

objects:
	@cd $(SRCDIR); $(CC) -c *.c $(CFLAGS)

objects-debug:
	@cd $(SRCDIR); $(CC) -c *.c -Wall $(CFLAGS) -Wshadow -Wstrict-overflow -Wtype-limits -fno-strict-aliasing -Wformat -Wformat-security -g

clight: objects
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

clight-debug: objects-debug
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

clean:
	@cd $(SRCDIR); $(RM) *.o

deb: all install-deb build-deb clean-deb

install-deb: DESTDIR=$(DEBIANDIR)
install-deb: install

build-deb:
	$(info setting deb package version.)
	@sed -i 's/Version:.*/Version: $(CLIGHT_VERSION)/' ./DEBIAN/control
	$(info copying debian build script.)
	@cp -r DEBIAN/ $(DEBIANDIR)
	@$(INSTALL_DIR) $(DEBOUTPUTDIR)
	$(info creating debian package.)
	@dpkg-deb --build $(DEBIANDIR) $(DEBOUTPUTDIR)

clean-deb:
	$(info cleaning up.)
	@$(RMDIR) $(DEBIANDIR)

install:
	$(info installing bin.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BINDIR)"
	@$(INSTALL_PROGRAM) $(BINNAME) "$(DESTDIR)$(BINDIR)"
	
	$(info installing conf file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(CONFDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(CONFNAME) "$(DESTDIR)$(CONFDIR)"

	$(info installing systemd unit.)
	@$(INSTALL_DIR) "$(DESTDIR)$(SYSTEMDDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(SYSTEMDUNIT) "$(DESTDIR)$(SYSTEMDDIR)"

uninstall:
	$(info uninstalling bin.)
	@$(RM) "$(DESTDIR)$(BINDIR)/$(BINNAME)"
	
	$(info uninstalling conf file.)
	@$(RM) "$(DESTDIR)$(CONFDIR)/$(CONFNAME)"

	$(info uninstalling systemd unit.)
	@$(RM) "$(DESTDIR)$(SYSTEMDDIR)/$(SYSTEMDUNIT)"
