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
	insnfunc_t* thumbInsnTable
);

void JIT_RecordMemoryWrite(unsigned int addr, unsigned int value, unsigned char size);

#define JIT_R5_TRACE_MAX 32

#define JIT_R5_TAG_PUSHPOP_BEFORE      1
#define JIT_R5_TAG_PUSHPOP_AFTER       2
#define JIT_R5_TAG_BRANCH_BEFORE       3
#define JIT_R5_TAG_BRANCH_AFTER        4
#define JIT_R5_TAG_TRACE_ENTRY         5   // NEW: R5 as seeded by the trampoline, before any compiled instruction runs
#define JIT_R5_TAG_SINGLEACCESS_ENTRY  6   // NEW: before EmitSingleAccessRecharge (Format 9/10/11)
#define JIT_R5_TAG_SINGLEACCESS_MID    7   // NEW: after EmitSingleAccessRecharge, before EmitDynamicNCyclePenalty
#define JIT_R5_TAG_SINGLEACCESS_AFTER  8   // NEW: after EmitDynamicNCyclePenalty

extern unsigned int g_jitR5Trace[JIT_R5_TRACE_MAX];
extern unsigned int g_jitR5TraceTags[JIT_R5_TRACE_MAX];
extern unsigned int g_jitR5TraceIndex;
extern unsigned int g_jitR5DumpSpill[4]; // R9,R10,R11,R12 save slots for EmitR5TraceDump

void JIT_ResetR5Trace();

#endif // JIT_DIFFERENTIAL_H
