// Lets other game threads run from inside a loop that waits on one of them.
//
// On the N64 a hardware interrupt preempts a thread spinning on a flag, so
// the thread that will clear it gets to run. The runtime's scheduler is
// cooperative: it switches threads only at OS calls, and only to a strictly
// higher-priority thread (osYieldThread, which would round-robin, isn't
// implemented). A polling loop with no OS call in it therefore never lets
// anything else run. The [[patches.hook]] entries in battletanx.us.rev0.toml
// call this from inside each such loop.
//
// Two steps, both needed (worked out on the sequel's port, btgarecomp
// STATUS.md rounds 47-53 and 65):
//  1. Deliver pending hardware events. VI/AI/SP/... events from the runtime's
//     host threads go into a separate external-message queue and reach a game
//     thread blocked in osRecvMesg only when some game thread drains it. Drain
//     all of it: the queue is shared by every event source, and popping one
//     message per call let a VI event starve behind the others.
//  2. Drop this thread's priority to 0 and restore it, which hands the CPU to
//     any ready thread, including ones of equal or lower priority.
//
// Waiting without burning a core (round 23, as in the sequel's port, its
// round 188): those steps return at once, so a loop waiting for the next
// frame called this back to back and its thread used ~95% of a core all the
// time, title screen included (top -H: "Game 2", in func_8007D850's and
// func_8007E248's loops), which costs battery on the Steam Deck. So once a
// thread has been calling back to back (each call within 2 ms of the last)
// for 2 ms, each call first sleeps until the next external message arrives
// (at most 1 ms) and delivers it, then does both steps as before. Not from
// the second call: func_80079064's copy loop calls this once per word it
// copies, and sleeping there would take a millisecond a word. A wait still
// reacts at once for its first 2 ms.

#include <chrono>
#include <cstdint>

#include "ultramodern/ultramodern.hpp"

extern "C" int32_t osGetThreadPri(uint8_t* rdram, int32_t t);
extern "C" void osSetThreadPri(uint8_t* rdram, int32_t t, int32_t pri);

// ultramodern/src/mesgqueue.cpp; external C++ linkage but in no header.
void dequeue_external_messages(uint8_t* rdram);

extern "C" void btx_yield_via_priority_drop(uint8_t* rdram) {
    using clock = std::chrono::steady_clock;
    constexpr auto kBackToBack = std::chrono::milliseconds(2);
    thread_local clock::time_point last_call{};
    thread_local clock::time_point streak_start{};
    const clock::time_point now = clock::now();
    if (now - last_call >= kBackToBack) {
        streak_start = now;
    }
    if (now - streak_start >= kBackToBack) {
        ultramodern::wait_for_external_message_timed(rdram, 1);
    }
    last_call = clock::now();

    dequeue_external_messages(rdram);

    int32_t saved_pri = osGetThreadPri(rdram, 0);
    osSetThreadPri(rdram, 0, 0);
    osSetThreadPri(rdram, 0, saved_pri);
}
