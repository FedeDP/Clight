# Custom Modules Skeletons

Here you can find a couple of small but hopefully self-explaining custom modules.  
You can change them as you like, or you can just use them.  

* inhibit_bl.skel -> will set 100% BL level when getting inhibited (eg: when start watching a movie) and will pause automatic BACKLIGHT calibration too.
As soon as inhibition disappears, it will take a quick capture and resume automatic calibration.
* nightmode.skel -> will just log new daytime value (eg "Day" or "Night"). It has a couple of commented lines to gracefully change DE theme at DAY/NIGHT.
* synctemp_bumblebee.skel -> it is a workaround for optimus laptop using bumblebee in conjunction with intel-virtual-output to handle an external monitor wired directly to nvidia gpu (through hdmi). It keeps temp in sync between external monitor and integrated one (as bumblebee starts a new X display on hdmi monitor), see https://github.com/FedeDP/Clight/issues/144
