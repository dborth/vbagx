#ifndef JIT_H
#define JIT_H
#include "JITCache.h"
#include "JITDebug.h"

struct JITResult {
    u32 cycles;
    u32 nextPC;
} __attribute__((aligned(32)));

extern JITCache jitCache;
BasicBlock* JITCompileThumbTrace(u32 startPC, JITCache& cache);
extern "C" void ExecuteJITTrace(JITBlockFunc execute, JITResult* outResult, u32* busPrefetchCount, volatile u32* gbaRegs, u32* flags, u8** readPages, u32* readMasks, u8** writePages);
extern "C" void ExecuteJITTrace_Return();

#endif // JIT_H
