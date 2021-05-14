#ifndef arm_simd32_h
#define arm_simd32_h

#include <stdint.h>

__attribute__((always_inline))
static inline int32_t
__ssub16(int32_t x, int32_t y)
{
	int32_t result;

	__asm volatile("ssub16 %0, %1, %2" : "=r"(result)
			: "r" (x), "r" (y));
	return result;
}

__attribute__((always_inline))
static inline int32_t
__sadd16(int32_t x, int32_t y)
{
	int32_t result;

	__asm volatile("sadd16 %0, %1, %2" : "=r"(result)
			: "r" (x), "r" (y));
	return result;
}

__attribute__((always_inline))
static inline uint32_t
__sel(uint32_t x, uint32_t y)
{
	uint32_t result;

	__asm volatile("sel %0, %1, %2" : "=r"(result)
			: "r" (x), "r" (y));
	return result;
}

__attribute__((always_inline))
static inline uint32_t
__pkhbt(uint32_t x, uint32_t y, uint32_t shift)
{
	uint32_t result;

	__asm volatile("pkhbt %0, %1, %2, lsl %3" : "=r"(result)
			: "r" (x), "r" (y), "I" (shift));
	return result;
}

__attribute__((always_inline))
static inline uint32_t
__pkhtb(uint32_t x, uint32_t y, uint32_t shift)
{
	uint32_t result;

	__asm volatile("pkhtb %0, %1, %2, asr %3" : "=r"(result)
			: "r" (x), "r" (y), "I" (shift));
	return result;
}

__attribute__((always_inline))
static inline uint32_t
__ror(uint32_t x, int amount)
{
	uint32_t result;

	__asm volatile("ror %0, %1, %2" : "=r"(result)
			: "r" (x), "I" (amount));
	return result;
}

#endif
