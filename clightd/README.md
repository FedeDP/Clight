This is clight's own bus interface. 

It currently needs:  
* libsystemd (systemd/sd-bus.h)
* libudev (libudev.h)

Needed only if built with gamma support:  
* libxrandr (X11/extensions/Xrandr.h)
* libx11 (X11/Xlib.h)

Neded only if built with frame captures support:  
* libjpeg-turbo (turbojpeg.h)
* linux-api-headers (linux/videodev2.h)

Build time switches:  
* DISABLE_FRAME_CAPTURES=1 (to disable frame captures support)
* DISABLE_GAMMA=1 (to disable gamma support)

You may ask why did i developed this solution. The answer is quite simple.  
In linux there is no simple and unified way of changing screen brightness.  
So, i thought it could be a good idea to develop a bus service that can be used by every other program.

My idea is that anyone can now implement something similar to clight without messing with videodev or libjpeg.
A clight bash replacement, using clightd, can be something like (pseudo-code):  

    $ current_br = busctl call org.clight.backlight /org/clight/backlight org.clight.backlight getbrightness s ""
    $ max_br = busctl call org.clight.backlight /org/clight/backlight org.clight.backlight getmaxbrightness s ""
    $ ambient_br = busctl call org.clight.backlight /org/clight/backlight org.clight.backlight captureframes si "" 5
    $ new_br = ambient_br * max_br
    $ busctl call org.clight.backlight /org/clight/backlight org.clight.backlight setbrightness si "" new_br
    
Note that passing an empty string as first parameter will make clightd use first subsystem matching device it finds (through libudev).  
It should be good to go in most cases.  

Bus interface methods:  
* *getbrightness* -> takes a backlight kernel interface (eg: intel_backlight) or nothing to just use first backlight kernel interface that libudev finds.  
Returns current brightness value (int).
* *getmaxbrightness* -> takes a backlight kernel interface (as above). Returns max supported brightness value for that interface (int).
* *setbrightness* -> takes a backlight kernel interface and a new value. Set the brightness value on that interface and returns new brightness value (int).  
Note that new brightness value is checked to be between 0 and max_brightness.
* *getactualbrightness* -> takes a backlight kernel interface. Returns actual brightness for that interface (int).

If built with gamma support:  
* *getgamma* -> returns current display temperature (int).
* *setgamma* -> takes a temperature value (int, between 1000 and 10000) and set display temperature. Returns newly setted display temperature (int).

If built with frame captures support:  
* *captureframes* -> takes a video sysname (eg: video0) and a number of frames to be captured. Returns average frame brightness (double).


**It does only support jpeg screen captures. So, if your webcam does not support jpeg frame captures, you are out of luck, sorry.**
