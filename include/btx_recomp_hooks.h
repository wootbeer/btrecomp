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

// src/game/hud_anchor.cpp
void btx_hud_anchor(uint8_t* rdram, recomp_context* ctx);

// src/game/interpolation_ids.cpp
void btx_interp_float_matrix(uint8_t* rdram, uint32_t head_field, uint32_t source, uint32_t mtx);
void btx_interp_ready_matrix(uint8_t* rdram, uint32_t head_field, uint32_t mtx);
void btx_interp_effect_sprite(uint8_t* rdram, uint32_t node);
void btx_frame_dt(uint8_t* rdram, recomp_context* ctx);
void btx_interp_auto(uint8_t* rdram, uint32_t head_field);
void btx_interp_frame_base(uint8_t* rdram, recomp_context* ctx);

#ifdef __cplusplus
}
#endif

#endif // BTX_RECOMP_HOOKS_H
