#pragma once
#include "../defs.h"

void linetunnel_init();     // MUST BE CALLED BEFORE running a part to allocate memory
void linetunnel_run();      // speaks for itself. blocks until part end because that's the design :p
void linetunnel_done();     // MUST BE CALLED AFTER  running a part to free memory
