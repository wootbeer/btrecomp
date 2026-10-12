// Matrix IDs for RT64's frame interpolation (Refresh Rate above the game's
// own frame rate), as in the sequel's Android port (btgarecomp,
// src/game/interpolation_ids.cpp there).
//
// Without IDs RT64 pairs each model with one in the previous frame by
// comparing position, orientation and screen position, and picks wrongly
// between similar models often enough to show: objects jitter back and
// forth, and parts flash. Tagging each object's matrix loads with a
// gEXMatrixGroup whose ID stays the same from frame to frame removes the
// guess.
//
// This game has no draw queue (the sequel added one). Models load their
// matrices through a family of small functions that convert the caller's
// float matrix to a fixed-point Mtx from the frame's pool (some cache it at
// float matrix + 0x40 for the rest of the frame) and write gSPMatrix straight
// into one of the frame's display lists: the main one (head at +0xD8 of the
// graphics state at *0x801257D0), the second one (+0xDC) or the small one
// (+0xE0). A hook in each, just before the gSPMatrix, writes a group to the
// same list:
//   - float matrix emitters: the float matrix's address is the ID. It sits at
//     the same address every frame for the same object. A float matrix that
//     produced more than one Mtx in a frame is shared scratch for several
//     objects, which says nothing about any one of them, so it stays on
//     RT64's automatic matching (the sequel learned this the hard way: one ID
//     for several objects made RT64 blend between them).
//   - ready-Mtx emitters: the Mtx's own address, unless it's in the per-frame
//     pool.
//   - the per-view camera matrix: automatic, so nothing after it inherits an
//     object's ID.
// Groups never push: each replaces the top of RT64's group stack, which RT64
// resets every frame and which the frame clear sets to automatic matching
// first (btx_interp_frame_base). Repeats of an ID are paired in order
// (G_EX_ORDER_LINEAR).
//
// Every group costs 16 bytes in a display list the game sized for itself, so
// tagging stops well before a list is full (with one automatic group, so
// nothing inherits an object's ID); the game's own draws are never squeezed.
#include <cstdint>
#include <unordered_map>

#include "recomp.h"

namespace {
    constexpr uint32_t kGfxStatePtr = 0x801257D0;
    constexpr uint32_t kIdAuto = 0xFFFFFFFFu; // G_EX_ID_AUTO
    constexpr uint32_t kGroupBytes = 16;

    // Frame slots at +0x90 + 0x18 * (frame index at +0xD0): list buffers at
    // +0x4 (main, 0x8000 bytes), +0x8 (second, 0x6000; the game itself stops
    // adding to it past 0x5C00) and +0xC (small, 0x400); the Mtx pool at
    // +0x14 (0xC000). Tag only while a list is under these.
    struct ListInfo {
        uint32_t head_field;  // in the graphics state
        uint32_t base_field;  // in the frame slot
        uint32_t tag_limit;   // bytes used, past which nothing more is tagged
    };
    constexpr ListInfo kLists[] = {
        { 0xD8, 0x4, 0x6000 },
        { 0xDC, 0x8, 0x4800 },
        { 0xE0, 0xC, 0x300 },
    };
    constexpr uint32_t kPoolField = 0x14;
    constexpr uint32_t kPoolSize = 0xC000;

    gpr kseg0(uint32_t address) {
        return (gpr)(int32_t)address;
    }

    gpr gfx_state(uint8_t* rdram) {
        return kseg0((uint32_t)MEM_W(0, kseg0(kGfxStatePtr)));
    }

    gpr frame_slot(uint8_t* rdram, gpr state) {
        uint32_t index = (uint16_t)MEM_HU(0xD0, state);
        return state + 0x90 + 0x18 * index;
    }

    bool in_pool(uint8_t* rdram, gpr state, uint32_t mtx) {
        for (uint32_t index = 0; index < 2; index++) {
            uint32_t base = (uint32_t)MEM_W(0x90 + 0x18 * index + kPoolField, state);
            if (mtx >= base && mtx < base + kPoolSize) {
                return true;
            }
        }
        return false;
    }

    // gEXMatrixGroupDecomposedNormal(id, push, G_MTX_MODELVIEW, G_EX_EDIT_NONE):
    // position, rotation, scale, skew, perspective and tiles always
    // interpolated, vertices and texcoords skipped, look-at automatic. Not
    // RT64's AUTO components (its default): AUTO position stops interpolating
    // on any frame where an object's speed jumps 10x over the last one, so
    // uneven per-frame motion flips between smoothed and snapped frames.
    uint32_t matrix_group_params(uint32_t id, bool push) {
        constexpr uint32_t kInterpolate = 1, kAuto = 2;
        const uint32_t order = (id == kIdAuto) ? 1u : 0u; // AUTO ordering for automatic, LINEAR for IDs
        return (push ? 1u : 0u) |   // push
            (0u << 1) |             // proj: modelview
            (1u << 2) |             // mode: decompose
            (kInterpolate << 3) |   // position
            (kInterpolate << 5) |   // rotation
            (kInterpolate << 7) |   // scale
            (kInterpolate << 9) |   // skew
            (kInterpolate << 11) |  // perspective
            (0u << 13) |            // vertices: skip
            (kInterpolate << 15) |  // tiles
            (order << 17) |         // ordering
            (0u << 19) |            // not editable
            (0u << 20) |            // aspect: auto
            (0u << 22) |            // texcoords: skip
            (kAuto << 24);          // look-at
    }

    // Per frame: float matrix -> the Mtx it produced (0 once it produced more
    // than one). Kept for the previous frame too, since a matrix that turned
    // out to be shared last frame shouldn't get an ID on its first use in
    // this one.
    struct Generation {
        std::unordered_map<uint32_t, uint32_t> source_mtx;
    };
    Generation generations[2];
    int current = 0;

    uint32_t id_for_float_matrix(uint32_t source, uint32_t mtx) {
        Generation& now = generations[current];
        const Generation& last = generations[current ^ 1];
        auto it = now.source_mtx.find(source);
        if (it == now.source_mtx.end()) {
            now.source_mtx.emplace(source, mtx);
        }
        else if (it->second != mtx) {
            it->second = 0;
        }
        auto prev = last.source_mtx.find(source);
        const bool shared_now = (now.source_mtx[source] == 0);
        const bool shared_last = (prev != last.source_mtx.end()) && (prev->second == 0);
        return (shared_now || shared_last) ? kIdAuto : source;
    }

    // Per list, whether tagging has stopped this frame (list nearly full).
    bool list_stopped[3];

    void write_group(uint8_t* rdram, int list, uint32_t id) {
        if (list_stopped[list]) {
            return;
        }
        const ListInfo& info = kLists[list];
        const gpr state = gfx_state(rdram);
        const uint32_t base = (uint32_t)MEM_W(info.base_field, frame_slot(rdram, state));
        const uint32_t head = (uint32_t)MEM_W(info.head_field, state);
        if (head < base || head >= base + info.tag_limit + kGroupBytes) {
            // Not this frame's list (or already past the limit): leave it be.
            list_stopped[list] = true;
            return;
        }
        if (head + kGroupBytes > base + info.tag_limit) {
            // Nearly full: one automatic group, so what follows doesn't keep
            // the last object's ID, then no more this frame.
            id = kIdAuto;
            list_stopped[list] = true;
        }
        const gpr h = kseg0(head);
        MEM_W(0x0, h) = (int32_t)0x6400000Cu; // G_EX_MATRIXGROUP_V1
        MEM_W(0x4, h) = (int32_t)id;
        MEM_W(0x8, h) = (int32_t)matrix_group_params(id, false);
        MEM_W(0xC, h) = 0;
        MEM_W(info.head_field, state) = (int32_t)(head + kGroupBytes);
    }

    int list_index(uint32_t head_field) {
        return head_field == 0xD8 ? 0 : head_field == 0xDC ? 1 : 2;
    }
}

// Float matrix emitters, just before the gSPMatrix: the float matrix and the
// Mtx made from it.
extern "C" void btx_interp_float_matrix(uint8_t* rdram, uint32_t head_field, uint32_t source, uint32_t mtx) {
    write_group(rdram, list_index(head_field), id_for_float_matrix(source, mtx));
}

// Ready-Mtx emitters: the Mtx's address is the object's, unless it's from
// the per-frame pool.
extern "C" void btx_interp_ready_matrix(uint8_t* rdram, uint32_t head_field, uint32_t mtx) {
    const bool pooled = in_pool(rdram, gfx_state(rdram), mtx);
    write_group(rdram, list_index(head_field), pooled ? kIdAuto : mtx);
}

// The per-view camera matrix: automatic matching from here on.
extern "C" void btx_interp_auto(uint8_t* rdram, uint32_t head_field) {
    write_group(rdram, list_index(head_field), kIdAuto);
}

// End of the frame clear (func_8007DC5C, head at 0x24($fp)), the first thing
// in the frame's master display list: enable RT64's extended GBI, set the
// base group to automatic matching with the components above (for
// everything drawn without one of the emitters: terrain and effects), and
// start a new generation.
extern "C" void btx_interp_frame_base(uint8_t* rdram, recomp_context* ctx) {
    current ^= 1;
    generations[current].source_mtx.clear();
    for (bool& stopped : list_stopped) {
        stopped = false;
    }
    const gpr frame = ctx->r30;
    const gpr h = kseg0((uint32_t)MEM_W(0x24, frame));
    MEM_W(0x00, h) = (int32_t)0xE0525464u; // gEXEnable
    MEM_W(0x04, h) = (int32_t)0x10000064u;
    MEM_W(0x08, h) = (int32_t)0x6400000Cu; // gEXMatrixGroup, no push: replaces the base entry
    MEM_W(0x0C, h) = (int32_t)kIdAuto;
    MEM_W(0x10, h) = (int32_t)matrix_group_params(kIdAuto, false);
    MEM_W(0x14, h) = 0;
    MEM_W(0x24, frame) = (int32_t)((uint32_t)h + 0x18);
}
