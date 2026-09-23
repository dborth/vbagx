/****************************************************************************
 * libgui
 * Daryl Borth 2009-2026
 * Time.h
 *
 * Platform-agnostic monotonic timing.
 ***************************************************************************/
#pragma once

#include <cstdint>

#if defined(__WIIU__)
#include <coreinit/time.h>
#else
#include <ogc/timesupp.h>
#endif

//!An opaque monotonic timestamp, returned by SystemTime::now(). Not
//!wall-clock time, has no defined epoch.
typedef uint64_t Ticks;

class SystemTime
{
	public:
		//!\return the current monotonic timestamp
		static inline Ticks now()
		{
			#if defined(__WIIU__)
			return (Ticks)OSGetSystemTime();
			#else
			return (Ticks)SYS_GetSystemTime();
			#endif
		}

		//!\return whole seconds elapsed between two now() samples
		//!\param start the earlier sample
		//!\param end the later sample - must not be earlier than start
		static inline uint32_t diffSecs(Ticks start, Ticks end)
		{
			#if defined(__WIIU__)
			return (uint32_t)OSTicksToSeconds((int64_t)(end - start));
			#else
			return diff_sec(start, end);
			#endif
		}

		//!\return whole milliseconds elapsed between two now() samples
		//!\param start the earlier sample
		//!\param end the later sample - must not be earlier than start
		static inline uint32_t diffMillisecs(Ticks start, Ticks end)
		{
			#if defined(__WIIU__)
			return (uint32_t)OSTicksToMilliseconds((int64_t)(end - start));
			#else
			return diff_msec(start, end);
			#endif
		}

		//!\return whole microseconds elapsed between two now() samples.
		//!This is the granularity an emulator core's frame-sync loop
		//!actually needs.
		//!\param start the earlier sample
		//!\param end the later sample - must not be earlier than start
		static inline uint32_t diffMicrosecs(Ticks start, Ticks end)
		{
			#if defined(__WIIU__)
			return (uint32_t)OSTicksToMicroseconds((int64_t)(end - start));
			#else
			return diff_usec(start, end);
			#endif
		}

		//!\return an already-elapsed raw tick COUNT (not two timestamps to
		//!diff) converted to microseconds, as a 64-bit value. Unlike
		//!diffMicrosecs() above, this is meant for converting an
		//!accumulator that has been summing raw (end - start) tick deltas
		//!across many samples - the accumulated total can exceed what
		//!diffMicrosecs()'s 32-bit microsecond return can hold long before
		//!any single sample would.
		//!\param ticks an elapsed duration, in the same units as Ticks
		static inline uint64_t ticksToMicrosecs(Ticks ticks)
		{
			#if defined(__WIIU__)
			return (uint64_t)OSTicksToMicroseconds((int64_t)ticks);
			#else
			return (uint64_t)ticks_to_microsecs(ticks);
			#endif
		}
};
