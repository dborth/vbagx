#ifndef JIT_H
#define JIT_H
#include "GBA.h"
#include "JITCache.h"
#include "JITDebug.h"

#ifndef NO_JIT_COMPILER
#define JIT_TRACE_MAX_INSTRUCTIONS 42

struct JITResult {
    u32 cycles;
    u32 nextPC;
    u32 instructions;
    u32 bailedOut; // 1 if guard failed, 0 if clean exit/yield
    u32 smcHit;      // Set to 1 if SMC guard triggered
	u32 smcAddress;  // Written EA (optional)
} __attribute__((aligned(32)));

extern JITCache jitCache;
struct CPUFlags;
BasicBlock* JITCompileThumbTrace(u32 startPC, JITCache& cache);
extern "C" void ExecuteJITTrace(JITBlockFunc execute, JITResult* outResult, u32* busPrefetchCount, u32* gbaRegs, CPUFlags* flags, GBAReadPageTable* readTable);
extern "C" void ExecuteJITTrace_Return();
#endif

#endif // JIT_H
