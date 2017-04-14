# Clight

[![Build Status](https://travis-ci.org/FedeDP/Clight.svg?branch=master)](https://travis-ci.org/FedeDP/Clight)

Clight is a C daemon utility to automagically change screen backlight level to match ambient brightness.  
Ambient brightness is computed by capturing frames from webcam.  
Moreover, it can manage your screen temperature, just like redshift does.  

It was heavily inspired by [calise](http://calise.sourceforge.net/wordpress/) in its initial intents.  

It is splitted in 2 modules: clight and clightd.  
[Clight](https://github.com/FedeDP/Clight/tree/master/clight) is the user service to automagically change screen brightness every N minutes and to manage screen gamma temperature.  
[Clightd](https://github.com/FedeDP/Clight/tree/master/clightd) is the bus interface that is needed to change screen brightness and to capture webcame frames.  

For more details, check each module README.

## Deb packages
Deb packages for amd64 architecture are provided. You can find it inside [Debian](https://github.com/FedeDP/Clight/blob/master/COPYING) folder.  
Moreover, you can very easily build your own packages, if aforementioned packages happen to be outdated.  
You only need to move to your desired module (eg: clightd), and issue:

    $ make deb

A deb file will be created in [Debian](https://github.com/FedeDP/Clight/blob/master/COPYING) folder.  
Please note that while i am using Debian testing at my job, i am developing clight from archlinux.  
Thus, you can encounter some packaging issues. Please, report back.  

## License
This softwares are distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clight/blob/master/COPYING) file for more informations.
