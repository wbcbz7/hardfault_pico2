#pragma once
#include <defs.h>

void bmpdist_init();     // MUST BE CALLED BEFORE running a part to allocate memory
void bmpdist_run();      // speaks for itself. blocks until part end because that's the design :p
void bmpdist_done();     // MUST BE CALLED AFTER  running a part to free memory
