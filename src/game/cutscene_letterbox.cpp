// Black at the sides of the 4:3 frame in widescreen, where the game would
// otherwise leave something else there.
//
// Letterboxed 3D cutscenes, as in the sequel's port (btgarecomp STATUS.md
// rounds 101 and 105). A 3D cutscene draws through the camera's cutscene
// viewport, entry 0x1E0 of the static viewport block (segment 1): a smaller
// view with the scissor at (34,19)-(286,134); the rest of the 4:3 frame stays
// black. But the frame clear (func_8007DC5C) fills the colour buffer with the
// scene's clear colour across the whole screen, and RT64 stretches a fill
// that spans the screen to the window's edges in Expand, so that colour
// showed left and right of the 4:3 frame. Here a black fill of the whole
// screen (stretched to the window edges, like the clear) is appended, then
// the clear colour again over just the cutscene's box (not full width, so it
// stays in the 4:3 frame). Cameras: 0x348 bytes each from 0x802DE620, count
// at 0x801B4AB4, viewport list at +0xF4 (as func_800A5EB0 reads them).
//
// Frames without a colour clear. The logos, menus and 2D story screens skip
// the colour fill (their pictures cover the 4:3 frame), so nothing ever
// drew the sides: black after boot, but after quitting a game to the menus
// the last 3D frames stayed there. Those frames get the black fill too. The
// game redraws its whole 4:3 frame every time (it rotates between
// framebuffers), so nothing it draws is lost.
#include <cstdint>

#include "recomp.h"

namespace {
    constexpr uint32_t kCameras = 0x802DE620;
    constexpr uint32_t kCameraSize = 0x348;
    constexpr uint32_t kCameraCount = 0x801B4AB4;
    constexpr uint32_t kCutsceneViewport = 0x010001E0;

    constexpr uint32_t kZClearColor = 0xFFFCFFFC; // the depth clear's fill colour
    constexpr uint32_t kBlack = 0x00010001;       // two RGBA5551 pixels, opaque black

    gpr kseg0(uint32_t address) {
        return (gpr)(int32_t)address;
    }

    bool cutscene_view(uint8_t* rdram) {
        const uint32_t count = (uint32_t)MEM_W(0, kseg0(kCameraCount));
        for (uint32_t i = 0; i < count && i < 4; i++) {
            if ((uint32_t)MEM_W(0xF4, kseg0(kCameras + i * kCameraSize)) == kCutsceneViewport) {
                return true;
            }
        }
        return false;
    }
}

// Called with the frame clear's display-list head, at its end; returns the
// new head.
uint32_t btx_cutscene_letterbox(uint8_t* rdram, uint32_t clear_start, uint32_t head) {
    // The clear colour: the last fill colour the clear set that isn't the
    // depth clear's, if a fill rect follows it (the colour clear is
    // conditional). And the clear's last scissor, to restore it.
    uint32_t clear_color = 0;
    bool cleared = false;
    bool scissor_set = false;
    uint32_t scissor_w0 = 0;
    uint32_t scissor_w1 = 0;
    for (uint32_t a = clear_start; a + 8 <= head; a += 8) {
        const uint32_t w0 = (uint32_t)MEM_W(0, kseg0(a));
        const uint32_t w1 = (uint32_t)MEM_W(4, kseg0(a));
        if ((w0 >> 24) == 0xED) {
            scissor_set = true;
            scissor_w0 = w0;
            scissor_w1 = w1;
        }
        else if ((w0 >> 24) == 0xF7) {
            if (w1 != kZClearColor) {
                clear_color = w1;
                cleared = false;
            }
        }
        else if ((w0 >> 24) == 0xF6 && clear_color != 0) {
            cleared = true;
        }
    }

    const bool letterbox = cutscene_view(rdram);
    if (!letterbox && cleared) {
        return head;
    }

    struct Cmd { uint32_t w0, w1; };
    Cmd cmds[10];
    int n = 0;
    cmds[n++] = { 0xBA001402, 0x00300000 }; // G_SETCYCLETYPE fill
    cmds[n++] = { 0xED000000, 0x005003C0 }; // scissor: the whole screen
    cmds[n++] = { 0xF7000000, kBlack };
    cmds[n++] = { 0xF64FC3BC, 0x00000000 }; // fill (0,0)-(319,239)
    if (letterbox && cleared) {
        cmds[n++] = { 0xF7000000, clear_color };
        cmds[n++] = { 0xF6474214, 0x0008804C }; // fill (34,19)-(285,133)
    }
    cmds[n++] = { 0xBA001402, 0x00000000 }; // back to 1-cycle, as the clear left it
    if (!letterbox && scissor_set) {
        cmds[n++] = { scissor_w0, scissor_w1 };
    }
    for (int i = 0; i < n; i++) {
        MEM_W(0, kseg0(head)) = (int32_t)cmds[i].w0;
        MEM_W(4, kseg0(head)) = (int32_t)cmds[i].w1;
        head += 8;
    }
    return head;
}
