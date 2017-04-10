TODO:

- [ ] add polkit checkAuthorization support. (MID) -> this way only active session can affect screen brightness/gamma (https://wiki.archlinux.org/index.php/Polkit#Actions)
- [x] fix getgamma for certain values of temp (eg : < 5950 and others)
- [ ] fix gettemp: it returns 6524 for 6500...find a way to deal with this approximation -> max_temp, min_temp and temperature int? Poi controllare altro... 
- [x] fix readme (use same layout as clight readme file)
