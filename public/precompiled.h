#pragma once

#include "osconfig.h"

#include "MemPool.h"
#include "engine.h"

#include "dlls.h"
#include "basetypes.h"

// A failed assert in the game dll is nearly always "this map entity is not
// what I expected" - a func_door_rotating with no distance key, a path_corner
// with no targetname, a trigger with health on it. Aborting the whole game
// over data we did not write is not acceptable, and release builds already
// skip every one of these checks because NDEBUG turns assert() into a no-op.
// Keep the check, write what failed into the log, and keep running.
#undef assert
#define assert( exp ) \
	( (exp) ? (void)0 : ALERT( at_console, "assert failed: %s:%d: %s\n", __FILE__, __LINE__, #exp ) )
