# Clight

Clight is a C daemon utility to automagically change screen backlight level to match ambient brightness.  
Ambient brightness is computed by capturing frames from webcam.

It is heavily inspired by [calise](http://calise.sourceforge.net/wordpress/), at least in its intents.

It is splitted in 2 modules: clight and clightd.  
*Clight* is user service to automagically change screen brightness every N minutes.  
**Clightd** is the bus interface that is needed to change screen brightness and to capture webcame frames.  

For more details, check each module README.
