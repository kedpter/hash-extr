/*
 * This file is part of John the Ripper password cracker,
 * Copyright (c) 1996-2002,2005,2006,2008,2010,2011,2013 by Solar Designer
 *
 * ...with changes in the jumbo patch for mingw and MSC, by JimF.
 * ...and NT_SSE2 by Alain Espinosa.
 * ...and various little things by magnum
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */

/*
 * Architecture specific parameters for x86 with SSE2 asm or intrinsics.
 */

#ifndef _JOHN_ARCH_H
#define _JOHN_ARCH_H

#if AC_BUILT
#include "autoconfig.h"
#else
#if defined (_MSC_VER) && !defined (_OPENMP)
#define __SSE2__ 1
//#define __SSSE3__ 1
//#define __SSE4_1__ 1
//#define __XOP__ 1
//#define __AVX__ 1
#endif
#define ARCH_WORD			long
#define ARCH_SIZE			4
#define ARCH_BITS			32
#define ARCH_BITS_LOG			5
#define ARCH_BITS_STR			"32"
#define ARCH_LITTLE_ENDIAN		1
#define ARCH_INT_GT_32			0
#endif

#define ARCH_ALLOWS_UNALIGNED		1
#define ARCH_INDEX(x)			((unsigned int)(unsigned char)(x))

#define CPU_DETECT			1
#define CPU_REQ				1
#define CPU_NAME			"SSE2"

#if CPU_FALLBACK && !defined(CPU_FALLBACK_BINARY)
#define CPU_FALLBACK_BINARY		"john-non-sse"
#define CPU_FALLBACK_BINARY_DEFAULT
#endif

#if __SSSE3__ || JOHN_SSSE3
#define CPU_REQ_SSSE3			1
#undef CPU_NAME
#define CPU_NAME			"SSSE3"
#if CPU_FALLBACK && !defined(CPU_FALLBACK_BINARY)
#define CPU_FALLBACK_BINARY		"john-non-ssse3"
#define CPU_FALLBACK_BINARY_DEFAULT
#endif
#endif

#if __SSE4_1__ || JOHN_SSE4_1
#define CPU_REQ_SSE4_1			1
#undef CPU_NAME
#define CPU_NAME			"SSE4.1"
#if CPU_FALLBACK && !defined(CPU_FALLBACK_BINARY)
#define CPU_FALLBACK_BINARY		"john-non-sse4.1"
#define CPU_FALLBACK_BINARY_DEFAULT
#endif
#endif

#ifdef __XOP__
#define JOHN_XOP			1
#endif
#if defined(__AVX__) || defined(JOHN_XOP)
#define JOHN_AVX			1
#endif

#define DES_ASM				1
#define DES_128K			0
#define DES_X2				1
#define DES_MASK			1
#define DES_SCALE			0
#define DES_EXTB			0
#define DES_COPY			1
#define DES_STD_ALGORITHM_NAME		"DES 48/64 4K MMX"
#define DES_BS				1
#if defined(JOHN_AVX) && (defined(__GNUC__) || defined(_OPENMP))
/*
 * Require gcc for AVX/XOP because DES_bs_all is aligned in a gcc-specific way,
 * except in OpenMP-enabled builds, where it's aligned by different means.
 */
#define CPU_REQ_AVX			1
#undef CPU_NAME
#define CPU_NAME			"AVX"
#ifdef CPU_FALLBACK_BINARY_DEFAULT
#undef CPU_FALLBACK_BINARY
#define CPU_FALLBACK_BINARY		"john-non-avx"
#endif
#define DES_BS_ASM			0
#if __AVX2__ || JOHN_AVX2
#undef CPU_NAME
#define CPU_NAME			"AVX2"
#define CPU_REQ_AVX2			1
#if CPU_FALLBACK && !defined(CPU_FALLBACK_BINARY)
#define CPU_FALLBACK_BINARY		"john-non-avx2"
#define CPU_FALLBACK_BINARY_DEFAULT
#endif
#define DES_BS_VECTOR			8
#if defined(JOHN_XOP) && defined(__GNUC__)
/* Require gcc for 256-bit XOP because of __builtin_ia32_vpcmov_v8sf256() */
#define CPU_REQ_XOP			1
#undef CPU_NAME
#define CPU_NAME			"XOP2"
#ifdef CPU_FALLBACK_BINARY_DEFAULT
#undef CPU_FALLBACK_BINARY
#define CPU_FALLBACK_BINARY		"john-non-xop"
#endif
#undef DES_BS
#define DES_BS				3
#define DES_BS_ALGORITHM_NAME		"DES 256/256 XOP2"
#else
#undef CPU_NAME
#define CPU_NAME			"AVX2"
#define DES_BS_ALGORITHM_NAME		"DES 256/256 AVX2"
#endif
#else
#define DES_BS_VECTOR			4
#ifdef JOHN_XOP
#undef DES_BS
#define DES_BS				3
#define DES_BS_ALGORITHM_NAME		"DES 128/128 XOP"
#else
#define DES_BS_ALGORITHM_NAME		"DES 128/128 AVX"
#endif
#endif
#elif defined(__SSE2__) && defined(_OPENMP)
#define DES_BS_ASM			0
#if 1
#define DES_BS_VECTOR			4
#define DES_BS_ALGORITHM_NAME		"DES 128/128 SSE2"
#elif 0
#define DES_BS_VECTOR			6
#define DES_BS_VECTOR_SIZE		8
#define DES_BS_ALGORITHM_NAME		"DES 128/128 SSE2 + 64/64 MMX"
#elif 0
#define DES_BS_VECTOR			5
#define DES_BS_VECTOR_SIZE		8
#define DES_BS_ALGORITHM_NAME		"DES 128/128 SSE2 + 32/32"
#else
#define DES_BS_VECTOR			7
#define DES_BS_VECTOR_SIZE		8
#define DES_BS_ALGORITHM_NAME		"DES 128/128 SSE2 + 64/64 MMX + 32/32"
#endif
#else
#define DES_BS_ASM			1
#define DES_BS_VECTOR			4
#define DES_BS_ALGORITHM_NAME		"DES 128/128 SSE2"
#endif
#define DES_BS_EXPAND			1

#ifdef _OPENMP
#define MD5_ASM				0
#define MD5_X2				1
#else
// NOTE, for some newer gcc compiliers, setting MD5_ASM to 2 and MD5_X2 to 1 is faster.
#define MD5_ASM				1
#define MD5_X2				0
#endif
// Also, for some compiliers, and possibly CPU's, MD5_IMM 0 would be faster.
// MORE testing needs done for these 3 items, OR
#define MD5_IMM				1

#if defined(_OPENMP) || defined(_MSC_VER) || \
    (defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)))
#define BF_ASM				0
#ifndef BF_X2
#define BF_X2				1
#endif
#else
#define BF_ASM				1
#define BF_X2				0
#endif
#define BF_SCALE			1

#ifndef JOHN_DISABLE_INTRINSICS
#ifdef __GNUC__
#define GCC_VERSION			(__GNUC__ * 10000 \
			 + __GNUC_MINOR__ * 100 \
			 + __GNUC_PATCHLEVEL__)
#endif

#if __AVX512__
#define SIMD_COEF_32 16
#define SIMD_COEF_64 8
#elif __AVX2__
#define SIMD_COEF_32 8
#define SIMD_COEF_64 4
#elif __SSE2__
#define SIMD_COEF_32 4
#define SIMD_COEF_64 2
#elif __MMX__
#define SIMD_COEF_32 2
#define SIMD_COEF_64 1
#endif

#ifndef SIMD_PARA_MD4
#if defined(__INTEL_COMPILER)
#define SIMD_PARA_MD4			3
#elif defined(__clang__)
#define SIMD_PARA_MD4			3
#elif defined (_MSC_VER)
#define SIMD_PARA_MD4			1
#elif defined(__GNUC__) && GCC_VERSION < 40405	// 4.4.5
#define SIMD_PARA_MD4			1
#elif defined(__GNUC__) && GCC_VERSION < 40500	// 4.5
#define SIMD_PARA_MD4			2
#elif defined(__GNUC__)
#define SIMD_PARA_MD4			3
#else
#define SIMD_PARA_MD4			2
#endif
#endif /* SIMD_PARA_MD4 */

#ifndef SIMD_PARA_MD5
#if defined(__INTEL_COMPILER)
#define SIMD_PARA_MD5			3
#elif defined(__clang__)
#define SIMD_PARA_MD5			4
#elif defined (_MSC_VER)
#define SIMD_PARA_MD5			1
#elif defined(__GNUC__) && GCC_VERSION < 40405	// 4.4.5
#define SIMD_PARA_MD5			1
#elif defined(__GNUC__)
#define SIMD_PARA_MD5			3
#else
#define SIMD_PARA_MD5			2
#endif
#endif /* SIMD_PARA_MD5 */

#ifndef SIMD_PARA_SHA1
#if defined(__INTEL_COMPILER)
#define SIMD_PARA_SHA1			1
#elif defined(__clang__)
#define SIMD_PARA_SHA1			3
#elif defined (_MSC_VER)
#define SIMD_PARA_SHA1			1
#elif defined(__GNUC__) && GCC_VERSION > 40600 // 4.6
#define SIMD_PARA_SHA1			2
#elif defined(__GNUC__)
#define SIMD_PARA_SHA1			1
#else
#define SIMD_PARA_SHA1			1
#endif
#endif /* SIMD_PARA_SHA1 */

#ifndef SIMD_PARA_SHA256
#define SIMD_PARA_SHA256 1
#endif
#ifndef SIMD_PARA_SHA512
#define SIMD_PARA_SHA512 1
#endif

#define STR_VALUE(arg)			#arg
#define PARA_TO_N(n)			STR_VALUE(n) "x"
#define PARA_TO_MxN(m, n)		STR_VALUE(m) "x" STR_VALUE(n)

#if SIMD_PARA_MD4 > 1
#define MD4_N_STR			PARA_TO_MxN(SIMD_COEF_32, SIMD_PARA_MD4)
#else
#define MD4_N_STR			PARA_TO_N(SIMD_COEF_32)
#endif
#if SIMD_PARA_MD5 > 1
#define MD5_N_STR			PARA_TO_MxN(SIMD_COEF_32, SIMD_PARA_MD5)
#else
#define MD5_N_STR			PARA_TO_N(SIMD_COEF_32)
#endif
#if SIMD_PARA_SHA1 > 1
#define SHA1_N_STR			PARA_TO_MxN(SIMD_COEF_32, SIMD_PARA_SHA1)
#else
#define SHA1_N_STR			PARA_TO_N(SIMD_COEF_32)
#endif
#if SIMD_PARA_SHA256 > 1
#define SHA256_N_STR		PARA_TO_MxN(SIMD_COEF_32, SIMD_PARA_SHA256)
#else
#define SHA256_N_STR		PARA_TO_N(SIMD_COEF_32)
#endif
#if SIMD_PARA_SHA512 > 1
#define SHA512_N_STR		PARA_TO_MxN(SIMD_COEF_64, SIMD_PARA_SHA512)
#else
#define SHA512_N_STR		PARA_TO_N(SIMD_COEF_64)
#endif

#endif /* JOHN_DISABLE_INTRINSICS */

#define SHA_BUF_SIZ			16

#define NT_SSE2

#endif
