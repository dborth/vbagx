
#ifndef __VBA_FASTMATH_H__
#define __VBA_FASTMATH_H__

// 
//	ftou: float to unsigned
//
inline u32 ftou(f32 f){
	return u32(s32(f));
}
// 
//	dtou: double to unsigned
//
inline u32 dtou(double f){
	return u32(s32(f));
}
// 
//	utof: unsigned to float
//
inline f32 utof(u32 u){
	// Warning! 'u' must be within the signed int range!
	return f32(s32(u));
}
// 
//	utod: unsigned to double
//
inline double utod(u32 u){
	// Warning! 'u' must be within the signed int range!
	return double(s32(u));
}

// ===== POWERPC SPECIFIC =====
//
//	absf: absolute value of a float
//
// ===== POWERPC SPECIFIC =====
inline float absf(float f){

	// THREE instructions!

	volatile float tmp = f; 
	// This MUST be "volatile"! Without it the compiler will try to
	// optimize it and ends up using 13 instructions!

	asm("fabs 	  %0, %0     		\n\t" 
		 :   "=f" (tmp)             //output        
		 :   "f"  (tmp)             //input
	);	
	return tmp;

}
// ===== POWERPC SPECIFIC =====
//
//	absd: absolute value of a double
//
// ===== POWERPC SPECIFIC =====
inline double absd(double d){

	// THREE instructions!

	volatile double tmp = d; 
	// This MUST be "volatile"! Without it the compiler will try to
	// optimize it and ends up using 13 instructions!

	asm("fabs 	  %0, %0     		\n\t" 
		 :   "=f" (tmp)             //output        
		 :   "f"  (tmp)             //input
	);	
	return tmp;

}

#endif
