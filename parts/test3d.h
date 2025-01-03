#pragma once
#include <defs.h>

void test3d_init();     // MUST BE CALLED BEFORE running a part to allocate memory
void test3d_run();      // speaks for itself. blocks until part end because that's the design :p
void test3d_done();     // MUST BE CALLED AFTER  running a part to free memory
