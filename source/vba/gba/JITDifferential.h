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

struct BasicBlock; // Forward declaration
typedef void (*insnfunc_t)(unsigned int);

// The heavy lifting and state access are fully isolated in the .cpp file.
int JIT_RunDifferentialThumbHook_Impl(
	unsigned int pc,
	BasicBlock* block,
	unsigned short startOpcode,
	int* diffClockTicks,
	insnfunc_t* thumbInsnTable,
	bool* useJIT
);

void JIT_RecordMemoryWrite(unsigned int addr, unsigned int value, unsigned char size);

#endif // JIT_DIFFERENTIAL_H
