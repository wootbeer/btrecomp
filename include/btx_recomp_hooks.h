#ifndef BTX_RECOMP_HOOKS_H
#define BTX_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt):
// declarations for the native functions the [[patches.hook]] text in
// battletanx.us.rev0.toml calls.

#include <stdint.h>
#include "recomp.h"

#ifdef __cplusplus
extern "C" {
#endif

// src/main/scheduler_workaround.cpp: lets other game threads run, from inside
// a loop that waits on one of them.
void btx_yield_via_priority_drop(uint8_t* rdram);

// src/game/widescreen.cpp
void btx_widen_cull(uint8_t* rdram, recomp_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // BTX_RECOMP_HOOKS_H
