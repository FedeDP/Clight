TODO before release:

## Low Priority:
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream)
- [ ] fix algorithm to set screen brightness based on ambient brightness (needs real world testing)
- [x] avoid printing "%d gamma value set" when smooth-transitioning
- [x] smooth_transition is useless. Just set a --no-smooth_transition flag (conf.no_smooth_transition).
- [x] add debian/ubuntu deb package script
