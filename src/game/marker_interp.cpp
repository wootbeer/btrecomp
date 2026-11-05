// Smooth radar arrows at higher refresh rates, as in the sequel's port
// (btgarecomp src/game/marker_interp.cpp, its rounds 152-154).
//
// With radar, func_80100550 (called from the HUD functions, func_800F6934
// for one player) draws a 5 x 5 arrow over every other tank, at the tank's
// projected screen position (func_8009453C), as a texture rectangle in the
// HUD's own display list (head at 0x8037A174, hud_anchor.cpp). RT64
// interpolates 3D and orthographic draws between game frames by pairing their
// matrices, but never rectangles, so above the game's frame rate the tanks
// moved smoothly and their arrows stepped along at the game's rate.
//
// So when the function returns, each arrow's rectangle (G_TEXRECT,
// G_RDPHALF_1, G_RDPHALF_2) becomes a call to a small display list (G_DL and
// two no-ops, so nothing moves in the game's list) that draws the same
// texels as two triangles under an orthographic projection, moved into place
// by a modelview matrix in a matrix group with its own ID. RT64 then
// interpolates that matrix like any object's. An arrow keeps its ID from
// frame to frame by pairing it with the previous frame's arrow of the same
// colour (the prim colour before it: each team has its own) nearest to where
// that one's last movement would take it; one with no partner nearby gets a
// new ID and is drawn where it is.
//
// The texture load, prim colour and combiner the game set for the rectangle
// stay in front of the call; the list only adds what triangles need, in
// this game's microcode, F3DEX 1.21 (encodings as RT64 parses them,
// lib/rt64/src/gbi/rt64_gbi_f3d.cpp and rt64_gbi_f3dex.cpp). The game
// doesn't clamp the rectangle: an arrow left of or above the screen has its
// coordinates wrapped in the 12-bit fields, which sign-extend back to where
// it really is.
//
// Widescreen: the arrows are at projected 3D positions, so they're never
// anchored with the HUD (once converted they aren't rectangles to
// hud_anchor.cpp), and each list widens the scissor to the whole window so
// arrows over tanks in the wider view aren't cut at the 4:3 edge.
//
// The lists, matrices and vertices go in the last 128 KB of RDRAM: the game
// uses only the first 4 MB of the 8 (checked: the upper half stays zero
// through boot, the menus and a mission). Rotating slots, one per call, so a
// list isn't overwritten while RT64 may still read it.
#include <cmath>
#include <cstdint>
#include <vector>

#include "recomp.h"

namespace {
    constexpr uint32_t kHudHead = 0x8037A174;

    constexpr uint32_t kScratchStart = 0x807E0000;
    constexpr uint32_t kSlotSize = 0x4000;
    constexpr uint32_t kSlotCount = 8;

    constexpr uint32_t kIdBase = 0x4D4B0000; // 'MK', clear of the IDs interpolation_ids.cpp uses
    // How far (pixels) an arrow may be from where its last movement would
    // put it and keep its ID: fast camera turns move every arrow tens of
    // pixels a frame.
    constexpr int kPairDistance = 32;
    constexpr size_t kMaxCalls = 4;          // calls per frame (one per player view)

    // F3DEX 1.21.
    constexpr uint32_t kOpDl = 0x06, kOpEndDl = 0xB8, kOpTexRect = 0xE4;
    constexpr uint32_t kOpRdpHalf1 = 0xB4, kOpRdpHalf2 = 0xB3, kOpNoOp = 0xC0;
    constexpr uint32_t kOpSetPrimColor = 0xFA;

    gpr kseg0(uint32_t address) {
        return (gpr)(int32_t)address;
    }

    struct Marker {
        int x, y;   // upper left, pixels
        int vx, vy; // movement since the previous frame
        uint32_t rgb;
        uint32_t id;
    };

    std::vector<Marker> prev_frame[kMaxCalls], cur_frame[kMaxCalls];
    size_t call_index = 0;
    uint32_t next_id = 0;
    uint32_t slot = 0;
    uint32_t call_start = 0;

    void pair(size_t call, Marker& m) {
        if (call < kMaxCalls) {
            std::vector<Marker>& prev = prev_frame[call];
            int best = -1, best_dist = kPairDistance * kPairDistance + 1;
            for (size_t i = 0; i < prev.size(); i++) {
                if (prev[i].rgb != m.rgb) {
                    continue;
                }
                int dx = prev[i].x + prev[i].vx - m.x, dy = prev[i].y + prev[i].vy - m.y;
                int d = dx * dx + dy * dy;
                if (d < best_dist) {
                    best = (int)i;
                    best_dist = d;
                }
            }
            if (best >= 0) {
                m.id = prev[best].id;
                m.vx = m.x - prev[best].x;
                m.vy = m.y - prev[best].y;
                prev.erase(prev.begin() + best);
                return;
            }
        }
        next_id = (next_id + 1) & 0xFFFF;
        m.id = kIdBase | next_id;
        m.vx = m.vy = 0;
    }

    struct Writer {
        uint8_t* rdram;
        uint32_t addr, end;
        bool ok() const { return addr <= end; }
        void w(uint32_t value) {
            if (addr + 4 <= end) {
                MEM_W(0, kseg0(addr)) = (int32_t)value;
            }
            addr += 4;
        }
        void cmd(uint32_t w0, uint32_t w1) { w(w0); w(w1); }
        void h(uint16_t value) {
            if (addr + 2 <= end) {
                MEM_H(0, kseg0(addr)) = (int16_t)value;
            }
            addr += 2;
        }
        void b(uint8_t value) {
            if (addr + 1 <= end) {
                MEM_B(0, kseg0(addr)) = (int8_t)value;
            }
            addr += 1;
        }
    };

    // An N64 Mtx (s15.16, integer halves then fractions) from a row-major float matrix.
    void write_mtx(Writer& out, const float m[4][4]) {
        int32_t fixed[16];
        for (int i = 0; i < 16; i++) {
            fixed[i] = (int32_t)std::lround(m[i / 4][i % 4] * 65536.0f);
        }
        for (int i = 0; i < 16; i++) {
            out.h((uint16_t)((uint32_t)fixed[i] >> 16));
        }
        for (int i = 0; i < 16; i++) {
            out.h((uint16_t)((uint32_t)fixed[i] & 0xFFFF));
        }
    }

    // Vertex coordinates are in quarter pixels (the rectangle's 10.2 units),
    // so the modelview scales by 1/4 on top of the projection's own factors.
    // The projection maps x 0..256 to -1..1 and y 0..256 to 1..-1 (powers of
    // two, exact in s15.16); the modelview brings 320 x 240 to that.
    constexpr float kScaleX = 256.0f / 320.0f / 4.0f;
    constexpr float kScaleY = 256.0f / 240.0f / 4.0f;

    // A 12-bit 10.2 coordinate, sign-extended (the game wraps negative ones).
    int coord(uint32_t field) {
        return (field & 0x800) ? (int)field - 0x1000 : (int)field;
    }

    enum class Result { Converted, Skipped, Full };

    // Replaces the rectangle at `rect` (G_TEXRECT, G_RDPHALF_1, G_RDPHALF_2),
    // drawn in prim colour `prim`, with a call to a list drawn at `out`.
    Result convert(uint8_t* rdram, uint32_t rect, uint32_t prim, Writer& out, size_t call) {
        const uint32_t w0 = (uint32_t)MEM_W(0, kseg0(rect));
        const uint32_t w1 = (uint32_t)MEM_W(4, kseg0(rect));
        const uint32_t st = (uint32_t)MEM_W(12, kseg0(rect));
        const uint32_t dsdt = (uint32_t)MEM_W(20, kseg0(rect));
        const int tile = (w1 >> 24) & 7;
        const int ulx = coord((w1 >> 12) & 0xFFF), uly = coord(w1 & 0xFFF);
        const int lrx = coord((w0 >> 12) & 0xFFF), lry = coord(w0 & 0xFFF);
        const int16_t s = (int16_t)(st >> 16), t = (int16_t)(st & 0xFFFF);
        const int16_t dsdx = (int16_t)(dsdt >> 16), dtdy = (int16_t)(dsdt & 0xFFFF);
        const int w = lrx - ulx, h = lry - uly;
        // 1-cycle rectangles only (copy mode steps 4.0 and can't draw triangles).
        if (w <= 0 || h <= 0 || w > 256 || h > 256 || dsdx != 0x0400 || dtdy != 0x0400) {
            return Result::Skipped;
        }
        // Texture coordinates (s10.5) at the corners (dsdx is s5.10 per
        // pixel; coordinates are in quarter pixels). The sequel's port starts
        // the quad half a pixel's step earlier, since a rectangle samples
        // pixel i at s + i * dsdx and a triangle at the pixel's centre; here
        // that put the arrow's texels half a texel off, and without it the
        // quad matches the rectangle pixel for pixel (compared in the
        // sandbox at 3x resolution).
        const int16_t s0 = s, t0 = t;
        const int16_t s1 = (int16_t)(s0 + (w * dsdx) / 128);
        const int16_t t1 = (int16_t)(t0 + (h * dtdy) / 128);

        const uint32_t start = out.addr;
        constexpr uint32_t kListSize = 30 * 8;
        const uint32_t viewport = start + kListSize;
        const uint32_t proj = viewport + 16;
        const uint32_t model = proj + 64;
        const uint32_t vtx = model + 64;
        const uint32_t total = vtx + 4 * 16 - start;
        if (start + total > out.end) {
            return Result::Full;
        }

        Marker marker{ ulx / 4, uly / 4, 0, 0, prim >> 8, 0 };
        pair(call, marker);
        const uint32_t id = marker.id;
        if (call < kMaxCalls) {
            cur_frame[call].push_back(marker);
        }

        auto phys = [](uint32_t a) { return a & 0x1FFFFFFF; };
        out.cmd(0x00525464, 0x10000064);               // gEXEnable (F3DEX 1.21: G_SPNOOP is 0x00)
        out.cmd(0x64000017, 0);                        // gEXPushScissor
        // gEXSetScissor(G_SC_NON_INTERLACE, LEFT, RIGHT, 0, 0, 0, 240): the whole window.
        out.cmd(0x64000005, (0x0u << 2) | (0x400u << 14));
        out.cmd(0, 240u * 4);
        out.cmd(0x64000015, 0);                        // gEXPushViewport
        out.cmd(0x03800010, phys(viewport));           // gSPViewport (full screen)
        out.cmd(0x6400001D, 0);                        // gEXPushProjectionMatrix
        out.cmd(0x01030040, phys(proj));               // gSPMatrix(G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH)
        out.cmd(0x64000029, 0);                        // gEXPushGeometryMode
        // gSPClearGeometryMode(z-buffer, shade, smooth shading, culling, fog,
        // lighting, texture gen, texture gen linear).
        out.cmd(0xB6000000, 0x00000001u | 0x00000004u | 0x00000200u | 0x00003000u | 0x00010000u | 0x00020000u | 0x00040000u | 0x00080000u);
        out.cmd(0xBB000001 | ((uint32_t)tile << 8), 0xFFFFFFFF); // gSPTexture(0xFFFF, 0xFFFF, 0, tile, G_ON)
        // gDPSetTexturePersp(G_TP_PERSP): the HUD draws with G_TP_NONE,
        // under which RT64 halves triangle texture coordinates (as the
        // hardware does without the perspective divide) but not a
        // rectangle's. And gDPSetTextureFilter(G_TF_POINT): the rectangle
        // landed on texel centres, so bilinear filtering never showed; at
        // the in-between positions it would blur the pixel art.
        out.cmd(0x64000019, 0);                        // gEXPushOtherMode
        out.cmd(0xBA001301, 0x00080000);
        out.cmd(0xBA000C02, 0x00000000);
        // gEXMatrixGroupSimple(id, G_EX_PUSH, G_MTX_MODELVIEW, position
        // interpolated, everything else skipped, order linear, edit none).
        out.cmd(0x6400000C, id);
        out.cmd(0x1 | (1u << 3), 0);
        out.cmd(0x01060040, phys(model));              // gSPMatrix(G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH)
        out.cmd(0x0400103F, phys(vtx));                // gSPVertex(vtx, 4, 0)
        out.cmd(0xB1000204, 0x00000406);               // gSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0)
        out.cmd(0xBD000000, 0);                        // gSPPopMatrix(G_MTX_MODELVIEW)
        out.cmd(0x6400000D, 1);                        // gEXPopMatrixGroup(G_MTX_MODELVIEW)
        out.cmd(0x6400001A, 0);                        // gEXPopOtherMode
        out.cmd(0xBB000000, 0);                        // gSPTexture off
        out.cmd(0x6400002A, 0);                        // gEXPopGeometryMode
        out.cmd(0x6400001E, 0);                        // gEXPopProjectionMatrix
        out.cmd(0x64000016, 0);                        // gEXPopViewport
        out.cmd(0x64000018, 0);                        // gEXPopScissor
        out.cmd(kOpEndDl << 24, 0);                    // gSPEndDisplayList
        while (out.addr < viewport) {
            out.cmd(kOpEndDl << 24, 0);
        }

        // Full-screen viewport (320 x 240).
        out.h(640); out.h(480); out.h(511); out.h(0);
        out.h(640); out.h(480); out.h(511); out.h(0);

        const float p[4][4] = {
            { 1.0f / 128.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, -1.0f / 128.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f, 0.0f },
            { -1.0f, 1.0f, 0.0f, 1.0f },
        };
        write_mtx(out, p);
        const float m[4][4] = {
            { kScaleX, 0.0f, 0.0f, 0.0f },
            { 0.0f, kScaleY, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { ulx * kScaleX, uly * kScaleY, 0.0f, 1.0f },
        };
        write_mtx(out, m);

        const int16_t corners[4][4] = {
            { 0, 0, s0, t0 },
            { (int16_t)w, 0, s1, t0 },
            { (int16_t)w, (int16_t)h, s1, t1 },
            { 0, (int16_t)h, s0, t1 },
        };
        for (const auto& c : corners) {
            out.h((uint16_t)c[0]); out.h((uint16_t)c[1]); out.h(0); out.h(0);
            out.h((uint16_t)c[2]); out.h((uint16_t)c[3]);
            out.b(0xFF); out.b(0xFF); out.b(0xFF); out.b(0xFF);
        }
        if (!out.ok()) {
            return Result::Full;
        }

        // The rectangle becomes gSPDisplayList(start) and two no-ops.
        MEM_W(0, kseg0(rect)) = (int32_t)(kOpDl << 24);
        MEM_W(4, kseg0(rect)) = (int32_t)phys(start);
        for (int i = 1; i < 3; i++) {
            MEM_W(i * 8, kseg0(rect)) = (int32_t)(kOpNoOp << 24);
            MEM_W(i * 8 + 4, kseg0(rect)) = 0;
        }
        return Result::Converted;
    }
}

// From the frame clear (interpolation_ids.cpp): a new frame's arrows.
void btx_markers_new_frame() {
    for (size_t i = 0; i < kMaxCalls; i++) {
        prev_frame[i].swap(cur_frame[i]);
        cur_frame[i].clear();
    }
    call_index = 0;
}

// func_80100550's entry: where its commands will start.
extern "C" void btx_markers_begin(uint8_t* rdram) {
    call_start = (uint32_t)MEM_W(0, kseg0(kHudHead));
}

// func_80100550, after its last command (0x801009B8): convert its arrows.
extern "C" void btx_markers_end(uint8_t* rdram) {
    const uint32_t end = (uint32_t)MEM_W(0, kseg0(kHudHead));
    const size_t call = call_index++;
    if (end <= call_start || end - call_start > 0x4000 || ((end - call_start) & 7) != 0) {
        return;
    }

    slot = (slot + 1) % kSlotCount;
    Writer out{ rdram, kScratchStart + slot * kSlotSize, kScratchStart + (slot + 1) * kSlotSize };
    uint32_t prim = 0;
    for (uint32_t a = call_start; a + 24 <= end; a += 8) {
        const uint32_t op = (uint32_t)MEM_W(0, kseg0(a)) >> 24;
        if (op == kOpSetPrimColor) {
            prim = (uint32_t)MEM_W(4, kseg0(a)); // the arrow's colour
            continue;
        }
        if (op != kOpTexRect ||
            ((uint32_t)MEM_W(8, kseg0(a)) >> 24) != kOpRdpHalf1 ||
            ((uint32_t)MEM_W(16, kseg0(a)) >> 24) != kOpRdpHalf2) {
            continue;
        }
        Result r = convert(rdram, a, prim, out, call);
        if (r == Result::Full) {
            break;
        }
        if (r == Result::Converted) {
            a += 16;
        }
    }
}
