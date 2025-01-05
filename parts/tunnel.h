#pragma once
#include <defs.h>

void tunnel_init();     // MUST BE CALLED BEFORE running a part to allocate memory
void tunnel_run();      // speaks for itself. blocks until part end because that's the design :p
void tunnel_done();     // MUST BE CALLED AFTER  running a part to free memory
