# Clight

Clight is a C daemon utility to automagically change screen backlight level to match ambient brightness.  
Ambient brightness is computed by capturing frames from webcam.

It is heavily insipred by [calise](http://calise.sourceforge.net/wordpress/), at least in its intents.

It currently needs:  
* v4l2 (linux/videodev2.h)
* libxcb (xcb.h, xcb/dpms.h)
* imagemagick (wand/MagickWand.h)
* polkit (runtime dep)

Optionally:  
* libsystemd (systemd/sd-journal.h) to log to systemd journal
 
Polkit is needed as otherwise clight won't be able to change your screen brightness as non-sudo user.  
Thus, "make install" will install a polkit policy too, and clight must be run with pkexec.

Current features:
* external signals catching (sigint/sigterm)
* dpms support: it will check current screen powersave level and won't do anything if screen is currently off.
* number of frames customizable from cmdline
* a single capture mode from cmdline

Cmdline options:
* "-c/--capture" to do a single capture
* "-f/--frames" to change number of frames taken (defaults to 5)
