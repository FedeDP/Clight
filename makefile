BINDIR = /usr/bin
BINNAME = clight
SYSTEMDDIR = /usr/lib/systemd/user/
SYSTEMDUNIT = clight.service
SYSTEMDTIMER = clight.timer
DEBIANDIR = ./Clight
DEBOUTPUTDIR = ./Debian
EXTRADIR = Extra
CONFDIR = /etc/default
CONFNAME = clight.conf
ICONSDIR = /usr/share/icons/hicolor/scalable/apps
ICONNAME = clight.svg
DESKTOPDIR = /usr/share/applications
AUTOSTARTDIR = /etc/xdg/autostart
RM = rm -f
RMDIR = rm -rf
INSTALL = install -p
INSTALL_PROGRAM = $(INSTALL) -m755
INSTALL_DATA = $(INSTALL) -m644
INSTALL_DIR = $(INSTALL) -d
SRCDIR = src/
LIBS = -lm $(shell pkg-config --libs libsystemd popt gsl libconfig)
CFLAGS = $(shell pkg-config --cflags libsystemd popt gsl libconfig) -DCONFDIR=\"$(CONFDIR)\" -D_GNU_SOURCE -std=c11

ifeq (,$(findstring $(MAKECMDGOALS),"clean install uninstall"))

ifneq ("$(shell pkg-config --atleast-version=221 systemd && echo yes)", "yes")
$(error systemd minimum required version 221.)
endif

endif

CLIGHT_VERSION ?= $(shell git describe --abbrev=0 --always --tags)
SYSTEMD_VERSION = $(shell pkg-config --modversion systemd)
CFLAGS+=-DVERSION=\"$(CLIGHT_VERSION)\" -DLIBSYSTEMD_VERSION=$(SYSTEMD_VERSION)

all: clight clean

debug: clight-debug clean

objects:
	@cd $(SRCDIR); $(CC) -c *.c $(CFLAGS) -O3

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

	$(info installing systemd unit and timer.)
	@$(INSTALL_DIR) "$(DESTDIR)$(SYSTEMDDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/systemd/$(SYSTEMDUNIT) "$(DESTDIR)$(SYSTEMDDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/systemd/$(SYSTEMDTIMER) "$(DESTDIR)$(SYSTEMDDIR)"
	
	$(info installing icon.)
	@$(INSTALL_DIR) "$(DESTDIR)$(ICONSDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/icons/$(ICONNAME) "$(DESTDIR)$(ICONSDIR)"
	
	$(info installing desktop files.)
	@$(INSTALL_DIR) "$(DESTDIR)$(AUTOSTARTDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/desktop/clight.desktop "$(DESTDIR)$(AUTOSTARTDIR)"
	
	@$(INSTALL_DIR) "$(DESTDIR)$(DESKTOPDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/desktop/clightc.desktop "$(DESTDIR)$(DESKTOPDIR)"

uninstall:
	$(info uninstalling bin.)
	@$(RM) "$(DESTDIR)$(BINDIR)/$(BINNAME)"
	
	$(info uninstalling conf file.)
	@$(RM) "$(DESTDIR)$(CONFDIR)/$(CONFNAME)"

	$(info uninstalling systemd unit and timer.)
	@$(RM) "$(DESTDIR)$(SYSTEMDDIR)/$(SYSTEMDUNIT)"
	@$(RM) "$(DESTDIR)$(SYSTEMDDIR)/$(SYSTEMDTIMER)"
	
	$(info uninstalling icon.)
	@$(RM) "$(DESTDIR)$(ICONSDIR)/$(ICONNAME)"
	
	$(info uninstalling desktop files.)
	@$(RM) "$(DESTDIR)$(AUTOSTARTDIR)/clight.desktop"
	@$(RM) "$(DESTDIR)$(DESKTOPDIR)/clightc.desktop"
