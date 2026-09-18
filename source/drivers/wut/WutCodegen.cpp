/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutCodegen.cpp
 *
 * JIT support functions
 ***************************************************************************/
#include "WutCodegen.h"

#include <coreinit/codegen.h>
#include <coreinit/cache.h>
#include <coreinit/core.h>
#include <coreinit/thread.h>

namespace {
	void *   codegenBase   = nullptr;
	uint32_t codegenBytes  = 0;
	int      writeDepth    = 0;

	// Watermark for the currently-open write window. nullptr/nullptr
	// means "nothing marked dirty yet this window" - deliberately not
	// (base,base) or any real address, so a single MarkDirty call at
	// the very start of the arena is never mistaken for "empty".
	uint8_t* dirtyLo = nullptr;
	uint8_t* dirtyHi = nullptr;
}

uint32_t * WutCodegenAcquire(size_t wanted) {
	void *   addr = nullptr;
	uint32_t size = 0;
	OSGetCodegenVirtAddrRange(&addr, &size);

	if (addr == nullptr || size == 0 || size < wanted) {
		return nullptr;
	}

	codegenBase  = addr;
	codegenBytes = size;

	// Codegen is granted to exactly one core (OSGetCodegenCore()). Every
	// RW-/R-X toggle and every write/execute against this slot has to
	// happen from that core, so pin the calling (emulation) thread here
	// now that we know the slot actually exists.
	OSSetThreadAffinity(OSGetCurrentThread(), 1u << OSGetCodegenCore());
	return (uint32_t *)addr;
}

void WutCodegenBeginWrite() {
	if (!codegenBase) return;
	if (writeDepth++ == 0) {
		OSSwitchSecCodeGenMode(CODEGEN_RW_);
	}
}

void WutCodegenMarkDirty(const void * p, size_t n) {
	if (!codegenBase || n == 0) return;

	uint8_t * lo = (uint8_t *)p;
	uint8_t * hi = lo + n;
	if (!dirtyLo || lo < dirtyLo) dirtyLo = lo;
	if (!dirtyHi || hi > dirtyHi) dirtyHi = hi;
}

void WutCodegenEndWrite() {
	if (!codegenBase) return;
	if (--writeDepth > 0) return;
	if (dirtyHi > dirtyLo) {
		DCStoreRange(dirtyLo, (uint32_t)(dirtyHi - dirtyLo));
	}
	OSSwitchSecCodeGenMode(CODEGEN_R_X);
	if (dirtyHi > dirtyLo) {
		ICInvalidateRange(dirtyLo, (uint32_t)(dirtyHi - dirtyLo));
	}
	asm volatile("isync" ::: "memory");
	dirtyLo = nullptr;
	dirtyHi = nullptr;
}
