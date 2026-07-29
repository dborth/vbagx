/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * JITDifferential.h
 *
 * Public interface for the differential mismatch-detection loop.
 * Deliberately kept tiny and logic-free: the actual state
 * save/restore/compare work lives entirely in JITDifferential.cpp,
 * and this header only exposes JIT_RunDifferentialThumbHook_Impl() plus the
 * JIT_DIFFERENTIAL_THUMB_HOOK(pc, block) macro that the interpreter's THUMB
 * dispatch loop calls at each potential JIT entry point.
 *
 * The macro's only job is control flow: it calls the impl function, then
 * interprets its return code to decide whether the calling dispatch loop
 * should return 0 (fatal/negative-tick abort), return 1 (event-boundary
 * exit), or continue (handled, keep looping) - all timer bookkeeping
 * (PROFILER_ADD_TIME) happens right at the call site since that macro
 * already has the relevant locals in scope. When JIT_DIFFERENTIAL_TESTING
 * isn't defined, the macro compiles to a no-op and this entire subsystem
 * costs nothing.
 ***************************************************************************/

#ifndef JIT_DIFFERENTIAL_H
#define JIT_DIFFERENTIAL_H

#include "Debug.h"

#ifdef JIT_DIFFERENTIAL_TESTING

struct BasicBlock; // Forward declaration
typedef void (*insnfunc_t)(unsigned int);

// The heavy lifting and state access are fully isolated in the .cpp file.
int JIT_RunDifferentialThumbHook_Impl(
	unsigned int pc,
	BasicBlock* block,
	unsigned short startOpcode,
	int* diffClockTicks,
	insnfunc_t* thumbInsnTable
);

// The macro contains zero execution logic. It simply triggers the hook,
// passes the local clockTicks reference, and handles control flow (bailout / continue)
// based on the returned state.
#define JIT_DIFFERENTIAL_THUMB_HOOK(pc, block) \
	do { \
		int diffState = JIT_RunDifferentialThumbHook_Impl((pc), (block), CPUReadHalfWord(pc), &clockTicks, thumbInsnTable); \
		if (diffState == -1) { PROFILER_ADD_TIME(timeSpentThumb, thumbTimeStart); return 0; } \
		if (diffState == 1) { PROFILER_ADD_TIME(timeSpentThumb, thumbTimeStart); return 1; } \
		if (diffState == 2) continue; \
	} while(0)
#endif

#ifndef JIT_DIFFERENTIAL_THUMB_HOOK
#define JIT_DIFFERENTIAL_THUMB_HOOK(pc, block) 										((void)0)
#endif

#endif // JIT_DIFFERENTIAL_H
