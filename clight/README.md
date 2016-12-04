This is clight user service source code. 

It currently needs:  
* libxcb (xcb.h, xcb/dpms.h)
* libsystemd (systemd/sd-bus.h)
* libconfig (libconfig.h)
 
A systemd user unit is shipped too. Just run "systemctl --user enable clight.service" and reboot.

Current features:
* very lightweight
* fully valgrind clean
* external signals catching (sigint/sigterm)
* dpms support: it will check current screen powersave level and won't do anything if screen is currently off.
* a single capture mode from cmdline

Cmdline options:
* "-h/--help" to print an help message
* "-c/--capture" to do a single capture
* "-f/--frames" to change number of frames taken (defaults to 5)
* "-s/--setup" to create initial config file
* "-d/--device" to change webcam device
* "-b/--backlight" to change screen syspath
