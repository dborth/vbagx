#ifndef NO_JIT_COMPILER
#include "JIT.h"
#include "GBAinline.h"
#include "GBAcpu.h"

#define MAX_INSTRUCTIONS 32
#define MAX_WORDS 2048
#define YIELD_NUMBER 256
#define MAX_BAILOUTS 256
#define MAX_BAILOUT_STUB_WORDS 26   // 1 (comp) + 5 (lazy flags) + 15 (registers) + 5 (return sequence)
#define EPILOGUE_RESERVE_WORDS 64   // Heavy prefetch sync + full lazy register/flag flushes + quota guard stubs

// STATIC TIMING MACROS
// Prevents the JIT compiler from mutating the busPrefetchCount state during trace compilation.
#define STATIC_CODE_TICKS_SEQ16(addr) memoryWaitSeq[((addr) >> 24) & 15]
#define STATIC_CODE_TICKS_16(addr)    memoryWait[((addr) >> 24) & 15]
#define STATIC_DATA_TICKS_32(addr)    memoryWait32[((addr) >> 24) & 15]
#define STATIC_DATA_TICKS_16(addr)    memoryWait[((addr) >> 24) & 15]

// --- LAZY FLAG TRACKER STATE ---
#define FLAG_N 0
#define FLAG_Z 1
#define FLAG_C 2
#define FLAG_V 3

struct FlagState {
	bool allocated;
	bool dirty;
	u8 hostReg;
} flagCache[4];

// --- LAZY REGISTER TRACKER STATE ---
struct RegisterState {
	bool allocated;
	bool dirty;
	u8 hostReg;
	u32 age; // Monotonic counter for LRU eviction
} regCache[15];

// --- DEFERRED BAILOUT ARCHITECTURE ---
enum BailoutCond { COND_BEQ, COND_BNE, COND_BGE, COND_BLT, COND_BLE };
enum CompType { COMP_NONE, COMP_REVERT_SP };

struct DeferredBailout {
	u32* branchPtr;
	BailoutCond cond;
	u32 pc;
	u32 cycles;
	u32 instructions;
	RegisterState registerSnapshot[15];
	FlagState flagSnapshot[4];
	CompType comp;
	u32 compArg;
};

static inline void FlushJITCache(void* addr, u32 size) {
    u32 start = (u32)addr & ~31;
    u32 end   = ((u32)addr + size + 31) & ~31;
    for (u32 i = start; i < end; i += 32) asm volatile("dcbst 0, %0" : : "r" (i) : "memory");
    asm volatile("sync" : : : "memory");
    for (u32 i = start; i < end; i += 32) asm volatile("icbi 0, %0" : : "r" (i) : "memory");
    asm volatile("sync \n isync" : : : "memory");
}

BasicBlock* JITCompileThumbTrace(u32 startPC, JITCache& cache) {
	if (!JIT_REGION_ALLOWED(startPC)) {
		return cache.registerBlock(startPC, 0, nullptr);
	}

	DeferredBailout bailouts[MAX_BAILOUTS];
	u32 bailoutCount = 0;

	u32 arenaOffsetStart = 0;
	u32* emitPtr = nullptr;
	u32* blockStart = nullptr;
	u32* quotaGuard = nullptr;
	bool arenaAllocated = false;

	u32 currentPC = startPC;
	u32 instrCount = 0;
	u32 chunkStartPC = startPC;
	u32 chunkInstrCount = 0;
	u32 chunkStaticCycles = 0;
	bool endBlock = false;

	auto RegisterBailout = [&](u32* bPtr, BailoutCond cond, u32 bPC, u32 bCycles, CompType comp = COMP_NONE, u32 compArg = 0) {
		if (bailoutCount >= MAX_BAILOUTS) { // Buffer overflow safety
			// Patch the uninitialized hole with a trap to prevent the CPU from executing random memory
			*bPtr = 0x7FE00008; // PPC 'trap' instruction
			endBlock = true;
			return;
		}
		bailouts[bailoutCount].branchPtr = bPtr;
		bailouts[bailoutCount].cond = cond;
		bailouts[bailoutCount].pc = bPC;
		bailouts[bailoutCount].cycles = bCycles;
		bailouts[bailoutCount].instructions = instrCount;
		bailouts[bailoutCount].comp = comp;
		bailouts[bailoutCount].compArg = compArg;

		for (int i = 0; i < 4; i++) bailouts[bailoutCount].flagSnapshot[i] = flagCache[i];
		// Snapshot the register state BEFORE any subsequent execution modifies it
		for (int i = 0; i < 15; i++) bailouts[bailoutCount].registerSnapshot[i] = regCache[i];
		bailoutCount++;
	};

	// Initialize flag cache natively mapped to their dedicated GPRs
	flagCache[FLAG_N] = {false, false, PPC_REG_N};
	flagCache[FLAG_Z] = {false, false, PPC_REG_Z};
	flagCache[FLAG_C] = {false, false, PPC_REG_C};
	flagCache[FLAG_V] = {false, false, PPC_REG_V};

	auto ReadFlag = [&](u8 flagIdx, u32*& ptr) -> u8 {
		if (flagCache[flagIdx].allocated) return flagCache[flagIdx].hostReg;

		// Lazy load: Stack frame offset 84(r1) securely holds the flags array pointer
		*ptr++ = PPC_LWZ(PPC_R12, 1, 84);
		*ptr++ = PPC_LWZ(flagCache[flagIdx].hostReg, PPC_R12, flagIdx * 4);
		flagCache[flagIdx].allocated = true;

		return flagCache[flagIdx].hostReg;
	};

	auto WriteFlag = [&](u8 flagIdx) -> u8 {
		flagCache[flagIdx].allocated = true;
		flagCache[flagIdx].dirty = true;
		return flagCache[flagIdx].hostReg;
	};

	auto FlushDirtyFlags = [&](u32*& ptr) {
		bool ptrLoaded = false;
		for (int i = 0; i < 4; i++) {
			if (flagCache[i].allocated && flagCache[i].dirty) {
				if (!ptrLoaded) {
					*ptr++ = PPC_LWZ(PPC_R12, 1, 84);
					ptrLoaded = true;
				}
				*ptr++ = PPC_STW(flagCache[i].hostReg, PPC_R12, i * 4);
				flagCache[i].dirty = false;
			}
		}
	};

	auto EmitDirtyFlagFlush = [&](u32*& ptr) {
		bool ptrLoaded = false;
		for (int i = 0; i < 4; i++) {
			if (flagCache[i].allocated && flagCache[i].dirty) {
				if (!ptrLoaded) {
					*ptr++ = PPC_LWZ(PPC_R12, 1, 84);
					ptrLoaded = true;
				}
				*ptr++ = PPC_STW(flagCache[i].hostReg, PPC_R12, i * 4);
			}
		}
	};

	// Initialize all GBA R0-R14 as unallocated (with 0 age)
	for (int i = 0; i < 15; i++) regCache[i] = {false, false, 0, 0};

	u32 currentAge = 0; // Tracks register access sequence for LRU spilling

	// O(1) Allocation Math supporting pinned instruction-local masks to prevent active operand eviction
	auto FindOrAllocateHostReg = [&](u8 gbaReg, u32*& ptr, bool loadFromMem, u32& lockedMask) -> u8 {
		if (gbaReg == 15) return 29;

		if (regCache[gbaReg].allocated) {
			regCache[gbaReg].age = ++currentAge; // Update age on Cache Hit
			lockedMask |= (1 << regCache[gbaReg].hostReg);
			return regCache[gbaReg].hostReg;
		}

		u32 inUseMask = lockedMask;
		for (int i = 0; i < 15; i++) {
			if (regCache[i].allocated) inUseMask |= (1 << regCache[i].hostReg);
		}

		// R15 to R28 boundaries = Bits 15 to 28
		u32 freeMask = (~inUseMask) & 0x1FFF8000;
		u8 freeReg = 0;

		if (freeMask != 0) {
			freeReg = 31 - __builtin_clz(freeMask); // O(1) hardware evaluation
		} else {
			// LRU Register Spilling Protocol
			u32 oldestAge = 0xFFFFFFFF;
			int spillTarget = -1;

			// Scan for the Least Recently Used register
			for (int i = 0; i < 15; i++) {
				if (regCache[i].allocated && ((lockedMask & (1 << regCache[i].hostReg)) == 0) && regCache[i].age < oldestAge) {
					oldestAge = regCache[i].age;
					spillTarget = i;
				}
			}

			// Evict the LRU register
			if (regCache[spillTarget].dirty) {
				*ptr++ = PPC_STW(regCache[spillTarget].hostReg, 14, spillTarget * 4);
			}
			regCache[spillTarget].allocated = false;
			regCache[spillTarget].dirty = false;
			freeReg = regCache[spillTarget].hostReg;
		}

		// Claim the free (or freshly evicted) host register
		regCache[gbaReg].allocated = true;
		regCache[gbaReg].dirty = false;
		regCache[gbaReg].hostReg = freeReg;
		regCache[gbaReg].age = ++currentAge; // Update age on Allocation
		lockedMask |= (1 << freeReg);

		if (loadFromMem) *ptr++ = PPC_LWZ(freeReg, 14, gbaReg * 4);

		return freeReg;
	};

	// Wrappers for explicit Intent
	auto ReadGBAReg = [&](u8 gbaReg, u32*& ptr, u32& lockedMask) -> u8 {
	    return FindOrAllocateHostReg(gbaReg, ptr, true, lockedMask);
	};

	auto WriteGBAReg = [&](u8 gbaReg, u32*& ptr, bool fullOverwrite, u32& lockedMask) -> u8 {
	    // If it's a complete 32-bit overwrite (like MOV), skip the LWZ memory fetch!
	    u8 hReg = FindOrAllocateHostReg(gbaReg, ptr, !fullOverwrite, lockedMask);
	    if (gbaReg < 15) regCache[gbaReg].dirty = true;
	    return hReg;
	};

	// Global Flush Protocol
	auto FlushDirtyRegisters = [&](u32*& ptr) {
	    for (int i = 0; i < 15; i++) {
	        if (regCache[i].allocated && regCache[i].dirty) {
	            *ptr++ = PPC_STW(regCache[i].hostReg, 14, i * 4);
	            regCache[i].dirty = false; // Mark clean to prevent double-stores on branching
	        }
	    }
	};

	// Global Flush Protocol (State-Preserving for Branches)
	auto EmitDirtyRegisterFlush = [&](u32*& ptr) {
		for (int i = 0; i < 15; i++) {
			if (regCache[i].allocated && regCache[i].dirty) {
				*ptr++ = PPC_STW(regCache[i].hostReg, 14, i * 4);
			}
		}
	};

	auto EmitResultMetadata = [&](u32*& ptr, u32 count, u32 bailedOut) {
	    *ptr++ = PPC_LWZ(PPC_R10, 1, 88);			// Load outResult ptr
	    *ptr++ = PPC_LWZ(PPC_R11, PPC_R10, 8);		// Load outResult->instructions
	    *ptr++ = PPC_ADDI(PPC_R11, PPC_R11, count);	// Accumulate count
	    *ptr++ = PPC_STW(PPC_R11, PPC_R10, 8);		// Store instructions
	    *ptr++ = PPC_LI(PPC_R11, bailedOut);		// Set bailedOut boolean
	    *ptr++ = PPC_STW(PPC_R11, PPC_R10, 12);		// Store bailedOut
	};

	// Lazily claims arena space (and emits the prologue) the FIRST time we're
	// actually about to commit real code -- not up front. Walking cold/unsupported
	// code costs a couple of branches and nothing else: no allocate(), no rewind(),
	// no arena traffic, no registerBlock spam beyond the one at the end.
	auto EnsureArenaAllocated = [&]() {
	    if (arenaAllocated) return;
	    arenaAllocated = true;

	    arenaOffsetStart = cache.getArenaOffset();
	    emitPtr = cache.allocateJITMemory(MAX_WORDS * sizeof(u32));
	    blockStart = emitPtr;

	    // EVENT QUOTA SHIELD
	    *emitPtr++ = PPC_CMPWI(0, PPC_R3, YIELD_NUMBER);
	    quotaGuard = emitPtr;
	    *emitPtr++ = PPC_BGE(0);
	};

    auto EmitPrefetchSync = [&](u32*& ptr, u32 cInstrCount, u32 cStaticCost, u32 pc) {
		if (cInstrCount == 0) {
			if (cStaticCost > 0) *ptr++ = PPC_ADDI(PPC_R3, PPC_R3, cStaticCost);
			return;
		}

		u32 bank = (pc >> 24) & 15;
		u32 seqCost = memoryWaitSeq[bank];

		if (cStaticCost > 0) *ptr++ = PPC_ADDI(PPC_R3, PPC_R3, cStaticCost);

		// Only ROM banks have utilized prefetch buffers
		if (bank >= 0x08 && bank <= 0x0D) {
			// 1. Calculate available prefetch hits: C = 31 - cntlzw((R5 & 0xFF) + 1)
			*ptr++ = PPC_RLWINM(PPC_R11, PPC_R5, 0, 24, 31);
			*ptr++ = PPC_ADDI(PPC_R11, PPC_R11, 1);
			*ptr++ = PPC_CNTLZW(PPC_R11, PPC_R11);
			*ptr++ = PPC_LI(PPC_R10, 31);
			*ptr++ = PPC_SUBF(PPC_R11, PPC_R11, PPC_R10);

			// 2. Clamp hits to instructions executed: H = min(C, instrCount)
			*ptr++ = PPC_CMPWI(0, PPC_R11, cInstrCount);
			*ptr++ = PPC_BLE(8); // If C <= instrCount, skip the override clamp
			*ptr++ = PPC_LI(PPC_R11, cInstrCount);

			// 3. Subtract H * seqCost from R3
			if (seqCost == 1) {
				*ptr++ = PPC_SUBF(PPC_R3, PPC_R11, PPC_R3);
			} else if (seqCost == 2) {
				*ptr++ = PPC_RLWINM(PPC_R10, PPC_R11, 1, 0, 30); // R10 = H * 2
				*ptr++ = PPC_SUBF(PPC_R3, PPC_R10, PPC_R3);
			} else {
				*ptr++ = PPC_MULLI(PPC_R10, PPC_R11, seqCost);
				*ptr++ = PPC_SUBF(PPC_R3, PPC_R10, PPC_R3);
			}

			// 4. Consume hits from the prefetch buffer: R5 >>= H
			*ptr++ = PPC_RLWINM(PPC_R10, PPC_R5, 0, 0, 23); // Preserve upper bits
			*ptr++ = PPC_RLWINM(PPC_R5, PPC_R5, 0, 24, 31);
			*ptr++ = PPC_SRW(PPC_R5, PPC_R5, PPC_R11);
			*ptr++ = PPC_OR(PPC_R5, PPC_R5, PPC_R10);

			// 5. Hardware reset quirk: If H == 0 and R5 was > 0xFF, clear R5.
			*ptr++ = PPC_CMPWI(0, PPC_R11, 0);
			*ptr++ = PPC_BNE(16); // Skip if H != 0 (16 bytes = 4 instructions)
			*ptr++ = PPC_CMPWI(0, PPC_R10, 0); // Check upper bits
			*ptr++ = PPC_BEQ(8);  // Skip if R10 == 0
			*ptr++ = PPC_LI(PPC_R5, 0);
		}
	};

    auto EmitPrefetchDataWait = [&](u32*& ptr, u32 bankReg, u32 dataWaitStateReg, u32 scratchReg, u32 pc) {
		u32 execBank = (pc >> 24) & 15;
		// The hardware prefetcher only runs if the CPU is executing from ROM
		if (execBank < 0x08 || execBank > 0x0D) return;

		u32 seqCost = memoryWaitSeq[execBank];
		if (seqCost == 0) seqCost = 1; // Fallback safety

		// Natively emulates C++ recharge without obliterating an already-active buffer
		*ptr++ = PPC_CMPWI(0, bankReg, 8);
		u32* branchRecharge = ptr++; // Branch to Recharge Path if bank < 8

		// Bank >= 8 (ROM) Data Read completely flushes prefetch!
		*ptr++ = PPC_LI(PPC_R5, 0);
		u32* branchSkip = ptr++; // Skip recharge logic

		// Recharge Path (Bank < 8)
		*branchRecharge = PPC_BLT((u32)((ptr - branchRecharge) * 4));

		// 1. Calculate hits (waitState / seqCost) into scratchReg
		if (seqCost == 1) {
			*ptr++ = PPC_OR(scratchReg, dataWaitStateReg, dataWaitStateReg);
		} else if (seqCost == 2) {
			*ptr++ = PPC_SRWI(scratchReg, dataWaitStateReg, 1);
		} else {
			*ptr++ = PPC_SRWI(scratchReg, dataWaitStateReg, 1); // Safe fallback
		}

		// 2. Clamp hits to 8 maximum safely
		*ptr++ = PPC_CMPWI(0, scratchReg, 8);
		u32* branchClamp = ptr++;
		*ptr++ = PPC_LI(scratchReg, 8);
		*branchClamp = PPC_BLE((u32)((ptr - branchClamp) * 4));

		// 3. Shift existing hits in R5 by the total wait cycles FIRST
		*ptr++ = PPC_SLW(PPC_R5, PPC_R5, scratchReg);

		// 4. Create bitmask: (1 << hits) - 1
		// We use dataWaitStateReg as a secondary scratch to absolutely avoid the PPC_ADDI R0 trap.
		*ptr++ = PPC_LI(dataWaitStateReg, 1);
		*ptr++ = PPC_SLW(scratchReg, dataWaitStateReg, scratchReg);  // scratchReg = 1 << hits
		*ptr++ = PPC_SUBF(scratchReg, dataWaitStateReg, scratchReg); // scratchReg = scratchReg - 1

		// 5. Append hits
		*ptr++ = PPC_OR(PPC_R5, PPC_R5, scratchReg); // Add new hits
		*ptr++ = PPC_ORI(PPC_R5, PPC_R5, 0x100);     // Ensure active flag

		*branchSkip = PPC_B((u32)((ptr - branchSkip) * 4));
	};

    while (!endBlock && instrCount < MAX_INSTRUCTIONS) {
		u32 lockedMask = 0; // Reset locked tracking pins per guest execution step

		// BUFFER OVERFLOW PROTECTION: Ensure we have enough words for the worst-case instruction + Epilogue
    	if (arenaAllocated && (emitPtr - blockStart) > (s32)(MAX_WORDS - EPILOGUE_RESERVE_WORDS - bailoutCount * MAX_BAILOUT_STUB_WORDS)) {
    	    endBlock = true;
    	    JIT_LOG_BAILOUT(currentPC, 0, BAILOUT_BUFFER_OVERFLOW);
    	    break;
    	}

    	u16 opcode = CPUReadHalfWord(currentPC);

    	// THUMB Formats 1, 2, 3, 4 - Native ALU (Shifts, Add, Sub, Mov, Cmp)
		if ((opcode & 0xC000) == 0x0000 || (opcode & 0xFC00) == 0x4000) {
			// Format 3: Move, Compare, Add, Subtract Immediate
			if ((opcode & 0xE000) == 0x2000) {
			    u8 op = (opcode >> 11) & 0x03; // 0=MOV, 1=CMP, 2=ADD, 3=SUB
			    u8 rd = (opcode >> 8) & 0x07;
			    u8 imm = opcode & 0xFF;
			    
			    //static const char* fmt[] = { "MOV", "CMP", "ADD", "SUB" };
				//JIT_LOG_INSN_COMPILED(currentPC, opcode, "%s R%u, #0x%02X", fmt[op], rd, imm);

			    EnsureArenaAllocated();

			    if (op == 0) { // MOV
			    	// Optimization: Full overwrite skips the underlying LWZ fetch
					u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask);
					u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);

					*emitPtr++ = PPC_LI(hostRd, imm);
					*emitPtr++ = PPC_LI(fN, 0);
					*emitPtr++ = PPC_LI(fZ, (imm == 0) ? 1 : 0);
				} else {
			        u32 hostRd = ReadGBAReg(rd, emitPtr, lockedMask);
			        *emitPtr++ = PPC_LI(PPC_R12, imm);

			        if (op == 1) { // CMP (Rd - Imm, result discarded)
			            *emitPtr++ = PPC_SUBFCO(PPC_R11, PPC_R12, hostRd);
			        } else if (op == 2) { // ADD
			            hostRd = WriteGBAReg(rd, emitPtr, false, lockedMask); // Mark Dirty
			            *emitPtr++ = PPC_ADDCO(hostRd, hostRd, PPC_R12);
			        } else if (op == 3) { // SUB
			            hostRd = WriteGBAReg(rd, emitPtr, false, lockedMask); // Mark Dirty
			            *emitPtr++ = PPC_SUBFCO(hostRd, PPC_R12, hostRd);
			        }

			        // Extract Hardware C and V Flags from XER natively
			        u8 fC = WriteFlag(FLAG_C);
					u8 fV = WriteFlag(FLAG_V);
			        *emitPtr++ = PPC_MFXER(PPC_R10);
			        *emitPtr++ = PPC_RLWINM(fC, PPC_R10, 3, 31, 31);
			        *emitPtr++ = PPC_RLWINM(fV, PPC_R10, 2, 31, 31);

			        // Extract N and Z Flags
			        u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);
			        u32 flagSrc = (op == 1) ? PPC_R11 : hostRd;
			        *emitPtr++ = PPC_SRWI(fN, flagSrc, 31);
			        *emitPtr++ = PPC_CNTLZW(fZ, flagSrc);
					// STRICT Z-FLAG CLAMP: Rotate IBM Bit 26 (value 32) to Bit 31, and mask ONLY Bit 31.
			        *emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);
			    }
			    chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			// Format 2: ADD / SUB (Register & Immediate)
			else if ((opcode & 0xF800) == 0x1800) {
				EnsureArenaAllocated();

				u8 rd = opcode & 0x07;
				u8 rs = (opcode >> 3) & 0x07;
				u8 type = (opcode >> 9) & 0x03; // 0=ADD reg, 1=SUB reg, 2=ADD imm, 3=SUB imm
				u8 rn_imm = (opcode >> 6) & 0x07;

				u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
				u32 hostRn = 0;
				if (type == 0 || type == 1) hostRn = ReadGBAReg(rn_imm, emitPtr, lockedMask); // read BEFORE write
				u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask);

				// Stage the right-hand operand natively into R12
				if (type == 0 || type == 1) {
					*emitPtr++ = PPC_OR(PPC_R12, hostRn, hostRn);
				} else {
					*emitPtr++ = PPC_LI(PPC_R12, rn_imm);
				}

				// Execute Math utilizing Broadway's Fixed-Point Exception Register (XER)
				if (type == 0 || type == 2) *emitPtr++ = PPC_ADDCO(hostRd, hostRs, PPC_R12); // ADD
				else                        *emitPtr++ = PPC_SUBFCO(hostRd, PPC_R12, hostRs); // SUB (Rs - R12)

				// Extract Hardware C and V Flags from XER (Branchless)
				u8 fC = WriteFlag(FLAG_C);
				u8 fV = WriteFlag(FLAG_V);
				*emitPtr++ = PPC_MFXER(PPC_R11);
				*emitPtr++ = PPC_RLWINM(fC, PPC_R11, 3, 31, 31); // IBM Bit 2 (CA) -> Bit 31
				*emitPtr++ = PPC_RLWINM(fV, PPC_R11, 2, 31, 31); // IBM Bit 1 (OV) -> Bit 31

				// Extract N and Z Flags
				u8 fN = WriteFlag(FLAG_N);
				u8 fZ = WriteFlag(FLAG_Z);
				*emitPtr++ = PPC_SRWI(fN, hostRd, 31);
				*emitPtr++ = PPC_CNTLZW(fZ, hostRd);
				// STRICT Z-FLAG CLAMP: Rotate IBM Bit 26 (value 32) to Bit 31, and mask ONLY Bit 31.
				*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			// Format 1: LSL / LSR / ASR
			else if ((opcode & 0x1800) != 0x1800 && (opcode & 0xE000) == 0x0000) {
				EnsureArenaAllocated();

				u8 rd = opcode & 0x07;
				u8 rs = (opcode >> 3) & 0x07;
				u8 offset = (opcode >> 6) & 0x1F;
				u8 op = (opcode >> 11) & 0x03; // 0=LSL, 1=LSR, 2=ASR

				u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
				u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask); // True: Rd is completely overwritten

				if (op == 0) { // LSL
					if (offset == 0) {
						*emitPtr++ = PPC_OR(hostRd, hostRs, hostRs); // MOV
					} else {
						// Correct IBM Bit-Math: Rotate Left by (offset)
						u8 fC = WriteFlag(FLAG_C);
						*emitPtr++ = PPC_RLWINM(fC, hostRs, offset, 31, 31);
						*emitPtr++ = PPC_RLWINM(hostRd, hostRs, offset, 0, 31 - offset);
					}
				}
				else if (op == 1) { // LSR
					if (offset == 0) {
						// LSR #32: ARM bit 31 (IBM bit 0) goes to carry. Rotate Left by 1.
						u8 fC = WriteFlag(FLAG_C);
						*emitPtr++ = PPC_RLWINM(fC, hostRs, 1, 31, 31);
						*emitPtr++ = PPC_LI(hostRd, 0);
					} else {
						// Correct IBM Bit-Math: Rotate Left by (33 - offset) & 31
						u8 fC = WriteFlag(FLAG_C);
						*emitPtr++ = PPC_RLWINM(fC, hostRs, (33 - offset) & 31, 31, 31);
						*emitPtr++ = PPC_SRWI(hostRd, hostRs, offset);
					}
				}
				else if (op == 2) { // ASR
					if (offset == 0) {
						// ASR #32: ARM bit 31 (IBM bit 0) goes to carry. Rotate Left by 1.
						u8 fC = WriteFlag(FLAG_C);
						*emitPtr++ = PPC_RLWINM(fC, hostRs, 1, 31, 31);

						// Protect host XER CA flag from being clobbered by srawi
						*emitPtr++ = PPC_MFXER(PPC_R10);
						*emitPtr++ = PPC_SRAWI(hostRd, hostRs, 31);
						*emitPtr++ = PPC_MTXER(PPC_R10);
					} else {
						u8 fC = WriteFlag(FLAG_C);
						*emitPtr++ = PPC_RLWINM(fC, hostRs, (33 - offset) & 31, 31, 31);

						// Protect host XER CA flag from being clobbered by srawi
						*emitPtr++ = PPC_MFXER(PPC_R10);
						*emitPtr++ = PPC_SRAWI(hostRd, hostRs, offset);
						*emitPtr++ = PPC_MTXER(PPC_R10);
					}
				}

				// Extract N and Z Flags natively from the host register
				u8 fN = WriteFlag(FLAG_N);
				u8 fZ = WriteFlag(FLAG_Z);
				*emitPtr++ = PPC_SRWI(fN, hostRd, 31);
				*emitPtr++ = PPC_CNTLZW(fZ, hostRd);
				// STRICT Z-FLAG CLAMP: Rotate IBM Bit 26 (value 32) to Bit 31, and mask ONLY Bit 31.
				*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);
				
				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			// Format 4: ALU Operations
			else if ((opcode & 0xFC00) == 0x4000) {
				u8 op = (opcode >> 6) & 0x0F;
				u8 rs = (opcode >> 3) & 0x07;
				u8 rd = opcode & 0x07;

				if (op == 10) { // CMP (Compare)
					EnsureArenaAllocated();
					u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
					u32 hostRd = ReadGBAReg(rd, emitPtr, lockedMask); // CMP does not modify Rd

					*emitPtr++ = PPC_SUBFCO(PPC_R12, hostRs, hostRd); // R12 = Rd - Rs

					u8 fC = WriteFlag(FLAG_C);
					u8 fV = WriteFlag(FLAG_V);
					*emitPtr++ = PPC_MFXER(PPC_R11);
					*emitPtr++ = PPC_RLWINM(fC, PPC_R11, 3, 31, 31);
					*emitPtr++ = PPC_RLWINM(fV, PPC_R11, 2, 31, 31);

					u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);
					*emitPtr++ = PPC_SRWI(fN, PPC_R12, 31);
					*emitPtr++ = PPC_CNTLZW(fZ, PPC_R12);
					*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 0 || op == 1 || op == 12 || op == 14) { // AND, EOR, ORR, BIC
					EnsureArenaAllocated();
					u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
					u32 hostRd = WriteGBAReg(rd, emitPtr, false, lockedMask); // Reads Rd, then modifies it

					if (op == 0)  *emitPtr++ = PPC_AND(hostRd, hostRd, hostRs);
					if (op == 1)  *emitPtr++ = PPC_XOR(hostRd, hostRd, hostRs);
					if (op == 12) *emitPtr++ = PPC_OR(hostRd, hostRd, hostRs);
					if (op == 14) *emitPtr++ = PPC_ANDC(hostRd, hostRd, hostRs); // BIC

					// Extract N and Z Flags
					u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);
					*emitPtr++ = PPC_SRWI(fN, hostRd, 31);
					*emitPtr++ = PPC_CNTLZW(fZ, hostRd);
					*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				} else if (op == 9) { // NEG (Rd = 0 - Rs)
					EnsureArenaAllocated();
					u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
					u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask); // Fully overwrites Rd

					*emitPtr++ = PPC_LI(PPC_R12, 0); // Load 0 into scratch
					*emitPtr++ = PPC_SUBFCO(hostRd, hostRs, PPC_R12); // Rd = R12 (0) - Rs

					// Extract Hardware C and V Flags natively from XER
					u8 fC = WriteFlag(FLAG_C);
					u8 fV = WriteFlag(FLAG_V);
					*emitPtr++ = PPC_MFXER(PPC_R11);
					*emitPtr++ = PPC_RLWINM(fC, PPC_R11, 3, 31, 31);
					*emitPtr++ = PPC_RLWINM(fV, PPC_R11, 2, 31, 31);

					// Extract N and Z Flags
					u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);
					*emitPtr++ = PPC_SRWI(fN, hostRd, 31);
					*emitPtr++ = PPC_CNTLZW(fZ, hostRd);
					*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 15) { // MVN (Bitwise NOT: Rd = ~Rs)
					EnsureArenaAllocated();
					u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
					u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask); // Fully overwrites Rd

					*emitPtr++ = PPC_NOR(hostRd, hostRs, hostRs);

					// MVN only updates N and Z
					u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);
					*emitPtr++ = PPC_SRWI(fN, hostRd, 31);
					*emitPtr++ = PPC_CNTLZW(fZ, hostRd);
					*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 8) { // TST (AND flags only, discard result)
					EnsureArenaAllocated();
					u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
					u32 hostRd = ReadGBAReg(rd, emitPtr, lockedMask); // TST does not modify Rd

					*emitPtr++ = PPC_AND(PPC_R12, hostRd, hostRs); // R12 = Rd & Rs

					u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);
					*emitPtr++ = PPC_SRWI(fN, PPC_R12, 31);
					*emitPtr++ = PPC_CNTLZW(fZ, PPC_R12);
					*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 13) { // MUL (Rd = Rd * Rs)
					EnsureArenaAllocated();
					u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
					u32 hostRd = WriteGBAReg(rd, emitPtr, false, lockedMask); // Reads Rd, then modifies it

					// thumb43_1's real cost depends on the ORIGINAL Rd's magnitude
					// (ARM7TDMI multiply early-termination): rm = |original Rd|-ish
					// (sign-complemented if negative), active_bits = 31-clz(rm|1),
					// cost = 2 + (active_bits>>3) [0..3] + codeTicksAccess16(armNextPC).
					// Save the original Rd before MULLW overwrites it.
					*emitPtr++ = PPC_OR(PPC_R11, hostRd, hostRd);
					*emitPtr++ = PPC_MULLW(hostRd, hostRd, hostRs);

					// rm = (original Rd < 0) ? ~original Rd : original Rd - branchless
					// via arithmetic-shift sign mask (0 or -1), same trick as thumb43_1.
					*emitPtr++ = PPC_MFXER(PPC_R10); // Protect host XER CA flag from srawi
					*emitPtr++ = PPC_SRAWI(PPC_R12, PPC_R11, 31);
					*emitPtr++ = PPC_MTXER(PPC_R10);
					*emitPtr++ = PPC_XOR(PPC_R11, PPC_R11, PPC_R12);
					*emitPtr++ = PPC_ORI(PPC_R11, PPC_R11, 1); // avoid clz(0)

					// active_bits = 31 - cntlzw(rm); (>>3) maps 0-31 to 0..3
					*emitPtr++ = PPC_CNTLZW(PPC_R12, PPC_R11);
					*emitPtr++ = PPC_LI(PPC_R10, 31);
					*emitPtr++ = PPC_SUBF(PPC_R12, PPC_R12, PPC_R10);
					*emitPtr++ = PPC_SRWI(PPC_R12, PPC_R12, 3);
					*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R12);

					// MUL only updates N and Z in Thumb
					u8 fN = WriteFlag(FLAG_N);
					u8 fZ = WriteFlag(FLAG_Z);
					*emitPtr++ = PPC_SRWI(fN, hostRd, 31);
					*emitPtr++ = PPC_CNTLZW(fZ, hostRd);
					*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

					// Non-sequential code-fetch table - thumb43_1 uses codeTicksAccess16
					// (non-seq) even though MUL isn't a guest memory access; that's just
					// what the real multiplier timing quirk calls. Flat "+2" (was "+1",
					// missing thumb43_1's separate "clockTicks = 1" base plus its final "+ 1").
					chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + 2;
				} else {
					endBlock = true;
					JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNSUPPORTED_ALU);
					break;
				}
			}
		}
		// THUMB Format 5 - High Register Operations (ADD / CMP / MOV)
		else if ((opcode & 0xFC00) == 0x4400 && ((opcode >> 8) & 0x03) != 3) {
			u8 op = (opcode >> 8) & 0x03; // 0=ADD, 1=CMP, 2=MOV
			u8 h1 = (opcode >> 7) & 0x01;
			u8 h2 = (opcode >> 6) & 0x01;
			u8 rs = (opcode >> 3) & 0x07;
			u8 rd = opcode & 0x07;

			u8 actualRs = rs | (h2 << 3);
			u8 actualRd = rd | (h1 << 3);

			// Modifying the Program Counter directly triggers a branch pipeline flush.
			// We bail these out to C++ to handle the complex timing sync.
			// NOTE: CMP (op == 1) only reads Rd, it does not modify it, so skip bailout!
			if (actualRd == 15 && op != 1) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_CONDITIONAL_BRANCH);
				break;
			}

			EnsureArenaAllocated();

			if (op == 1) { // CMP
				u32 regRd = (actualRd == 15) ? PPC_R11 : ReadGBAReg(actualRd, emitPtr, lockedMask);
				u32 regRs = (actualRs == 15) ? PPC_R10 : ReadGBAReg(actualRs, emitPtr, lockedMask);

				// Stage PC natively into scratch registers if it is used as an operand
				if (actualRd == 15) {
					*emitPtr++ = PPC_LIS(regRd, (currentPC + 4) >> 16);
					*emitPtr++ = PPC_ORI(regRd, regRd, (currentPC + 4) & 0xFFFF);
				}
				if (actualRs == 15) {
					*emitPtr++ = PPC_LIS(regRs, (currentPC + 4) >> 16);
					*emitPtr++ = PPC_ORI(regRs, regRs, (currentPC + 4) & 0xFFFF);
				}

				// Execute Math utilizing Broadway's Fixed-Point Exception Register (XER)
				// PPC_SUBFCO(rD, rA, rB) computes rB - rA[cite: 2]. So R12 = regRd - regRs.
				*emitPtr++ = PPC_SUBFCO(PPC_R12, regRs, regRd);

				// Extract Hardware C and V Flags from XER (Branchless)
				u8 fC = WriteFlag(FLAG_C);
				u8 fV = WriteFlag(FLAG_V);
				*emitPtr++ = PPC_MFXER(PPC_R10);
				*emitPtr++ = PPC_RLWINM(fC, PPC_R10, 3, 31, 31); // IBM Bit 2 (CA) -> Bit 31
				*emitPtr++ = PPC_RLWINM(fV, PPC_R10, 2, 31, 31); // IBM Bit 1 (OV) -> Bit 31

				// Extract N and Z Flags from the Result (PPC_R12)
				u8 fN = WriteFlag(FLAG_N);
				u8 fZ = WriteFlag(FLAG_Z);
				*emitPtr++ = PPC_SRWI(fN, PPC_R12, 31);
				*emitPtr++ = PPC_CNTLZW(fZ, PPC_R12);
				// STRICT Z-FLAG CLAMP: Rotate IBM Bit 26 (value 32) to Bit 31, and mask ONLY Bit 31.
				*emitPtr++ = PPC_RLWINM(fZ, fZ, 27, 31, 31);

				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			else if (op == 2) { // MOV
				u32 regRs = 0;
				if (actualRs != 15) regRs = ReadGBAReg(actualRs, emitPtr, lockedMask); // read before write
				u32 regRd = WriteGBAReg(actualRd, emitPtr, true, lockedMask); // Rd != 15 due to bailout

				if (actualRs == 15) {
					*emitPtr++ = PPC_LIS(regRd, (currentPC + 4) >> 16);
					*emitPtr++ = PPC_ORI(regRd, regRd, (currentPC + 4) & 0xFFFF);
				} else {
					*emitPtr++ = PPC_OR(regRd, regRs, regRs);
				}

				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			else if (op == 0) { // ADD
				u32 regRs = 0;
				if (actualRs != 15) regRs = ReadGBAReg(actualRs, emitPtr, lockedMask); // read before write
				u32 regRd = WriteGBAReg(actualRd, emitPtr, false, lockedMask); // Rd != 15 due to bailout

				if (actualRs == 15) {
					*emitPtr++ = PPC_LIS(PPC_R12, (currentPC + 4) >> 16);
					*emitPtr++ = PPC_ORI(PPC_R12, PPC_R12, (currentPC + 4) & 0xFFFF);
					*emitPtr++ = PPC_ADD(regRd, regRd, PPC_R12);
				} else {
					*emitPtr++ = PPC_ADD(regRd, regRd, regRs);
				}

				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
		}
		// THUMB Format 5 - Branch Exchange (BX Rs)
		else if ((opcode & 0xFF00) == 0x4700) {
			EnsureArenaAllocated();

			u8 rs = (opcode >> 3) & 0x0F;

			// FLUSH DIRTY FLAGS AND REGISTERS: Crucial sync before a dynamic block exit
			FlushDirtyFlags(emitPtr);
			FlushDirtyRegisters(emitPtr);

			// Synchronize outResult metadata before dynamic exit
			EmitResultMetadata(emitPtr, instrCount + 1, 0);

			// Protect against dynamic reads of R15 causing a stale PC desync
			if (rs == 15) {
				*emitPtr++ = PPC_LIS(PPC_R12, (currentPC + 4) >> 16);
				*emitPtr++ = PPC_ORI(PPC_R12, PPC_R12, (currentPC + 4) & 0xFFFF);
			} else {
				u32 hostRs = ReadGBAReg(rs, emitPtr, lockedMask);
				*emitPtr++ = PPC_OR(PPC_R12, hostRs, hostRs);
			}

			// Extract Bit 0 to check if we are switching to ARM mode
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R12, 0, 31, 31);
			*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);

			u32* branchArmSwitch = emitPtr++;
			RegisterBailout(branchArmSwitch, COND_BEQ, currentPC, chunkStaticCycles);

			// TRUE PATH: Stay in THUMB, exit block dynamically
			// PIPELINE SYNC: Dynamic branch forces a pipeline flush (+3 cycles)
			u32 takenPenalty = STATIC_CODE_TICKS_SEQ16(currentPC) + 3;
			EmitPrefetchSync(emitPtr, chunkInstrCount + 1, chunkStaticCycles + takenPenalty, chunkStartPC);
			*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer
			*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R12, 0, 0, 30); // R4 = TargetPC & ~1

			// Do NOT call linkerStubAddress. Return to C++ host instead.
			s32 returnOffset = (s32)((u8*)cache.linkerReturnAddress - (u8*)emitPtr);
			*emitPtr++ = PPC_B(returnOffset);

			endBlock = true;
			break;
		}
		// THUMB Format 6 - PC-Relative Load (LDR Rd, [PC, #Imm])
		else if ((opcode & 0xF800) == 0x4800) {
			u8 rd = (opcode >> 8) & 0x07;
			u8 imm = opcode & 0xFF;
			u32 targetAddr = ((currentPC + 4) & ~3) + (imm << 2);

			//JIT_LOG_INSN_COMPILED(currentPC, opcode, "LDR R%u, [PC, #0x%X] ; (0x%08X)", rd, imm, targetAddr);

			u8 bank = targetAddr >> 24;
			// ONLY statically bake BIOS (0x00) and ROM (0x08-0x0D).
			// Bank 0x0E+ contains SRAM/Registers which mutate!
			if (bank == 0x00 || (bank >= 0x08 && bank <= 0x0D)) {
				EnsureArenaAllocated();

				// Sync the pipeline BEFORE processing the load to balance the chunk ledger
				EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0);
				chunkInstrCount = 0;
				chunkStaticCycles = 0;
				chunkStartPC = currentPC + 2;

				u32 loadedValue = CPUReadMemory(targetAddr);

				// LAZY REGISTERS: Full overwrite bypasses the memory fetch
				u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask);

				if ((loadedValue & ~0x7FFF) == 0) {
					*emitPtr++ = PPC_LI(hostRd, loadedValue);
				} else if ((loadedValue & ~0xFFFF) == 0) {
					*emitPtr++ = PPC_LI(hostRd, 0);
					*emitPtr++ = PPC_ORI(hostRd, hostRd, loadedValue);
				} else if ((loadedValue & 0xFFFF) == 0) {
					*emitPtr++ = PPC_LIS(hostRd, loadedValue >> 16);
				} else {
					*emitPtr++ = PPC_LIS(hostRd, loadedValue >> 16);
					*emitPtr++ = PPC_ORI(hostRd, hostRd, loadedValue & 0xFFFF);
				}

				// Format 6 is always a 32-bit load, and its target is a compile-time
				// constant, so (unlike Format 9/10/11) the real data-access wait-state
				// cost folds directly into staticCycles here. Matches thumb48's
				// `3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC)`:
				// non-sequential code table (a memory access breaks the sequential
				// prefetch stream), +3 (load constant, not +2 which was the store one).
				chunkStaticCycles += STATIC_DATA_TICKS_32(targetAddr) + STATIC_CODE_TICKS_16(currentPC) + 3;
			} else {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNSUPPORTED_MEM_BANK);
				break;
			}
		}
		// THUMB Formats 9, 10, 11 - Unified Memory Loads AND Stores
		else if (((opcode & 0xE000) == 0x6000) || ((opcode & 0xF000) == 0x5000) || ((opcode & 0xF000) == 0x8000)) {
			bool isMemLoad = false;
			bool isMemStore = false;
			u8 rd = 0, rb = 0, ro = 0, imm = 0;
			u32 accessType = 0; // 4=Word, 2=Halfword, 1=Byte

			// Deferred emission variables
			u32 immediateOffset = 0;
			bool useRegisterOffset = false;

			// Format 9: LDR/STR Rd, [Rb, #Imm] (Word Access)
			if ((opcode & 0xF000) == 0x6000) {
				rd = opcode & 0x07;
				rb = (opcode >> 3) & 0x07;
				imm = (opcode >> 6) & 0x1F;
				immediateOffset = imm << 2;
				accessType = 4;
				isMemLoad = (opcode & 0x0800) != 0;
				isMemStore = !isMemLoad;
			}
			// Format 9: LDRB/STRB Rd, [Rb, #Imm] (Byte Access)
			else if ((opcode & 0xF000) == 0x7000) {
				rd = opcode & 0x07;
				rb = (opcode >> 3) & 0x07;
				imm = (opcode >> 6) & 0x1F;
				immediateOffset = imm; // Byte access requires no shift
				accessType = 1;
				isMemLoad = (opcode & 0x0800) != 0;
				isMemStore = !isMemLoad;
			}
			// Format 8: LDRH/STRH Rd, [Rb, #Imm] (Halfword Access)
			else if ((opcode & 0xF000) == 0x8000) {
				rd = opcode & 0x07;
				rb = (opcode >> 3) & 0x07;
				imm = (opcode >> 6) & 0x1F;
				immediateOffset = imm << 1; // Halfword access immediate offset is multiplied by 2
				accessType = 2;
				isMemLoad = (opcode & 0x0800) != 0;
				isMemStore = !isMemLoad;
			}
			// Format 10: Register Offset Loads & Stores (LDR, LDRH, LDRB, STR, STRH, STRB)
			else if ((opcode & 0xF000) == 0x5000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; ro = (opcode >> 6) & 0x07;
				u16 subOp = opcode & 0x0E00;
				if (subOp <= 0x0C00 && subOp != 0x0600) {
					useRegisterOffset = true;
					if (subOp == 0x0800) { isMemLoad = true; accessType = 4; }
					else if (subOp == 0x0A00) { isMemLoad = true; accessType = 2; }
					else if (subOp == 0x0C00) { isMemLoad = true; accessType = 1; }
					else if (subOp == 0x0000) { isMemStore = true; accessType = 4; }
					else if (subOp == 0x0200) { isMemStore = true; accessType = 2; }
					else if (subOp == 0x0400) { isMemStore = true; accessType = 1; }
				}
			}

			if (isMemLoad || isMemStore) {
				EnsureArenaAllocated();

				// Emit the deferred Effective Address calculation natively into R12
				u32 hostRb = ReadGBAReg(rb, emitPtr, lockedMask);

				if (useRegisterOffset) {
					u32 hostRo = ReadGBAReg(ro, emitPtr, lockedMask);
					*emitPtr++ = PPC_ADD(PPC_R12, hostRb, hostRo);
				} else {
					*emitPtr++ = PPC_ADDI(PPC_R12, hostRb, immediateOffset);
				}

				EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
				chunkInstrCount = 0;
				chunkStaticCycles = 0;
				chunkStartPC = currentPC + 2;

				// 1. Extract Memory Bank (R12 >> 24)
				*emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);
				// Stash the bank in R4 (instruction-local scratch only - nothing here
				// needs it to survive to the next instruction) since R11 gets
				// overwritten by the page/mask lookup below, and we need the bank
				// again afterward for the data-access cycle-cost lookup.
				*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R11, 0, 28, 31); // R4 = R11 & 15

				if (isMemStore) {
					// STORE STRICT GUARD: Only Banks 2 & 3 (WRAM) allowed
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
					*emitPtr++ = PPC_BEQ(12);
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
					u32* branchGuard1 = emitPtr++;
					RegisterBailout(branchGuard1, COND_BNE, currentPC, chunkStaticCycles);
				} else {
					// LOAD GUARD: Block BIOS (0), MMIO (4), and EEPROM/SRAM (>= 0x0D)
					// 1. BIOS GUARD (Bank 0x00) - Enforce "Open-Bus" protection
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
					u32* branchGuard3 = emitPtr++;
					RegisterBailout(branchGuard3, COND_BEQ, currentPC, chunkStaticCycles);

					// 2. MMIO HARDWARE GUARD (Bank 0x04) - Trigger hardware updates
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 4);
					u32* branchGuard1 = emitPtr++;
					RegisterBailout(branchGuard1, COND_BEQ, currentPC, chunkStaticCycles);

					// 3. EEPROM/SRAM SAVE GUARD (Banks >= 0x0D) - Enforce save intercepts
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 13);
					u32* branchGuard2 = emitPtr++;
					RegisterBailout(branchGuard2, COND_BGE, currentPC, chunkStaticCycles);

					// 4. REAL-TIME CLOCK (RTC) GUARD (Bank 0x08, Offsets 0xC4-0xC8)
					// Prevent Pok�mon games from fetching raw ROM data instead of the clock response.
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 8);
					u32* branchSkipRTC = emitPtr++;

					// Extract the lower 16 bits of the Effective Address (R4 = R12 & 0xFFFF)
					*emitPtr++ = PPC_RLWINM(PPC_R10, PPC_R12, 0, 16, 31);
					*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0x00C4);
					u32* branchRTC_Low = emitPtr++;

					*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0x00C8);
					u32* branchGuard4 = emitPtr++;
					RegisterBailout(branchGuard4, COND_BLE, currentPC, chunkStaticCycles);

					// Patch the RTC skip branches to cleanly hop over the bailout trigger
					*branchSkipRTC = PPC_BNE((u32)((emitPtr - branchSkipRTC) * 4));
					*branchRTC_Low = PPC_BLT((u32)((emitPtr - branchRTC_Low) * 4));
				}

				// 2. Load Page Pointer and Memory Mask
				*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29);
				*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
				*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

				// 3. UNIVERSAL NULL POINTER GUARD
				*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
				u32* branchNullToBailout = emitPtr++;
				RegisterBailout(branchNullToBailout, COND_BEQ, currentPC, chunkStaticCycles);

				*emitPtr++ = PPC_AND(PPC_R12, PPC_R12, PPC_R11);

				// Alignment Fix - Prevent Broadway Alignment Exception Storms by masking the EA natively
				if (accessType == 4) {
					*emitPtr++ = PPC_RLWINM(PPC_R12, PPC_R12, 0, 0, 29); // Clear bits 30-31 (Word align)
				} else if (accessType == 2) {
					*emitPtr++ = PPC_RLWINM(PPC_R12, PPC_R12, 0, 0, 30); // Clear bit 31 (Halfword align)
				}

				// 4.5 RUNTIME DATA-ACCESS CYCLE LOOKUP (safe path only - every guard
				// above has already passed by this point, so R3 is correctly left
				// untouched on every bailout path). Mirrors dataTicksAccess32/16:
				// word accesses use memoryWait32[], byte/halfword share memoryWait[].
				// The bank isn't known until runtime here (unlike Format 6's
				// PC-relative case), so this has to be an emitted table lookup
				// rather than a compile-time constant.

				u8* dataTicksTable = (accessType == 4) ? memoryWait32 : memoryWait;
				*emitPtr++ = PPC_LIS(PPC_R11, ((u32)dataTicksTable) >> 16);
				*emitPtr++ = PPC_ORI(PPC_R11, PPC_R11, ((u32)dataTicksTable) & 0xFFFF);
				*emitPtr++ = PPC_LBZX(PPC_R11, PPC_R4, PPC_R11); // EA = R4 + R11
				*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R11);

				EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R11, PPC_R0, currentPC); // Use R0 as scratch

				// 5. Execute Memory Load or Store Instruction
				if (isMemLoad) {
					u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask);
					if (accessType == 4) *emitPtr++ = PPC_LWBRX(hostRd, PPC_R10, PPC_R12);
					else if (accessType == 2) *emitPtr++ = PPC_LHBRX(hostRd, PPC_R10, PPC_R12);
					else *emitPtr++ = PPC_LBZX(hostRd, PPC_R10, PPC_R12);
				} else {
					u32 hostRd = ReadGBAReg(rd, emitPtr, lockedMask);
					if (accessType == 4) *emitPtr++ = PPC_STWBRX(hostRd, PPC_R10, PPC_R12);
					else if (accessType == 2) *emitPtr++ = PPC_STHBRX(hostRd, PPC_R10, PPC_R12);
					else *emitPtr++ = PPC_STBZX(hostRd, PPC_R10, PPC_R12);
				}

				// Non-sequential table for the code-fetch component: a data-bus access
				// disrupts the prefetch pipeline, so the interpreter's real formula
				// (thumb68 etc: `dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2/3`)
				// always pays the non-sequential cost for the instruction after a memory op.
				// C++ Boolean promotion removes pipeline-stalling ternary
				chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + (2 + isMemLoad);
			} else {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNKNOWN_MEM_OP);
				break;
			}
		}
		// THUMB Format 11: SP-relative Load/Store (LDR/STR Rd, [SP, #imm])
		else if ((opcode & 0xF000) == 0x9000) {
			EnsureArenaAllocated();

			// SP-relative loads/stores also break the prefetch stream!
			EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
			chunkInstrCount = 0;
			chunkStaticCycles = 0;
			chunkStartPC = currentPC + 2;

			u8 isLoad = (opcode >> 11) & 0x01;
			u8 rd     = (opcode >> 8) & 0x07;
			u32 offset = (opcode & 0xFF) << 2;

			// 1. Lazily allocate SP (GBA R13) and Calculate Target Address in PPC_R12
			u32 hostSp = ReadGBAReg(13, emitPtr, lockedMask);
			if (offset == 0) {
				*emitPtr++ = PPC_OR(PPC_R12, hostSp, hostSp);
			} else {
				*emitPtr++ = PPC_ADDI(PPC_R12, hostSp, offset);
			}

			// 2. Bank Guard Check: Check if EA is in EWRAM (0x02) or IWRAM (0x03)
			// (EA >> 25) == 1 is true IF AND ONLY IF bank is 2 or 3.
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R12, 7, 25, 31); // R11 = EA >> 25
			*emitPtr++ = PPC_CMPWI(0, PPC_R11, 1);

			u32* branchGuard = emitPtr++;
			RegisterBailout(branchGuard, COND_BNE, currentPC, chunkStaticCycles);

			// 3. Host Address Translation
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R12, 8, 24, 31); // R11 = bank (EA >> 24)
			// Stash the bank in R4 (unused elsewhere in this handler) before R11
			// gets turned into a table index below - needed for the data-tick
			// lookup once the guard has passed.
			*emitPtr++ = PPC_OR(PPC_R4, PPC_R11, PPC_R11);
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29);  // R11 = bank * 4
			*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11); // R10 = readPages[bank]
			*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11); // R11 = readMasks[bank]
			*emitPtr++ = PPC_AND(PPC_R12, PPC_R12, PPC_R11);       // R12 = EA & mask

			// Alignment Fix - SP-relative ops are always Word (32-bit) accesses
			*emitPtr++ = PPC_RLWINM(PPC_R12, PPC_R12, 0, 0, 29);   // Clear bits 30-31

			// 3.5 RUNTIME DATA-ACCESS CYCLE LOOKUP (safe path only - the guard
			// above has already passed by this point). Always a 32-bit access,
			// so always memoryWait32[] - matches thumb90/thumb98's
			// dataTicksAccess32(address). The bank is fixed to 2 or 3 by the
			// guard, but not known at compile time (SP is a runtime value).
			*emitPtr++ = PPC_LIS(PPC_R11, ((u32)memoryWait32) >> 16);
			*emitPtr++ = PPC_ORI(PPC_R11, PPC_R11, ((u32)memoryWait32) & 0xFFFF);
			*emitPtr++ = PPC_LBZX(PPC_R11, PPC_R4, PPC_R11); // Safe: rA=R4
			*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R11);
			EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R11, PPC_R0, currentPC); // Recharge prefetch buffer

			// 4. Endian-Correct Load or Store (Lazy Register Execution)
			if (isLoad) {
				u32 hostRd = WriteGBAReg(rd, emitPtr, true, lockedMask); // Full overwrite bypasses read
				*emitPtr++ = PPC_LWBRX(hostRd, PPC_R10, PPC_R12);
			} else {
				u32 hostRd = ReadGBAReg(rd, emitPtr, lockedMask);
				*emitPtr++ = PPC_STWBRX(hostRd, PPC_R10, PPC_R12);
			}

			// thumb90 (STR): `dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2`
			// thumb98 (LDR): `3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC)`
			// Non-sequential code-fetch table (a memory access breaks the sequential
			// prefetch stream), and the flat constant depends on load vs store -
			// this previously used +2 for both, which under-counts every LDR by 1.
			chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + (isLoad ? 3 : 2);
		}
		// THUMB Format 13: SP-relative Add / Subtract (ADD/SUB SP, #Imm)
		else if ((opcode & 0xFF00) == 0xB000) {
			EnsureArenaAllocated();

			u32 offset = (opcode & 0x7F) << 2;
			bool isSub = (opcode & 0x0080) != 0;

			// LAZY REGISTERS: Load SP natively, mark as modified for the write-back
			// 'false' means we need the current value before overwriting
			u32 hostSp = WriteGBAReg(13, emitPtr, false, lockedMask);

			// Safely execute math using Broadway's immediate addition
			// (hostSp is guaranteed by our allocator to be R15-R28, avoiding the R0 literal trap)
			if (isSub) {
				*emitPtr++ = PPC_ADDI(hostSp, hostSp, -offset);
			} else {
				*emitPtr++ = PPC_ADDI(hostSp, hostSp, offset);
			}

			// Format 13 does NOT update condition flags.
			// Pure register math costs standard sequential cycles.
			chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
		}
    	// THUMB Format 14 - PUSH / POP
		else if ((opcode & 0xF600) == 0xB400) {
			bool isPop = (opcode & 0x0800) != 0;
			bool Rbit = (opcode & 0x0100) != 0;
			u8 regList = opcode & 0xFF;

			int numRegs = 0;
			for (int i = 0; i < 8; i++) if (regList & (1 << i)) numRegs++;
			if (Rbit) numRegs++;

			if (numRegs == 0) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_PUSH_POP_REGS);
				break;
			}

			EnsureArenaAllocated();

			EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
			chunkInstrCount = 0;
			chunkStaticCycles = 0;
			chunkStartPC = currentPC + 2;

			// LAZY REGISTERS: Stage SP natively into R12
			u32 hostSp = WriteGBAReg(13, emitPtr, false, lockedMask); // Reads and marks dirty

			if (!isPop) {
				// PUSH: Decrement SP first. PPC_ADDI handles negative offsets.
				*emitPtr++ = PPC_ADDI(hostSp, hostSp, -numRegs * 4);
				*emitPtr++ = PPC_OR(PPC_R12, hostSp, hostSp);
			} else {
				// POP: Use SP as base address directly.
				*emitPtr++ = PPC_OR(PPC_R12, hostSp, hostSp);
			}

			// Alignment Fix - Base execution alignment (Enforce 32-bit word alignment for the memory loop)
			*emitPtr++ = PPC_RLWINM(PPC_R12, PPC_R12, 0, 0, 29);

			// 1. Extract Memory Bank (R12 >> 24)
			*emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);
			// Stash the bank in R4 (free until the per-register loop below starts
			// using it) since R11 becomes the mask and R5 becomes the address
			// tracker before we get a chance to look up the data-access cost.
			*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R11, 0, 28, 31); // R4 = R11 & 15
			bool pushPopAccumulateDataTicks = !(isPop && !Rbit);

			if (!isPop) {
				// STORE STRICT GUARD: Only Banks 2 & 3
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
				*emitPtr++ = PPC_BEQ(12);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
				u32* branchGuard1 = emitPtr++;
				RegisterBailout(branchGuard1, COND_BNE, currentPC, chunkStaticCycles, COMP_REVERT_SP, numRegs * 4);
			} else {
				// LOAD GUARD: Allow WRAM and ROM. Block BIOS (0), MMIO (4), and EEPROM/SRAM (>= 0x0D)
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
				u32* branchGuard3 = emitPtr++;
				RegisterBailout(branchGuard3, COND_BEQ, currentPC, chunkStaticCycles);

				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 4);
				u32* branchGuard1 = emitPtr++;
				RegisterBailout(branchGuard1, COND_BEQ, currentPC, chunkStaticCycles);

				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 13); // EEPROM BLOCK
				u32* branchGuard2 = emitPtr++;
				RegisterBailout(branchGuard2, COND_BGE, currentPC, chunkStaticCycles);
			}

			// 2. Load Page Pointer and Mask
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29); // R11 = Bank * 4
			*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
			*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

			// 3. Null Pointer Guard
			*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
			u32* branchNullToBailout = emitPtr++;
			RegisterBailout(branchNullToBailout, COND_BEQ, currentPC, chunkStaticCycles, (!isPop) ? COMP_REVERT_SP : COMP_NONE, (!isPop) ? numRegs * 4 : 0);

			// 3.5 RUNTIME DATA-ACCESS CYCLE LOOKUP (safe path only).
			// PUSH_REG/POP_REG charge dataTicksAccess32 (non-seq) for the first
			// register and dataTicksAccessSeq32 (seq) for every register after
			// that, all in the same bank (SP never crosses a bank mid-instruction).
			// numRegs is compile-time-known, so the repeat count is unrolled here
			// (avoids mullw per Broadway convention) - only the two table values
			// themselves need a runtime lookup. Skipped entirely for POP{Rlist}
			// (see pushPopAccumulateDataTicks above).
			// Dynamic Cycle Calculation & Prefetcher Recharge
			if (pushPopAccumulateDataTicks) {
				// We must do a runtime lookup because bank is dynamic (SP). R4 holds the bank.

				// 1. First register is non-sequential (memoryWait32)
				*emitPtr++ = PPC_LIS(PPC_R0, ((u32)memoryWait32) >> 16);
				*emitPtr++ = PPC_ORI(PPC_R0, PPC_R0, ((u32)memoryWait32) & 0xFFFF);
				*emitPtr++ = PPC_LBZX(PPC_R0, PPC_R4, PPC_R0); // R0 = nWait

				*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R0);
				EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R0, PPC_R4, currentPC); // Recharge using R4 as scratch

				// R4 was clobbered by the lambda. Safely restore it from R12 base address.
				*emitPtr++ = PPC_SRWI(PPC_R4, PPC_R12, 24);
				*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R4, 0, 28, 31); // R4 = Bank

				if (numRegs > 1) {
					// 2. Sequential cycles for remaining registers (memoryWaitSeq)
					*emitPtr++ = PPC_LIS(PPC_R0, ((u32)memoryWaitSeq) >> 16);
					*emitPtr++ = PPC_ORI(PPC_R0, PPC_R0, ((u32)memoryWaitSeq) & 0xFFFF);
					*emitPtr++ = PPC_LBZX(PPC_R0, PPC_R4, PPC_R0); // R0 = sWait

					if ((numRegs - 1) > 1) {
						*emitPtr++ = PPC_MULLI(PPC_R0, PPC_R0, numRegs - 1); // R0 = sWait * (numRegs - 1)
					}

					*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R0);
					EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R0, PPC_R4, currentPC); // Recharge total seq wait
					// R4 is clobbered again, but it's safe as it's immediately
					// overwritten by PPC_AND in the memory loop below.
				}
			}

			// 4. Memory Operations Loop
			for (int i = 0; i < 8; i++) {
				if (regList & (1 << i)) {
					*emitPtr++ = PPC_AND(PPC_R4, PPC_R12, PPC_R11); // Apply Mask
					if (isPop) {
						u32 hostRd = WriteGBAReg(i, emitPtr, true, lockedMask);
						*emitPtr++ = PPC_LWBRX(hostRd, PPC_R10, PPC_R4);
					} else {
						u32 hostRs = ReadGBAReg(i, emitPtr, lockedMask);
						*emitPtr++ = PPC_STWBRX(hostRs, PPC_R10, PPC_R4);
					}
					*emitPtr++ = PPC_ADDI(PPC_R12, PPC_R12, 4); // Advance 4 bytes
				}
			}
			if (Rbit) {
				*emitPtr++ = PPC_AND(PPC_R4, PPC_R12, PPC_R11);
				if (isPop) {
					*emitPtr++ = PPC_LWBRX(PPC_R12, PPC_R10, PPC_R4); // POP PC into R12 scratch
				} else {
					u32 hostLr = ReadGBAReg(14, emitPtr, lockedMask);
					*emitPtr++ = PPC_STWBRX(hostLr, PPC_R10, PPC_R4); // PUSH LR (GBA R14)
				}
			}

			// 5. Update SP (POP only)
			if (isPop) {
				// Safely re-fetch the current physical location of SP in case the LRU evicted it mid-loop
				u32 currentHostSp = WriteGBAReg(13, emitPtr, false, lockedMask);
				*emitPtr++ = PPC_ADDI(currentHostSp, currentHostSp, numRegs * 4);
			}

			// 6. Branch out
			if (isPop && Rbit) {
				// POP PC: We must exit the block dynamically

				// PIPELINE SYNC: Dynamic branch forces a pipeline flush (+3 cycles)
				// Extract PC into R4 BEFORE flushing, as FlushDirtyFlags clobbers R12
				*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R12, 0, 0, 30); // R4 = TargetPC & ~1

				FlushDirtyFlags(emitPtr);
				FlushDirtyRegisters(emitPtr);

				// Synchronize outResult metadata before returning to C++ host
				EmitResultMetadata(emitPtr, instrCount + 1, 0);

				u32 takenPenalty = numRegs + STATIC_CODE_TICKS_16(currentPC) * 2 + 3;
				EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles + takenPenalty, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer

				// Do NOT call linkerStubAddress. Return to C++ host instead.
				s32 returnOffset = (s32)((u8*)cache.linkerReturnAddress - (u8*)emitPtr);
				*emitPtr++ = PPC_B(returnOffset);

				endBlock = true;
			}
			else if (isPop && !Rbit) {
				// POP {Rlist}: thumbBC ends with `clockTicks = 2 + codeTicksAccess16(...)`
				// - a plain assignment, not +=, which discards every +1-per-register
				// AND every per-register data-tick that POP_REG built up. Verified
				// directly against GBA-thumb.cpp. Faithfully reproduce that discard
				// (numRegs deliberately excluded here) rather than "fixing" it into
				// something more hardware-accurate but interpreter-divergent.
				chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + 2;
			} else if (!(isPop && Rbit)) {
				// PUSH {Rlist} / PUSH {Rlist, LR}: thumbB4/B5 use += throughout, so
				// numRegs' +1s and their data-ticks (already accumulated into R3
				// above) both survive, plus each one's own flat "+1".
				chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + numRegs + 1;
			}
			// (POP {Rlist, PC} already emitted its own exit above and always ends
			// the block there, so it never reaches this trailing accumulation.)
		}
    	// THUMB Format 15 - Multiple Load/Store (LDMIA / STMIA)
		else if ((opcode & 0xF000) == 0xC000) {
			bool isLoad = (opcode & 0x0800) != 0;
			u8 rb = (opcode >> 8) & 0x07;
			u8 regList = opcode & 0xFF;

			int numRegs = 0;
			for (int i = 0; i < 8; i++) if (regList & (1 << i)) numRegs++;

			if (numRegs == 0) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_LDMIA_STMIA_REGS);
				break;
			}

			EnsureArenaAllocated();

			EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
			chunkInstrCount = 0;
			chunkStaticCycles = 0;
			chunkStartPC = currentPC + 2;

			// Stage Base Address natively into R12
			u32 hostRb = ReadGBAReg(rb, emitPtr, lockedMask);
			*emitPtr++ = PPC_OR(PPC_R12, hostRb, hostRb);

			// Alignment Fix
			*emitPtr++ = PPC_RLWINM(PPC_R12, PPC_R12, 0, 0, 29); // Enforce 32-bit word alignment on base

			// 1. Extract Memory Bank (R12 >> 24)
			*emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);

			if (!isLoad) {
				// STMIA GUARD: Only Banks 2 & 3
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
				*emitPtr++ = PPC_BEQ(12);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
				u32* branchGuard1 = emitPtr++;
				RegisterBailout(branchGuard1, COND_BNE, currentPC, chunkStaticCycles);
			} else {
				// LDMIA GUARD: Allow WRAM and ROM. Block BIOS (0), MMIO (4), and EEPROM/SRAM (>= 0x0D)
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
				u32* branchGuard3 = emitPtr++;
				RegisterBailout(branchGuard3, COND_BEQ, currentPC, chunkStaticCycles);

				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 4);
				u32* branchGuard1 = emitPtr++;
				RegisterBailout(branchGuard1, COND_BEQ, currentPC, chunkStaticCycles);

				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 13); // EEPROM BLOCK
				u32* branchGuard2 = emitPtr++;
				RegisterBailout(branchGuard2, COND_BGE, currentPC, chunkStaticCycles);
			}

			// 2. Load Page Pointer and Mask
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29);
			*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
			*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

			// 3. Null Pointer Guard
			*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
			u32* branchNullToBailout = emitPtr++;
			RegisterBailout(branchNullToBailout, COND_BEQ, currentPC, chunkStaticCycles);

			// 4. Memory Operations Loop
			for (int i = 0; i < 8; i++) {
				if (regList & (1 << i)) {
					*emitPtr++ = PPC_AND(PPC_R4, PPC_R12, PPC_R11); // Apply Mask
					if (isLoad) {
						u32 hostRd = WriteGBAReg(i, emitPtr, true, lockedMask);
						*emitPtr++ = PPC_LWBRX(hostRd, PPC_R10, PPC_R4);
					} else {
						u32 hostRs = ReadGBAReg(i, emitPtr, lockedMask);
						*emitPtr++ = PPC_STWBRX(hostRs, PPC_R10, PPC_R4);
					}
					*emitPtr++ = PPC_ADDI(PPC_R12, PPC_R12, 4); // ADVANCE R12 DIRECTLY
				}
			}

			// 5. Writeback to Base Register (Rn)
			// ARM protocol: If Rb is in the load list, the loaded value overrides writeback.
			bool writeback = true;
			if (isLoad && (regList & (1 << rb))) writeback = false;

			if (writeback) {
				u32 hostRbWB = WriteGBAReg(rb, emitPtr, false, lockedMask);
				*emitPtr++ = PPC_ADDI(hostRbWB, hostRbWB, numRegs * 4);
			}

			// thumbC0 (STMIA) / thumbC8 (LDMIA) both end with a plain assignment -
			// `clockTicks = (1 or 2) + codeTicksAccess16(armNextPC)` - which discards
			// every +1-per-register and per-register data-tick that THUMB_STM_REG/
			// THUMB_LDM_REG accumulated.
			// The real cost is just this flat constant + one code-fetch lookup;
			// `numRegs` never survives to be observable, so it's deliberately
			// excluded here rather than added (as it was before this fix).
			chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + (isLoad ? 2 : 1);
		}
    	// THUMB Format 16 - Conditional Branches (Bcc)
		else if ((opcode & 0xF000) == 0xD000 && (opcode & 0x0F00) != 0x0F00) {
			u8 cond = (opcode >> 8) & 0x0F;
			s8 offset = (s8)(opcode & 0xFF);
			u32 targetPC = currentPC + 4 + (offset << 1);

			bool supported = false;
			bool isComposite = false;
			bool branchIfSet = false;
			u32 flagReg = 0;
			u8 fN, fZ, fC, fV;

			// Ensure the arena is mapped BEFORE requesting lazy flags!
			EnsureArenaAllocated();

			// Map native PowerPC hardware registers to GBA condition flags
			switch (cond) {
				case 0x0: flagReg = ReadFlag(FLAG_Z, emitPtr); branchIfSet = true;  supported = true; break; // EQ (Z==1)
				case 0x1: flagReg = ReadFlag(FLAG_Z, emitPtr); branchIfSet = false; supported = true; break; // NE (Z==0)
				case 0x2: flagReg = ReadFlag(FLAG_C, emitPtr); branchIfSet = true;  supported = true; break; // CS (C==1)
				case 0x3: flagReg = ReadFlag(FLAG_C, emitPtr); branchIfSet = false; supported = true; break; // CC (C==0)
				case 0x4: flagReg = ReadFlag(FLAG_N, emitPtr); branchIfSet = true;  supported = true; break; // MI (N==1)
				case 0x5: flagReg = ReadFlag(FLAG_N, emitPtr); branchIfSet = false; supported = true; break; // PL (N==0)
				case 0x6: flagReg = ReadFlag(FLAG_V, emitPtr); branchIfSet = true;  supported = true; break; // VS (V==1)
				case 0x7: flagReg = ReadFlag(FLAG_V, emitPtr); branchIfSet = false; supported = true; break; // VC (V==0)
				case 0x8: isComposite = true; branchIfSet = true;  supported = true; break;  // HI (C==1 & Z==0)
				case 0x9: isComposite = true; branchIfSet = false; supported = true; break;  // LS (C==0 | Z==1)
				case 0xA: isComposite = true; branchIfSet = true;  supported = true; break;  // GE (N==V)
				case 0xB: isComposite = true; branchIfSet = false; supported = true; break;  // LT (N!=V)
				case 0xC: isComposite = true; branchIfSet = true;  supported = true; break;  // GT (Z==0 & N==V)
				case 0xD: isComposite = true; branchIfSet = false; supported = true; break;  // LE (Z==1 | N!=V)
			}

			if (supported) {
				u32* branchSkipTruePath = nullptr;
				bool branchIfZero = false;

				if (!isComposite) {
					// If branchIfSet == true, we skip the True Path when Flag == 0 (Equal to 0)
					// If branchIfSet == false, we skip the True Path when Flag == 1 (Not Equal to 0)
					*emitPtr++ = PPC_CMPWI(0, flagReg, 0);
					branchSkipTruePath = emitPtr;
					if (branchIfSet) *emitPtr++ = PPC_BEQ(0);
					else             *emitPtr++ = PPC_BNE(0);
				} else {
					if (cond == 0x8 || cond == 0x9) {
						// HI takes branch if (C & ~Z) == 1. LS takes branch if (C & ~Z) == 0.
						fC = ReadFlag(FLAG_C, emitPtr);
						fZ = ReadFlag(FLAG_Z, emitPtr);
						*emitPtr++ = PPC_ANDC(PPC_R11, fC, fZ);
						branchIfZero = (cond == 0x9); // LS
					} else if (cond == 0xA || cond == 0xB) {
						// GE takes branch if (N ^ V) == 0. LT takes branch if (N ^ V) != 0.
						fN = ReadFlag(FLAG_N, emitPtr);
						fV = ReadFlag(FLAG_V, emitPtr);
						*emitPtr++ = PPC_XOR(PPC_R11, fN, fV);
						branchIfZero = (cond == 0xA); // GE
					} else if (cond == 0xC || cond == 0xD) {
						// GT takes branch if Z | (N ^ V) == 0. LE takes branch if Z | (N ^ V) != 0.
						fN = ReadFlag(FLAG_N, emitPtr);
						fV = ReadFlag(FLAG_V, emitPtr);
						fZ = ReadFlag(FLAG_Z, emitPtr);
						*emitPtr++ = PPC_XOR(PPC_R11, fN, fV);
						*emitPtr++ = PPC_OR(PPC_R11, PPC_R11, fZ);
						branchIfZero = (cond == 0xC); // GT
					}
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
					branchSkipTruePath = emitPtr;
					if (branchIfZero) *emitPtr++ = PPC_BNE(0);
					else              *emitPtr++ = PPC_BEQ(0);
				}

				// TRUE PATH (Branch Taken Exit)
				u32 takenPenalty = STATIC_CODE_TICKS_SEQ16(currentPC) + 1 +
										   STATIC_CODE_TICKS_SEQ16(targetPC) + STATIC_CODE_TICKS_16(targetPC) + 2;

				EmitPrefetchSync(emitPtr, chunkInstrCount + 1, chunkStaticCycles + takenPenalty, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer

				// Emit the dirty flags and registers WITHOUT clearing the compiler's tracking state
				EmitDirtyFlagFlush(emitPtr);
				EmitDirtyRegisterFlush(emitPtr);
				
				// Synchronize outResult metadata before exiting block
				EmitResultMetadata(emitPtr, instrCount + 1, 0);

				// Synchronize Pipeline PC
				*emitPtr++ = PPC_LIS(PPC_R29, (targetPC + 4) >> 16);
				*emitPtr++ = PPC_ORI(PPC_R29, PPC_R29, (targetPC + 4) & 0xFFFF);

				*emitPtr++ = PPC_LIS(PPC_R4, targetPC >> 16);
				*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, targetPC & 0xFFFF);
				s32 takenStubOffset = (s32)((u8*)cache.linkerStubAddress - (u8*)emitPtr);
				*emitPtr++ = PPC_BL(takenStubOffset);

				// Back-patch the guard branch to skip exactly the stub we just emitted
				u32* truePathEnd = emitPtr;
				u32 skipOffset = (u32)((truePathEnd - branchSkipTruePath) * 4);
				bool guardBranchIsBEQ = isComposite ? !branchIfZero : branchIfSet;
				*branchSkipTruePath = guardBranchIsBEQ ? PPC_BEQ(skipOffset) : PPC_BNE(skipOffset);

				// FALSE PATH (Branch Not Taken)
				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC + 2) + 1;
			} else {
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_CONDITIONAL_BRANCH);
				endBlock = true;
				break;
			}
		}
		// THUMB Format 18 - Unconditional Branch (B)
		else if ((opcode & 0xF800) == 0xE000) {
			EnsureArenaAllocated();

			// 1. Calculate Target PC
			// Extract 11-bit offset, shift left 21 bits to align sign bit,
			// then arithmetic shift right 20 bits (sign extends and multiplies by 2).
			s32 sOffset = (s32)((opcode & 0x07FF) << 21);
			sOffset >>= 20;
			u32 targetPC = currentPC + 4 + sOffset;

			// 2. Calculate Pipeline Penalty
			// Unconditional branch breaks prefetch and forces a full N+S cycle refill
			u32 takenPenalty = STATIC_CODE_TICKS_SEQ16(currentPC) + 1 +
							   STATIC_CODE_TICKS_SEQ16(targetPC) + STATIC_CODE_TICKS_16(targetPC) + 2;

			EmitPrefetchSync(emitPtr, chunkInstrCount + 1, chunkStaticCycles + takenPenalty, chunkStartPC);
			*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer

			// 3. JIT EXIT: Flush State
			FlushDirtyFlags(emitPtr);
			FlushDirtyRegisters(emitPtr);

			// Synchronize outResult metadata before exiting block
			EmitResultMetadata(emitPtr, instrCount + 1, 0);

			// 4. Synchronize Pipeline PC & Linker Stub Address
			*emitPtr++ = PPC_LIS(PPC_R29, (targetPC + 4) >> 16);
			*emitPtr++ = PPC_ORI(PPC_R29, PPC_R29, (targetPC + 4) & 0xFFFF);

			*emitPtr++ = PPC_LIS(PPC_R4, targetPC >> 16);
			*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, targetPC & 0xFFFF);
			s32 takenStubOffset = (s32)((u8*)cache.linkerStubAddress - (u8*)emitPtr);
			*emitPtr++ = PPC_BL(takenStubOffset);

			// Unconditional branch ends the block naturally
			endBlock = true;
			break;
		}
		// THUMB Formats 18 & 19 - Branch with Link (BL)
		else if ((opcode & 0xF800) == 0xF000) {
			u16 nextOpcode = CPUReadHalfWord(currentPC + 2);
			if ((nextOpcode & 0xF800) == 0xF800) {
				EnsureArenaAllocated();

				u32 offsetHigh = opcode & 0x07FF;
				u32 offsetLow = nextOpcode & 0x07FF;

				// Cast to signed BEFORE right shifting to force an arithmetic shift (sign extension)
				s32 sOffset = (s32)(offsetHigh << 21);
				sOffset >>= 9;
				sOffset |= (offsetLow << 1);

				u32 targetPC = currentPC + 4 + sOffset;

				// 1. Update LR (GBA R14) with the return address: (currentPC + 4) | 1
				u32 returnPC = (currentPC + 4) | 1;
				u32 hostLr = WriteGBAReg(14, emitPtr, true, lockedMask); // Overwrite completely
				*emitPtr++ = PPC_LIS(hostLr, returnPC >> 16);
				*emitPtr++ = PPC_ORI(hostLr, hostLr, returnPC & 0xFFFF);

				// 2. JIT EXIT: Branch Taken
				// PIPELINE SYNC: BL evaluates prefix at currentPC+2, and suffix at targetPC using N-cycle
				u32 takenPenalty = STATIC_CODE_TICKS_SEQ16(currentPC + 2) + 1 +
								   (STATIC_CODE_TICKS_SEQ16(targetPC) * 2) + STATIC_CODE_TICKS_16(targetPC) + 3;

				EmitPrefetchSync(emitPtr, chunkInstrCount + 1, chunkStaticCycles + takenPenalty, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer

				// Flush dirty flags and registers before dynamic block exit
				FlushDirtyFlags(emitPtr);
				FlushDirtyRegisters(emitPtr);

				// Populate result metadata directly to memory
				EmitResultMetadata(emitPtr, instrCount + 2, 0);

				// Synchronize Pipeline PC
				*emitPtr++ = PPC_LIS(PPC_R29, (targetPC + 4) >> 16);
				*emitPtr++ = PPC_ORI(PPC_R29, PPC_R29, (targetPC + 4) & 0xFFFF);

				*emitPtr++ = PPC_LIS(PPC_R4, targetPC >> 16);
				*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, targetPC & 0xFFFF);
				s32 takenStubOffset = (s32)((u8*)cache.linkerStubAddress - (u8*)emitPtr);
				*emitPtr++ = PPC_BL(takenStubOffset);

				// BL is two THUMB halfwords (prefix + suffix), not one.
				instrCount += 2;
				currentPC += 4;
				endBlock = true;
				break;
			} else {
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_BRANCH_WITH_LINK);
				endBlock = true;
				break;
			}
		}
		else {
			JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNSUPPORTED_OPCODE);
			endBlock = true;
			break;
		}

		chunkInstrCount++;
		instrCount++;
		currentPC += 2;
    }

	if (instrCount == 0) {
		JIT_LOG_ARENA(startPC, arenaOffsetStart, MAX_WORDS, 0, MAX_WORDS);
		// Nothing was ever compilable -- the arena was never touched at all.
		return cache.registerBlock(startPC, 0, nullptr);
	}

	// 1. Allocate space to jump over the bailouts for ANY fall-through path
	u32* branchSkipBailouts = nullptr;
	if (bailoutCount > 0) {
		branchSkipBailouts = emitPtr++;
	}

	// =========================================================================
	// DEFERRED BAILOUT GENERATION
	// =========================================================================
	for (u32 i = 0; i < bailoutCount; i++) {
		u32* target = emitPtr;
		u32 offset = (u32)((target - bailouts[i].branchPtr) * 4);

		// 1. Back-patch the original inline branch offset
		switch (bailouts[i].cond) {
			case COND_BEQ: *bailouts[i].branchPtr = PPC_BEQ(offset); break;
			case COND_BNE: *bailouts[i].branchPtr = PPC_BNE(offset); break;
			case COND_BGE: *bailouts[i].branchPtr = PPC_BGE(offset); break;
			case COND_BLT: *bailouts[i].branchPtr = PPC_BLT(offset); break;
			case COND_BLE: *bailouts[i].branchPtr = PPC_BLE(offset); break;
		}

		// 2. Inject compensation instructions if required (e.g., PUSH guard failure)
		// Explicitly handle memory reversion and correct compile-time/runtime synchronization
		if (bailouts[i].comp == COMP_REVERT_SP) {
			if (bailouts[i].registerSnapshot[13].allocated) {
				u8 snapSp = bailouts[i].registerSnapshot[13].hostReg;
				*emitPtr++ = PPC_ADDI(snapSp, snapSp, bailouts[i].compArg);
				*emitPtr++ = PPC_STW(snapSp, 14, 13 * 4);
			} else {
				*emitPtr++ = PPC_LWZ(PPC_R12, 14, 13 * 4);
				*emitPtr++ = PPC_ADDI(PPC_R12, PPC_R12, bailouts[i].compArg);
				*emitPtr++ = PPC_STW(PPC_R12, 14, 13 * 4);
			}
		}

		bool flagPtrLoaded = false;
		for (int r = 0; r < 4; r++) {
			if (bailouts[i].flagSnapshot[r].allocated && bailouts[i].flagSnapshot[r].dirty) {
				if (!flagPtrLoaded) {
					*emitPtr++ = PPC_LWZ(PPC_R12, 1, 84);
					flagPtrLoaded = true;
				}
				*emitPtr++ = PPC_STW(bailouts[i].flagSnapshot[r].hostReg, PPC_R12, r * 4);
			}
		}

		// 4. Serialize the Register State strictly from the point of failure
		for (int r = 0; r < 15; r++) {
			// Skip SP if it was already updated by the compensation code above
			if (r == 13 && bailouts[i].comp == COMP_REVERT_SP) continue;

			if (bailouts[i].registerSnapshot[r].allocated && bailouts[i].registerSnapshot[r].dirty) {
				*emitPtr++ = PPC_STW(bailouts[i].registerSnapshot[r].hostReg, 14, r * 4);
			}
		}

		// 5. Return to C++ Interpreter
		*emitPtr++ = PPC_ADDI(PPC_R3, PPC_R3, bailouts[i].cycles);

		// Populate result metadata directly to memory
		EmitResultMetadata(emitPtr, bailouts[i].instructions, 1);

		*emitPtr++ = PPC_LIS(PPC_R4, bailouts[i].pc >> 16);
		*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, bailouts[i].pc & 0xFFFF);
		s32 returnOffset = (s32)((u8*)cache.linkerReturnAddress - (u8*)emitPtr);
		*emitPtr++ = PPC_B(returnOffset);
	}

	// 2. Back-patch the fall-through jump to land exactly at the Epilogue
	if (branchSkipBailouts) {
		*branchSkipBailouts = PPC_B((u32)((emitPtr - branchSkipBailouts) * 4));
	}

	// Default Epilogue
	EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);

	// 1. Synchronize all modified Lazy Flags and Registers back to memory
	FlushDirtyFlags(emitPtr);
	FlushDirtyRegisters(emitPtr);

	// Populate result metadata directly to memory
	EmitResultMetadata(emitPtr, instrCount, 0);

	// 2. Synchronize R29 (GBA R15) so the incoming chained block inherits the correct pipeline PC
	*emitPtr++ = PPC_LIS(PPC_R29, (currentPC + 4) >> 16);
	*emitPtr++ = PPC_ORI(PPC_R29, PPC_R29, (currentPC + 4) & 0xFFFF);

	*emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
	*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
	s32 defaultStubOffset = (s32)((u8*)cache.linkerStubAddress - (u8*)emitPtr);
	*emitPtr++ = PPC_BL(defaultStubOffset);

	// --- QUOTA SHIELD BAILOUT STUB ---
	u32* yieldTarget = emitPtr;

	// Synchronize metadata (0 instructions executed, bailedOut = 1)
	EmitResultMetadata(emitPtr, 0, 1);

	*emitPtr++ = PPC_LIS(PPC_R4, startPC >> 16);
	*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, startPC & 0xFFFF);
	s32 yieldOffset = (s32)((u8*)cache.linkerReturnAddress - (u8*)emitPtr);
	*emitPtr++ = PPC_B(yieldOffset);

	// Patch the prologue guard to hit this yield stub
	*quotaGuard = PPC_BGE((u32)((yieldTarget - quotaGuard) * 4));

	u32 emittedWords = (u32)(emitPtr - blockStart);
	u32 actualBytes = emittedWords * sizeof(u32);
	u32 rewindAmount = MAX_WORDS - emittedWords;

	JIT_LOG_ARENA(startPC, arenaOffsetStart, MAX_WORDS, emittedWords, rewindAmount);

	cache.rewindJITMemory((MAX_WORDS - emittedWords) * sizeof(u32));
	FlushJITCache(blockStart, actualBytes);

	return cache.registerBlock(startPC, instrCount, (JITBlockFunc)blockStart);
}
#endif
