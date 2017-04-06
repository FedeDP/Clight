TODO before release:

## High Priority:
- [ ] Very frequent capture at "event" changing (ie: during sunset or sunrise)

## Mid Priority:
- [ ] use sd_bus_call_async for captures? To make them async
- [ ] add settable timers for gamma temp events (eg --sunset 7:00 --sunshine 19:30)

## Low Priority:
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (need to make a PR upstream)
- [ ] fix algorithm to set screen brightness based on ambient brightness (needs real world testing)
