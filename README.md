# Clight

Clight is a C daemon utility to automagically change screen backlight level to match ambient brightness.  
Ambient brightness is computed by capturing frames from webcam.

It is heavily inspired by [calise](http://calise.sourceforge.net/wordpress/), at least in its intents.

It currently needs:  
* linux-api-headers (linux/videodev2.h)
* libxcb (xcb.h, xcb/dpms.h)
* libjpeg-turbo (turbojpeg.h)
* libconfig (libconfig.h)

Optionally:  
* libsystemd (systemd/sd-journal.h) to log to systemd journal
 
A systemd user unit is shipped too. Just run "systemctl --user enable clight.service" and reboot.

Current features:
* very lightweight
* fully valgrind clean
* external signals catching (sigint/sigterm) even during a frame capture
* dpms support: it will check current screen powersave level and won't do anything if screen is currently off.
* a single capture mode from cmdline

Cmdline options:
* "-h/--help" to print an help message
* "-c/--capture" to do a single capture
* "-f/--frames" to change number of frames taken (defaults to 5)
* "-s/--setup" to create initial config file
* "-d/--device" to change webcam device
* "-b/--backlight" to change screen syspath

*It does only support jpeg screen captures. So, if your webcam does not support jpeg frame captures, you are out of luck, sorry.*
