## Mid Priority:
- [ ] add an initial setup to ask user to eg: set desired screen backlight level matching current ambient brightness, max brightess captured from webcam (eg: ask him to switch on a torch on webcam lens), and min brightness captured (ask him to cover the webcam).

## Low Priority:
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream) [IN-PROGRESS]
- [x] upload deb packages after first release
- [ ] add an autostart script with "xhost + && clight" in /etc/xdg/autostart/


## Ideas
Use "idle" instead of dpms? https://www.freedesktop.org/wiki/Software/systemd/logind/  
PROS:  
* signal emitted by logind
* we can catch the signal in both X and wayland

CONS:  
* when session is idle, we have no guarantees that screen is switched off/not being used; even if all major video player will use a logind inhibitor

Catch active session signals from logind. If current session becomes not active, clight should be paused.  
