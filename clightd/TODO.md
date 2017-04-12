TODO:

- [ ] add polkit checkAuthorization support. (MID) -> this way only active session can affect screen brightness/gamma (https://wiki.archlinux.org/index.php/Polkit#Actions)
- [ ] fix gettemp: it returns 6524 for 6500...find a way to deal with this approximation -> max_temp, min_temp and temperature int?
- [x] move back number_of_frame_captured support to clightd (it is very slow to re-init every resource for every capture.)
- [x] add debian/ubuntu deb package script
- [x] switch to devil to support non-jpeg webcam devices
