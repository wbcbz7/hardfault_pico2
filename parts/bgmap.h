#pragma once
#include <defs.h>

void bgmap_init();     // MUST BE CALLED BEFORE running a part to allocate memory
void bgmap_run();      // speaks for itself. blocks until part end because that's the design :p
void bgmap_done();     // MUST BE CALLED AFTER  running a part to free memory
