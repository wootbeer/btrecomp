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

#include <cstdint>

#include "ultramodern/ultramodern.hpp"

extern "C" int32_t osGetThreadPri(uint8_t* rdram, int32_t t);
extern "C" void osSetThreadPri(uint8_t* rdram, int32_t t, int32_t pri);

// ultramodern/src/mesgqueue.cpp; external C++ linkage but in no header.
void dequeue_external_messages(uint8_t* rdram);

extern "C" void btx_yield_via_priority_drop(uint8_t* rdram) {
    dequeue_external_messages(rdram);

    int32_t saved_pri = osGetThreadPri(rdram, 0);
    osSetThreadPri(rdram, 0, 0);
    osSetThreadPri(rdram, 0, saved_pri);
}
