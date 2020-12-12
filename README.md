# Clight [![builds.sr.ht status](https://builds.sr.ht/~fededp/clight.svg)](https://builds.sr.ht/~fededp/clight?)

[![Packaging status](https://repology.org/badge/vertical-allrepos/clight.svg)](https://repology.org/project/clight/versions)

Clight is a C user daemon utility that aims to fully manage your display.  
It will automagically change screen backlight level to match ambient brightness, as computed by capturing frames from webcam or from an Ambient Light Sensor (ALS).  
Moreover, it can manage your screen temperature, just like redshift does.  
Finally, it can dim your screen after a timeout and manage screen DPMS.  

Note that all its features are available on both X, Wayland or tty.  
On wayland clight requires specific protocols to be implemented in your compositor; have a look at https://github.com/FedeDP/Clight/wiki/Modules#wayland-support.  

It was heavily inspired by [calise](http://calise.sourceforge.net/wordpress/) in its initial intents.  

Clight makes use of [Clightd](https://github.com/FedeDP/Clightd), a system DBus service that exposes an [API](https://github.com/FedeDP/Clightd/wiki/Api) to manage various aspects of your screen and allows Webcam/ALS devices captures.  
Its API is as generic as possible and it has nothing specifically for clight; this means anyone can use it;  
If you are interested, please have a look at its wiki pages too!  
I even developed a super simple clight clone as an hello world application in Go: https://github.com/FedeDP/GoLight.  
It is much simpler than Clight for obvious reasons and i do not expect to ever develop it further.  

**For a guide on how to build, features and lots of other infos, head to [Clight Wiki Pages](https://github.com/FedeDP/Clight/wiki).**  
**Note that Wiki pages will always refer to master branch.**  
*For any other info, please feel free to open an issue.*  

## Arch AUR packages
Clight is available on AUR as both stable or devel package: https://aur.archlinux.org/packages/?K=clight .  
Note that devel package may break in some weird ways during development. Use it at your own risk.  
Moreover, some brave distros are already shipping Clight as you can see from above packaging badge. Special thanks to all mantainers!

## License
This software is distributed with GPL license, see [COPYING](https://github.com/FedeDP/Clight/blob/master/COPYING) file for more informations.
