/****************************************************************************
 * Platform Abstraction Layer (WUT driver)
 * Daryl Borth 2026
 * WutCodegen.h
 *
 * JIT support functions
 * Thin wrapper around coreinit/codegen.h's sanctioned RWX-toggle mechanism
 ***************************************************************************/
#pragma once
#include <stdint.h>
#include <stddef.h>

// Attempts to claim the title's codegen slot (OSGetCodegenVirtAddrRange).
// Returns nullptr if no slot is available, or if the available slot is
// smaller than wanted - then we are forced to use the interpreter.
// On success, pins the calling thread to the codegen-owning core, since
// codegen is granted to exactly one core and every write/execute must
// happen from it.
uint32_t * WutCodegenAcquire(size_t wanted);

// The core (0-2) codegen was granted to by OSGetCodegenVirtAddrRange(), and
// the core the calling thread was pinned to as a result (diagnostic only)
int32_t WutCodegenGetCore();
int32_t WutCodegenGetPinnedCore();

// Opens a write window: flips the codegen region RW-. Nestable - only
// the outermost call actually toggles the mode, so a flushCache() that
// happens to run while a compile's own JITWriteScope is already open
// doesn't flip back to R-X early.
void WutCodegenBeginWrite();

// Accumulates [p, p+n) into the pending write window's dirty
// lo/hi watermark. Does not touch the cache/hardware itself - that
// happens once, for the whole accumulated range, in EndWrite. Safe to
// call zero or many times per window.
void WutCodegenMarkDirty(const void * p, size_t n);

// Closes a write window. On the outermost matching call: flushes the
// accumulated dirty range from D-cache, flips the region back to R-X,
// invalidates that range in I-cache, then isyncs. A window with no
// marked writes (dirty range empty) still flips back to R-X but skips
// the flush/invalidate work.
void WutCodegenEndWrite();
