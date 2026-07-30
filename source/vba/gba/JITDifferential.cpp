/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * JITDifferential.cpp
 *
 * Implements the differential mismatch-detection loop (only compiled when
 * JIT_DIFFERENTIAL_TESTING is defined). For a given block, this:
 *   1. Snapshots the full CPU state (all 16 registers, flags, armNextPC,
 *      cpuTotalTicks, prefetch buffer state) before anything runs.
 *   2. Runs the compiled JIT trace via ExecuteJITTrace() and records its
 *      reported cycles/nextPC/flags/registers.
 *   3. Restores the *pre-JIT* snapshot, then re-executes the same guest
 *      instructions through the plain C++ interpreter one at a time
 *      (thumbInsnTable[]) until it has burned the same number of cycles
 *      the JIT reported (or hits a scheduler/event boundary), as an
 *      independent "should have happened" reference run.
 *   4. Compares the two outcomes register-by-register, flag-by-flag, plus
 *      next PC and total cycles. Any mismatch is written to the shared
 *      debug log via LogJITMismatch(), with the specific field(s) that
 *      diverged called out explicitly (not just the first one) to help
 *      narrow down which format handler is at fault.
 *
 * The *first* reported mismatch in a run is the one to actually chase -
 * once state has diverged, every mismatch after that is just noise,
 * not an independent bug. Capped at MAX_JIT_MISMATCH_COUNT total reports
 * per run to avoid runaway log growth once something is actually broken.
 ***************************************************************************/

#include "Debug.h"
#include "JITDifferential.h"

#ifdef JIT_DIFFERENTIAL_TESTING

#include "JIT.h"
#include "GBA.h"
#include "GBAcpu.h"
#include "GBAinline.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define JIT_DIFFERENTIAL_MAX_CATCHUP 128
#define JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN 8

struct MemoryWriteEntry {
    u32 address;
    u32 value;
    u8 size; // 1 = byte, 2 = halfword, 4 = word
};

// Tracks individual instruction execution & side effects during catch-up
struct CatchupTrace {
    u32 pc;
    u16 opcode;
    int cycles;
    u8 writeCount;
    MemoryWriteEntry writes[JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN];
};

struct CPUStateBackup {
    u32 regs[16];
    CPUFlags flags;
    u32 armNextPC;
    u32 totalTicks;
    u32 prefetch[2];
    u32 busPrefetchCount;
};

// Hook state for intercepting C++ memory writes during catch-up
static bool g_diffTrackingActive = false;
static u8 g_currentInsnWriteCount = 0;
static MemoryWriteEntry g_currentInsnWrites[JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN];

void JIT_RecordMemoryWrite(unsigned int addr, unsigned int value, unsigned char size) {
	if (!g_diffTrackingActive) return;

	if (g_currentInsnWriteCount < JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN) {
		g_currentInsnWrites[g_currentInsnWriteCount].address = addr;
		g_currentInsnWrites[g_currentInsnWriteCount].value = value;
		g_currentInsnWrites[g_currentInsnWriteCount].size = size;
		g_currentInsnWriteCount++;
	}
}

static inline void JIT_SaveCPUState(CPUStateBackup* b) {
    for (int i = 0; i < 16; i++) b->regs[i] = reg[i].I;
    b->flags = gbaFlags;
    b->armNextPC = armNextPC;
    b->totalTicks = cpuTotalTicks;
    b->prefetch[0] = cpuPrefetch[0];
    b->prefetch[1] = cpuPrefetch[1];
    b->busPrefetchCount = busPrefetchCount;
}

static inline void JIT_RestoreCPUState(const CPUStateBackup* b) {
    for (int i = 0; i < 16; i++) reg[i].I = b->regs[i];
    gbaFlags = b->flags;
    armNextPC = b->armNextPC;
    cpuTotalTicks = b->totalTicks;
    cpuPrefetch[0] = b->prefetch[0];
    cpuPrefetch[1] = b->prefetch[1];
    busPrefetchCount = b->busPrefetchCount;
}

int JIT_RunDifferentialThumbHook_Impl(u32 pc, BasicBlock* block, u16 startOpcode, int* diffClockTicks, insnfunc_t* thumbInsnTable) {
	if (block == nullptr || block->execute == nullptr || debugStats.mismatchCount >= MAX_JIT_MISMATCH_COUNT) {
		return 0; // Did not handle, proceed to normal fallback
	}

	// 1. Save initial emulator state
	CPUStateBackup initial;
	JIT_SaveCPUState(&initial);

	// 2. Run JIT Trace
	JITResult jitResult = {0, 0, 0, 0};
	reg[15].I = pc + 4;
	ExecuteJITTrace(block->execute, &jitResult, &busPrefetchCount, &reg[0].I, &gbaFlags, &gbaReadTable);

	JIT_LOG_EXEC(jitResult.instructions, block->length, jitResult.bailedOut);

	// 3. Save JIT output & restore initial C++ state for catch-up
	CPUStateBackup jitState;
	JIT_SaveCPUState(&jitState);
	JIT_RestoreCPUState(&initial);

	// 4. Run Native C++ Catch-up execution
	int cppCycles = 0;
	u32 instructionCount = 0;
	CatchupTrace catchupChain[JIT_DIFFERENTIAL_MAX_CATCHUP];

	// Enable write tracking hook
	g_diffTrackingActive = true;

	// Execute EXACTLY the same number of instructions the JIT ran
	while (instructionCount < jitResult.instructions && instructionCount < JIT_DIFFERENTIAL_MAX_CATCHUP && !armState && !holdState && !SWITicks) {
		u16 opcode = cpuPrefetch[0];
		cpuPrefetch[0] = cpuPrefetch[1];
		busPrefetch = false;

		if (busPrefetchCount & 0xFFFFFF00)
			busPrefetchCount = 0x100 | (busPrefetchCount & 0xFF);

		u32 oldArmNextPC = armNextPC;
		armNextPC = reg[15].I;
		reg[15].I += 2;

		THUMB_PREFETCH_NEXT;

		*diffClockTicks = 0;

		// Reset per-instruction write hook buffer
		g_currentInsnWriteCount = 0;

		(*thumbInsnTable[opcode>>6])(opcode);

		if (*diffClockTicks < 0) break;
		if (*diffClockTicks == 0) *diffClockTicks = codeTicksAccessSeq16(oldArmNextPC) + 1;

		// Snapshot execution details & writes performed by this instruction
		catchupChain[instructionCount].pc = oldArmNextPC;
		catchupChain[instructionCount].opcode = opcode;
		catchupChain[instructionCount].cycles = *diffClockTicks;
		catchupChain[instructionCount].writeCount = g_currentInsnWriteCount;
		for (u8 w = 0; w < g_currentInsnWriteCount; w++) {
			catchupChain[instructionCount].writes[w] = g_currentInsnWrites[w];
		}

		cpuTotalTicks += *diffClockTicks;
		cppCycles += *diffClockTicks;

		instructionCount++;
	}

	g_diffTrackingActive = false;

	// 5. Compare & Detect Divergence
	bool armModeDuringCatchup = armState;
	bool mismatch = false;
	bool regMismatches[15] = { false };

	bool instMismatch = (jitResult.instructions != instructionCount);
	bool pcMismatch = (jitResult.nextPC != armNextPC);
	bool flagMismatch = (jitState.flags.N != gbaFlags.N || jitState.flags.Z != gbaFlags.Z ||
						 jitState.flags.C != gbaFlags.C || jitState.flags.V != gbaFlags.V);
	bool cycleMismatch = ((int)jitResult.cycles != cppCycles);
	bool prefetchMismatch = (jitState.busPrefetchCount != busPrefetchCount);

	for (int i = 0; i < 15; i++) {
		if (jitState.regs[i] != reg[i].I) {
			regMismatches[i] = true;
			mismatch = true;
		}
	}

	// The JIT bails before applying PC/Cycle costs on a mode switch
	// Forgive PC/Cycle divergences if the C++ interpreter hit the ARM
	// switch on the exact final instruction of the JIT trace
	if (armModeDuringCatchup && (instructionCount == jitResult.instructions)) {
		pcMismatch = false;
		cycleMismatch = false;
	}

	if (pcMismatch || flagMismatch || cycleMismatch || instMismatch || prefetchMismatch) mismatch = true;

	// 6. Log Detailed Mismatch State
	if (mismatch) {
		static const char* regNames[15] = {
			"R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
			"R8", "R9", "R10", "R11", "R12", "SP", "LR"
		};

		static char assembledMsg[8192];
		char *ptr = assembledMsg;
		size_t remaining = sizeof(assembledMsg);
		assembledMsg[0] = '\0';

		auto appendToMsg = [&](const char* format, ...) {
			va_list args;
			va_start(args, format);
			int written = vsnprintf(ptr, remaining, format, args);
			va_end(args);

			if (written > 0) {
				if ((size_t)written >= remaining) {
					ptr += remaining - 1;
					remaining = 1;
				} else {
					ptr += written;
					remaining -= (size_t)written;
				}
			}
		};

		appendToMsg("StartPC: 0x%08X | Trace Length: %u | Opcode: 0x%04X\n", pc, jitResult.instructions, startOpcode);
		appendToMsg("Initial Flags: N=%u Z=%u C=%u V=%u\n", initial.flags.N, initial.flags.Z, initial.flags.C, initial.flags.V);
		appendToMsg("JIT Result:  NextPC=0x%08X | Cycles=%u | Flags=(N:%u Z:%u C:%u V:%u) | Insns=%u\n",
			jitResult.nextPC, jitResult.cycles, jitState.flags.N, jitState.flags.Z, jitState.flags.C, jitState.flags.V, jitResult.instructions);
		appendToMsg("C++ Result:  NextPC=0x%08X | Cycles=%d | Flags=(N:%u Z:%u C:%u V:%u) | Insns=%u\n",
			armNextPC, cppCycles, gbaFlags.N, gbaFlags.Z, gbaFlags.C, gbaFlags.V, instructionCount);

		appendToMsg("--- MISMATCH DETAILS ---\n");
		if (instMismatch) {
			appendToMsg("  [INSTRUCTIONS] JIT claimed %u vs C++ ran %u\n", jitResult.instructions, instructionCount);
		}
		if (prefetchMismatch) {
			appendToMsg("  [PREFETCH] JIT=0x%08X vs C++=0x%08X\n", jitState.busPrefetchCount, busPrefetchCount);
		}
		for (int i = 0; i < 15; i++) {
			if (regMismatches[i]) {
				appendToMsg("  [REG %-3s] Initial=0x%08X | JIT=0x%08X vs C++=0x%08X\n",
					regNames[i], initial.regs[i], jitState.regs[i], reg[i].I);
			}
		}
		if (pcMismatch) {
			appendToMsg("  [NEXT PC] JIT=0x%08X vs C++=0x%08X\n", jitResult.nextPC, armNextPC);
		}
		if (flagMismatch) {
			appendToMsg("  [FLAGS]   JIT=(N:%u Z:%u C:%u V:%u) vs C++=(N:%u Z:%u C:%u V:%u)\n",
				jitState.flags.N, jitState.flags.Z, jitState.flags.C, jitState.flags.V,
				gbaFlags.N, gbaFlags.Z, gbaFlags.C, gbaFlags.V);
		}
		if (cycleMismatch) {
			appendToMsg("  [CYCLES]  JIT=%u vs C++=%d\n", jitResult.cycles, cppCycles);
		}
		if (armModeDuringCatchup) {
			appendToMsg("  [NOTE] Interpreter entered ARM mode during catch-up...\n");
		}

		// Append the actual sequence of executed instructions and their memory writes
		appendToMsg("\n--- C++ EXECUTION TRACE (Ran %u insns) ---\n", instructionCount);
		for (u32 i = 0; i < instructionCount; i++) {
			const CatchupTrace& tr = catchupChain[i];
			appendToMsg("  [%02u] PC: 0x%08X | Opcode: 0x%04X | Cycles: %d | Writes: %u\n",
				i, tr.pc, tr.opcode, tr.cycles, tr.writeCount);

			for (u8 w = 0; w < tr.writeCount; w++) {
				const MemoryWriteEntry& write = tr.writes[w];
				appendToMsg("       -> [WRITE %2ubit] Addr: 0x%08X | Val: 0x%08X\n",
					write.size * 8, write.address, write.value);
			}
		}

		JIT_LOG_MISMATCH(assembledMsg);
	}

	if (*diffClockTicks < 0) return -1;
	if (cpuTotalTicks >= cpuNextEvent || armState || holdState || SWITicks) return 1;
	return 2; // Success/Handled, continue loop
}

#endif // JIT_DIFFERENTIAL_TESTING
