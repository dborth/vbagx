#ifndef JIT_H
#define JIT_H
#include "JITCache.h"
#include "JITDebug.h"

#ifndef NO_JIT_COMPILER
struct JITResult {
    u32 cycles;
    u32 nextPC;
    u32 instructions;
    u32 bailedOut; // 1 if guard failed, 0 if clean exit/yield
} __attribute__((aligned(32)));

extern JITCache jitCache;
struct CPUFlags;
BasicBlock* JITCompileThumbTrace(u32 startPC, JITCache& cache);
extern "C" void ExecuteJITTrace(JITBlockFunc execute, JITResult* outResult, u32* busPrefetchCount, u32* gbaRegs, CPUFlags* flags, u8** readPages, u32* readMasks);
extern "C" void ExecuteJITTrace_Return();
#endif

#endif // JIT_H
