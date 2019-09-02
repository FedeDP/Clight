---
name: Custom Module proposal
about: Propose a new custom module to be installed together with clight, as skeleton
title: "[CUSTOM] "
labels: Contribution
assignees: FedeDP

---

**Custom Module's behaviour**
Why did you create this custom module? Ie: what's its purpose?

**Custom Module's code**
Share your module's code here!
```
#include <clight/public.h>

CLIGHT_MODULE("MYMOD");

static void init(void) {

}

static void receive(const msg_t *msg, const void *userdata) {
    switch (MSG_TYPE()) {
    default:
        break;
    }
}
```
