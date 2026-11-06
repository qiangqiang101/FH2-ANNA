# FH2 ANNA interface

Add entries and lists to [FH2 ANNA](https://github.com/qiangqiang101/FH2_ANNA)'s in-car menu from your own Forza Horizon 2 platform mod.

Installing this package puts `fh2anna.h` on your C/C++ project's include path:

```c
#include "fh2anna.h"

static void honk(void* user, const char* key) { /* ... */ }

/* from a thread of yours, after api->wait_for_mods(...) */
const FH2AnnaApi* anna = (const FH2AnnaApi*)api->get_interface(FH2ANNA_INTERFACE, 4);
if (anna) {
    FH2AnnaEntry e = {sizeof e};
    e.key       = "honkhonk.horn";   /* unique: start it with your mod's name */
    e.parent    = "car";             /* or NULL for the top list, or a list of yours */
    e.label     = "Honk";
    e.on_choose = honk;
    anna->add_entry(&e);
}
```

ANNA's own lists are `car`, `car.lights`, `car.doors`, `route` and `online`. Entries can be said as voice commands too, by their label. The header documents every field.

The package holds only the header: players install FH2 ANNA itself from its releases.
