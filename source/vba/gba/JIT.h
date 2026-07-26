/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * JIT.h
 *
 * ===========================================================================
 * PROJECT OVERVIEW
 * ===========================================================================
 * This is VBA-GX's THUMB trace JIT: a micro-JIT that recognizes short runs
 * of high-frequency GBA THUMB instructions, compiles them directly to
 * native Broadway (Wii) PowerPC, and falls back to the existing, correct
 * C++ interpreter for anything it doesn't recognize. It is not a full CPU
 * core rewrite, has no cross-block register allocation or optimizing IR,
 * and does not attempt ARM (32-bit) mode. Correctness of the fallback path
 * is non-negotiable: a block that can't be safely compiled bails to C++
 * silently and cheaply rather than guessing.
 *
 * A compiled block can exit several ways: falling off the end (default
 * epilogue), a taken conditional branch, a dynamic BX, a memory/SMC guard
 * failure (deferred bailout), or a scheduler quota yield — every exit
 * reports exact cycles-elapsed and a resume PC via JITResult, so JIT'd and
 * interpreted execution are interchangeable from the scheduler's point of
 * view. Where profitable, compiled blocks chain directly to one another
 * through a self-patching linker stub, so hot loops stay in native code
 * without round-tripping through the C++ dispatch loop every iteration.
 *
 * GBA registers and condition flags are allocated into host PowerPC
 * registers lazily and on demand (per block) rather than eagerly at every
 * call, so a block only pays for what it actually touches. Guest memory
 * access always follows the same guard template — bank check, page/mask
 * lookup, null-pointer guard, mask/align, access — and self-modifying code
 * is handled by tracking which compiled blocks' native code lives on which
 * guest memory page, patching any affected block to bail the moment a
 * guest write lands on top of it.
 *
 * This header is the entry point for the rest of the emulator; the actual
 * compiler, cache/arena, PowerPC emitter macros, ABI trampoline, and debug
 * plumbing live in their own files (JITCompiler.cpp, JITCache.*,
 * JITPPCEmitter.h, JITTrampoline.S, Debug.*) and are documented there.
 * ===========================================================================
 *
 * Top-level public interface for the THUMB trace JIT.
 *   - JITResult: the fixed-layout, 32-byte-aligned struct compiled traces
 *     write their output into (cycles elapsed, resume PC, instruction
 *     count, a bailedOut flag, and SMC-hit metadata/address) — this is the
 *     handshake between JIT-emitted code and the C++ dispatch loop.
 *   - JITCompileThumbTrace(): forward declaration of the compiler entry
 *     point implemented in JITCompiler.cpp.
 *   - ExecuteJITTrace() / ExecuteJITTrace_Return(): the C-callable
 *     declarations for the hand-written trampoline in JITTrampoline.S.
 *
 ***************************************************************************/

#ifndef JIT_H
#define JIT_H
#include "Debug.h"
#include "GBA.h"
#include "JITCache.h"

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
