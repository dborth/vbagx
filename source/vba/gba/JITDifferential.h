#ifndef JIT_DIFFERENTIAL_H
#define JIT_DIFFERENTIAL_H

#ifdef JIT_DIFFERENTIAL_TESTING

struct BasicBlock; // Forward declaration

// The heavy lifting and state access are fully isolated in the .cpp file.
int JIT_RunDifferentialThumbHook_Impl(
    unsigned int pc,
    BasicBlock* block,
    unsigned short startOpcode,
    int* diffClockTicks
);

// The macro contains zero execution logic. It simply triggers the hook,
// passes the local clockTicks reference, and handles control flow (bailout / continue)
// based on the returned state.
#define JIT_DIFFERENTIAL_THUMB_HOOK(pc, block) \
    do { \
        int diffState = JIT_RunDifferentialThumbHook_Impl((pc), (block), CPUReadHalfWord(pc), &clockTicks); \
        if (diffState == -1) { PROFILER_ADD_TIME(timeSpentThumb, thumbTimeStart); return 0; } \
        if (diffState == 1) { PROFILER_ADD_TIME(timeSpentThumb, thumbTimeStart); return 1; } \
        if (diffState == 2) continue; \
    } while(0)

#else // JIT_DIFFERENTIAL_TESTING

// Completely zero-cost when disabled
#define JIT_DIFFERENTIAL_THUMB_HOOK(pc, block) do {} while(0)

#endif // JIT_DIFFERENTIAL_TESTING

#endif // JIT_DIFFERENTIAL_H
