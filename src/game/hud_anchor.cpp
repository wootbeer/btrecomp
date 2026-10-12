// Widescreen HUD: in Expand, the gameplay HUD's left and right parts move out
// to the window edges, as in the sequel's port (btgarecomp rounds 99-104).
//
// RT64 keeps 2D rectangles inside the centred 4:3 area unless the display
// list anchors them to a screen edge (extended GBI gEXSetRectAlign; the
// Graphics tab's HUD Ratio then sets how far out an anchored edge goes).
//
// The sequel draws its HUD with a script interpreter, so its port hooks each
// element. This game's HUD is drawn by fixed code instead: func_800F5B6C
// calls one of four functions by player count (func_800F6934 for one player,
// func_800F8120 / func_800F9F34 / func_800FC674 for 2-4), which draw into a
// display list of their own: three rotating 0x8000-byte buffers from
// 0x80362120 (index at 0x801416F4), head at 0x8037A174. When the HUD
// function returns (0x800F6664), the frame's HUD is all in that buffer, so
// it's anchored there in one pass:
//   - every rectangle (texture and fill) is grouped with the ones right next
//     to it on the same row, so a string or a number with its icon moves as
//     one piece and is never split;
//   - a group entirely in the left part of the screen is anchored left, one
//     entirely in the right part right, anything over the middle (the health
//     bar, centred messages) stays centred;
//   - anchored groups also move 16 pixels further out: the original layout
//     leaves a TV-safe margin.
// Then the buffer is rewritten with the anchoring commands in front of the
// rectangles that need them. Only with one or two players (RT64 widens only
// full-width views, and two players split the screen top and bottom) and
// only when the result fits the buffer.
#include <algorithm>
#include <cstdint>
#include <vector>

#include "recomp.h"

#include "btx_config.h"

namespace {
    constexpr uint32_t kHudBufferBase = 0x80362120;
    constexpr uint32_t kHudBufferSize = 0x8000;
    constexpr uint32_t kHudBufferIndex = 0x801416F4;
    constexpr uint32_t kHudHead = 0x8037A174;
    constexpr uint32_t kHudPlayerCount = 0x8037A140;

    // gEXEnable for this game's microcode, F3DEX 1.21: RT64's hook is
    // G_SPNOOP, which is opcode 0x00 here (0xE0 is F3DEX2's, as the sequel
    // uses). With the wrong one RT64 never enables its extended commands
    // and runs their parameter words as ordinary commands.
    struct ExEnable { uint32_t w0, w1; };
    constexpr ExEnable kExEnable{ 0x00525464, 0x10000064 };

    constexpr uint16_t kOriginLeft = 0x0;    // G_EX_ORIGIN_LEFT
    constexpr uint16_t kOriginRight = 0x400; // G_EX_ORIGIN_RIGHT
    constexpr uint16_t kOriginNone = 0x800;  // G_EX_ORIGIN_NONE

    constexpr int kHudMarginShift = 16 * 4; // 10.2 fixed point
    // RT64 adds the framebuffer width to right-anchored coordinates, so they
    // are given relative to the right edge.
    constexpr int kRightOriginShift = -320 * 4;

    // Screen x (in pixels) a group must stay left of / right of to be
    // anchored.
    constexpr int kLeftLimit = 140;
    constexpr int kRightLimit = 180;

    enum class Side { None, Left, Right };

    gpr kseg0(uint32_t address) {
        return (gpr)(int32_t)address;
    }

    struct Cmd {
        uint32_t w0, w1;
    };

    struct Rect {
        size_t index;      // command index in the buffer
        int ulx, uly, lrx, lry;
        size_t group;
    };

    struct Group {
        int ulx, uly, lrx, lry;
        Side side = Side::None;
    };

    bool is_rect(uint32_t w0) {
        uint8_t op = w0 >> 24;
        return op == 0xE4 || op == 0xE5 || op == 0xF6; // G_TEXRECT, G_TEXRECTFLIP, G_FILLRECT
    }

    // Rect corners in pixels. Texture rects: w0 = op | lrx << 12 | lry,
    // w1 = tile << 24 | ulx << 12 | uly (10.2 fixed point). Fill rects the
    // same, inclusive in fill mode, which doesn't matter here.
    Rect parse_rect(size_t index, const Cmd& c) {
        Rect r;
        r.index = index;
        r.lrx = ((c.w0 >> 12) & 0xFFF) / 4;
        r.lry = (c.w0 & 0xFFF) / 4;
        r.ulx = ((c.w1 >> 12) & 0xFFF) / 4;
        r.uly = (c.w1 & 0xFFF) / 4;
        r.group = 0;
        return r;
    }

    void append_align(std::vector<Cmd>& out, Side side, bool& scissor_pushed) {
        uint16_t left = kOriginNone, right = kOriginNone;
        int16_t left_offset = 0, right_offset = 0;
        if (side == Side::Left) {
            left = right = kOriginLeft;
            left_offset = right_offset = -kHudMarginShift;
        }
        else if (side == Side::Right) {
            left = right = kOriginRight;
            left_offset = right_offset = kRightOriginShift + kHudMarginShift;
        }
        // RT64 clips rects to the scissor, which unanchored is the centred
        // 4:3 area: while anything is anchored, widen it to the whole window.
        if (side != Side::None && !scissor_pushed) {
            out.push_back({ 0x64000017, 0 }); // gEXPushScissor
            // gEXSetScissor(G_SC_NON_INTERLACE, LEFT, RIGHT, 0, 0, 0, 240)
            out.push_back({ 0x64000005, (uint32_t)((kOriginLeft << 2) | (kOriginRight << 14)) });
            out.push_back({ 0, 240u * 4 });
            scissor_pushed = true;
        }
        out.push_back({ 0x64000006, (uint32_t)((left & 0xFFF) | ((right & 0xFFF) << 12)) }); // gEXSetRectAlign
        out.push_back({ (uint32_t)(uint16_t)left_offset << 16, (uint32_t)(uint16_t)right_offset << 16 });
        if (side == Side::None && scissor_pushed) {
            out.push_back({ 0x64000018, 0 }); // gEXPopScissor
            scissor_pushed = false;
        }
    }
}

// func_800F5B6C at 0x800F6664: the HUD function has returned and the end of
// the list hasn't been written yet.
extern "C" void btx_hud_anchor(uint8_t* rdram, recomp_context*) {
    if (btx::get_widescreen_scale() <= 1.0f) {
        return;
    }
    const uint32_t players = (uint32_t)MEM_W(0, kseg0(kHudPlayerCount));
    if (players < 1 || players > 2) {
        return;
    }
    const uint32_t index = (uint32_t)MEM_W(0, kseg0(kHudBufferIndex));
    if (index > 2) {
        return;
    }
    const uint32_t start = kHudBufferBase + (index << 15);
    const uint32_t head = (uint32_t)MEM_W(0, kseg0(kHudHead));
    if (head <= start || head > start + kHudBufferSize || ((head - start) & 7) != 0) {
        return;
    }

    std::vector<Cmd> cmds;
    cmds.reserve((head - start) / 8);
    for (uint32_t a = start; a < head; a += 8) {
        cmds.push_back({ (uint32_t)MEM_W(0, kseg0(a)), (uint32_t)MEM_W(4, kseg0(a)) });
    }

    // Rects, grouped with their neighbours on the same row.
    std::vector<Rect> rects;
    std::vector<Group> groups;
    for (size_t i = 0; i < cmds.size(); i++) {
        if (!is_rect(cmds[i].w0)) {
            continue;
        }
        Rect r = parse_rect(i, cmds[i]);
        bool joined = false;
        if (!groups.empty()) {
            Group& g = groups.back();
            const bool same_row = r.uly <= g.lry && r.lry >= g.uly;
            const bool adjacent = r.ulx <= g.lrx + 12 && r.lrx >= g.ulx - 12;
            if (same_row && adjacent) {
                g.ulx = std::min(g.ulx, r.ulx);
                g.uly = std::min(g.uly, r.uly);
                g.lrx = std::max(g.lrx, r.lrx);
                g.lry = std::max(g.lry, r.lry);
                joined = true;
            }
        }
        if (!joined) {
            groups.push_back({ r.ulx, r.uly, r.lrx, r.lry });
        }
        r.group = groups.size() - 1;
        rects.push_back(r);
    }
    bool any = false;
    for (Group& g : groups) {
        if (g.lrx - g.ulx >= 300) {
            g.side = Side::None; // full-width: fades, flashes
        }
        else if (g.lrx <= kLeftLimit) {
            g.side = Side::Left;
        }
        else if (g.ulx >= kRightLimit) {
            g.side = Side::Right;
        }
        any |= g.side != Side::None;
    }
    if (!any) {
        return;
    }

    // Rewrite with the anchoring in front of each rect whose side changes.
    std::vector<Cmd> out;
    out.reserve(cmds.size() + rects.size() * 6 + 8);
    out.push_back({ kExEnable.w0, kExEnable.w1 });
    Side current = Side::None;
    bool scissor_pushed = false;
    size_t next_rect = 0;
    for (size_t i = 0; i < cmds.size(); i++) {
        if (next_rect < rects.size() && rects[next_rect].index == i) {
            Side side = groups[rects[next_rect].group].side;
            if (side != current) {
                append_align(out, side, scissor_pushed);
                current = side;
            }
            next_rect++;
        }
        out.push_back(cmds[i]);
    }
    if (current != Side::None) {
        append_align(out, Side::None, scissor_pushed);
    }
    // Room for the end-of-list command the game writes next.
    if (out.size() * 8 + 0x100 > kHudBufferSize) {
        return;
    }
    uint32_t a = start;
    for (const Cmd& c : out) {
        MEM_W(0, kseg0(a)) = (int32_t)c.w0;
        MEM_W(4, kseg0(a)) = (int32_t)c.w1;
        a += 8;
    }
    MEM_W(0, kseg0(kHudHead)) = (int32_t)a;
}
