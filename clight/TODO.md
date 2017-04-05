TODO before release:

## Screen brightness algorithm
- [ ] fix algorithm to set screen brightness based on ambient brightness (low priority | needs real world testing)
- [ ] Very frequent capture at "event" changing (ie: during sunset or sunrise)  (high priority)

## Generic
- [ ] follow https://github.com/systemd/systemd/issues/5654 (next systemd release?) (low priority | code cosmetic)
- [ ] use sd_bus_call_async for captures? To make them async (mid priority)
