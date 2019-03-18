/*
 * This file is part of John the Ripper password cracker,
 * Copyright (c) 2010 by Solar Designer
 * Copyright (c) 2011, 2012 by magnum
 *
 * Use of Bartavelle's mmx/sse2/intrinsics and reduced binary size by
 * magnum in 2011-2012.
 *
 * OMP added May 2013, JimF
 */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_rawMD4;
#elif FMT_REGISTERS_H
john_register_one(&fmt_rawMD4);
#else

#include <string.h>

#include "arch.h"

#include "md4.h"
#include "common.h"
#include "johnswap.h"
#include "formats.h"

#if !FAST_FORMATS_OMP
#undef _OPENMP
#endif

//#undef SIMD_COEF_32
//#undef SIMD_PARA_MD4

/*
 * Only effective for SIMD.
 * Undef to disable reversing steps for benchmarking.
 */
#define REVERSE_STEPS

#ifdef _OPENMP
#ifdef SIMD_COEF_32
#ifndef OMP_SCALE
#define OMP_SCALE               1024
#endif
#else
#ifndef OMP_SCALE
#define OMP_SCALE				2048
#endif
#endif
#include <omp.h>
#endif
#include "simd-intrinsics.h"
#include "memdbg.h"

#define FORMAT_LABEL			"Raw-MD4"
#define FORMAT_NAME			""
#define ALGORITHM_NAME			"MD4 " MD4_ALGORITHM_NAME

#ifdef SIMD_COEF_32
#define NBKEYS				(SIMD_COEF_32 * SIMD_PARA_MD4)
#endif

#define BENCHMARK_COMMENT		""
#define BENCHMARK_LENGTH		-1
#ifndef MD4_BUF_SIZ
#define MD4_BUF_SIZ				16
#endif

#define CIPHERTEXT_LENGTH		32

#define DIGEST_SIZE				16
#define BINARY_SIZE				DIGEST_SIZE
#define BINARY_ALIGN			4
#define SALT_SIZE				0
#define SALT_ALIGN				1

#define FORMAT_TAG				"$MD4$"
#define TAG_LENGTH				(sizeof(FORMAT_TAG) - 1)

static struct fmt_tests tests[] = {
	{"8a9d093f14f8701df17732b2bb182c74", "password"},
	{FORMAT_TAG "6d78785c44ea8dfa178748b245d8c3ae", "magnum" },
	{"6d78785c44ea8dfa178748b245d8c3ae", "magnum" },
	{"6D78785C44EA8DFA178748B245D8C3AE", "magnum" },
	{FORMAT_TAG "31d6cfe0d16ae931b73c59d7e0c089c0", "" },
	{FORMAT_TAG "934eb897904769085af8101ad9dabca2", "John the ripper" },
	{FORMAT_TAG "cafbb81fb64d9dd286bc851c4c6e0d21", "lolcode" },
	{FORMAT_TAG "585028aa0f794af812ee3be8804eb14a", "123456" },
	{FORMAT_TAG "23580e2a459f7ea40f9efa148b63cafb", "12345" },
	{FORMAT_TAG "2ae523785d0caf4d2fb557c12016185c", "123456789" },
	{FORMAT_TAG "f3e80e83b29b778bc092bf8a7c6907fe", "iloveyou" },
	{FORMAT_TAG "4d10a268a303379f224d8852f2d13f11", "princess" },
	{FORMAT_TAG "bf75555ca19051f694224f2f5e0b219d", "1234567" },
	{FORMAT_TAG "41f92cf74e3d2c3ba79183629a929915", "rockyou" },
	{FORMAT_TAG "012d73e0fab8d26e0f4d65e36077511e", "12345678" },
	{FORMAT_TAG "0ceb1fd260c35bd50005341532748de6", "abc123" },
	{NULL}
};

#ifdef SIMD_COEF_32
#define PLAINTEXT_LENGTH		55
#define MIN_KEYS_PER_CRYPT		NBKEYS
#define MAX_KEYS_PER_CRYPT		NBKEYS
#define GETPOS(i, index)		( (index&(SIMD_COEF_32-1))*4 + ((i)&(0xffffffff-3))*SIMD_COEF_32 + ((i)&3) + (unsigned int)index/SIMD_COEF_32*MD4_BUF_SIZ*4*SIMD_COEF_32 )
#else
#define PLAINTEXT_LENGTH		125
#define MIN_KEYS_PER_CRYPT		1
#define MAX_KEYS_PER_CRYPT		1
#endif

#ifdef SIMD_COEF_32
static uint32_t (*saved_key)[MD4_BUF_SIZ*NBKEYS];
static uint32_t (*crypt_key)[DIGEST_SIZE/4*NBKEYS];
#else
static int (*saved_len);
static char (*saved_key)[PLAINTEXT_LENGTH + 1];
static uint32_t (*crypt_key)[4];
#endif

static void init(struct fmt_main *self)
{
#ifdef _OPENMP
	int omp_t = omp_get_max_threads();
	self->params.min_keys_per_crypt *= omp_t;
	omp_t *= OMP_SCALE;
	self->params.max_keys_per_crypt *= omp_t;
#endif
#ifndef SIMD_COEF_32
	saved_len = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*saved_len));
	saved_key = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*saved_key));
	crypt_key = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*crypt_key));
#else
	saved_key = mem_calloc_align(self->params.max_keys_per_crypt/NBKEYS,
	                             sizeof(*saved_key), MEM_ALIGN_SIMD);
	crypt_key = mem_calloc_align(self->params.max_keys_per_crypt/NBKEYS,
	                             sizeof(*crypt_key), MEM_ALIGN_SIMD);
#endif
}

static void done(void)
{
	MEM_FREE(crypt_key);
	MEM_FREE(saved_key);
#ifndef SIMD_COEF_32
	MEM_FREE(saved_len);
#endif
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *p, *q;

	p = ciphertext;
	if (!strncmp(p, FORMAT_TAG, TAG_LENGTH))
		p += TAG_LENGTH;

	q = p;
	while (atoi16[ARCH_INDEX(*q)] != 0x7F)
		q++;
	return !*q && q - p == CIPHERTEXT_LENGTH;
}

static char *split(char *ciphertext, int index, struct fmt_main *self)
{
	static char out[TAG_LENGTH + CIPHERTEXT_LENGTH + 1];

	if (ciphertext[0] == '$' &&
			!strncmp(ciphertext, FORMAT_TAG, TAG_LENGTH))
		ciphertext += TAG_LENGTH;

	memcpy(out, FORMAT_TAG, TAG_LENGTH);
	memcpy(out + TAG_LENGTH, ciphertext, CIPHERTEXT_LENGTH + 1);
	strlwr(&out[TAG_LENGTH]);
	return out;
}

static void *get_binary(char *ciphertext)
{
	static union {
		unsigned long dummy;
		unsigned int i[DIGEST_SIZE/sizeof(unsigned int)];
	} _out;
	unsigned int *out = _out.i;
	unsigned int i;
	unsigned int temp;

	ciphertext += TAG_LENGTH;
	for (i=0; i<4; i++)
	{
		temp  = ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+0])]))<<4;
		temp |= ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+1])]));

		temp |= ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+2])]))<<12;
		temp |= ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+3])]))<<8;

		temp |= ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+4])]))<<20;
		temp |= ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+5])]))<<16;

		temp |= ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+6])]))<<28;
		temp |= ((unsigned int)(atoi16[ARCH_INDEX(ciphertext[i*8+7])]))<<24;

#if ARCH_LITTLE_ENDIAN
		out[i]=temp;
#else
		out[i]=JOHNSWAP(temp);
#endif
	}

#if SIMD_COEF_32 && defined(REVERSE_STEPS)
	md4_reverse(out);
#endif

	return out;
}

static char *source(char *source, void *binary)
{
	static char out[TAG_LENGTH + CIPHERTEXT_LENGTH + 1] = FORMAT_TAG;
	uint32_t b[4];
	char *p;
	int i, j;

	memcpy(b, binary, sizeof(b));

#if SIMD_COEF_32 && defined(REVERSE_STEPS)
	md4_unreverse(b);
#endif

#if ARCH_LITTLE_ENDIAN==0
	alter_endianity(b, 16);
#endif

	p = &out[TAG_LENGTH];
	for (i = 0; i < 4; i++)
		for (j = 0; j < 8; j++)
			*p++ = itoa16[(b[i] >> ((j ^ 1) * 4)) & 0xf];

	return out;
}

#ifdef SIMD_COEF_32
static void set_key(char *_key, int index)
{
#if ARCH_ALLOWS_UNALIGNED
	const uint32_t *key = (uint32_t*)_key;
#else
	char buf_aligned[PLAINTEXT_LENGTH + 1] JTR_ALIGN(sizeof(uint32_t));
	const uint32_t *key = (uint32_t*)(is_aligned(_key, sizeof(uint32_t)) ?
	                                      _key : strcpy(buf_aligned, _key));
#endif
	uint32_t *keybuffer = &((uint32_t*)saved_key)[(index&(SIMD_COEF_32-1)) + (unsigned int)index/SIMD_COEF_32*MD4_BUF_SIZ*SIMD_COEF_32];
	uint32_t *keybuf_word = keybuffer;
	unsigned int len;
	uint32_t temp;

	len = 0;
	while((temp = *key++) & 0xff) {
		if (!(temp & 0xff00))
		{
			*keybuf_word = (temp & 0xff) | (0x80 << 8);
			len++;
			goto key_cleaning;
		}
		if (!(temp & 0xff0000))
		{
			*keybuf_word = (temp & 0xffff) | (0x80 << 16);
			len+=2;
			goto key_cleaning;
		}
		if (!(temp & 0xff000000))
		{
			*keybuf_word = temp | (0x80U << 24);
			len+=3;
			goto key_cleaning;
		}
		*keybuf_word = temp;
		len += 4;
		keybuf_word += SIMD_COEF_32;
	}
	*keybuf_word = 0x80;

key_cleaning:
	keybuf_word += SIMD_COEF_32;
	while(*keybuf_word) {
		*keybuf_word = 0;
		keybuf_word += SIMD_COEF_32;
	}
	keybuffer[14*SIMD_COEF_32] = len << 3;
}
#else
static void set_key(char *key, int index)
{
	int len = strlen(key);
	saved_len[index] = len;
	memcpy(saved_key[index], key, len);
}
#endif

#ifdef SIMD_COEF_32
static char *get_key(int index)
{
	static char out[PLAINTEXT_LENGTH + 1];
	unsigned int i;
	uint32_t len = ((uint32_t*)saved_key)[14*SIMD_COEF_32 + (index&(SIMD_COEF_32-1)) + (unsigned int)index/SIMD_COEF_32*MD4_BUF_SIZ*SIMD_COEF_32] >> 3;

	for (i=0;i<len;i++)
		out[i] = ((char*)saved_key)[GETPOS(i, index)];
	out[i] = 0;
	return (char*)out;
}
#else
static char *get_key(int index)
{
	saved_key[index][saved_len[index]] = 0;
	return saved_key[index];
}
#endif

#ifndef REVERSE_STEPS
#undef SSEi_REVERSE_STEPS
#define SSEi_REVERSE_STEPS 0
#endif

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index = 0;

#ifdef _OPENMP
	int loops = (count + MAX_KEYS_PER_CRYPT - 1) / MAX_KEYS_PER_CRYPT;

#pragma omp parallel for
	for (index = 0; index < loops; index++)
#endif
	{
#if SIMD_COEF_32
		SIMDmd4body(saved_key[index], crypt_key[index], NULL, SSEi_REVERSE_STEPS | SSEi_MIXED_IN);
#else
		MD4_CTX ctx;
		MD4_Init(&ctx);
		MD4_Update(&ctx, saved_key[index], saved_len[index]);
		MD4_Final((unsigned char *)crypt_key[index], &ctx);
#endif
	}
	return count;
}

static int cmp_all(void *binary, int count) {
#ifdef SIMD_COEF_32
	unsigned int x, y;
#ifdef _OPENMP
	const unsigned int c = (count + SIMD_COEF_32 - 1) / SIMD_COEF_32;
#else
	const unsigned int c = SIMD_PARA_MD4;
#endif
	for (y = 0; y < c; y++)
		for (x = 0; x < SIMD_COEF_32; x++)
		{
			if ( ((uint32_t*)binary)[1] == ((uint32_t*)crypt_key)[y*SIMD_COEF_32*4+x+SIMD_COEF_32] )
				return 1;
		}
	return 0;
#else
	unsigned int index = 0;

#ifdef _OPENMP
	for (index = 0; index < count; index++)
#endif
		if (!memcmp(binary, crypt_key[index], BINARY_SIZE))
			return 1;
	return 0;
#endif
}

static int cmp_one(void *binary, int index)
{
#ifdef SIMD_COEF_32
	unsigned int x = index&(SIMD_COEF_32-1);
	unsigned int y = (unsigned int)index/SIMD_COEF_32;

	return ((uint32_t*)binary)[1] == ((uint32_t*)crypt_key)[x+y*SIMD_COEF_32*4+SIMD_COEF_32];
#else
	return !memcmp(binary, crypt_key, BINARY_SIZE);
#endif
}

static int cmp_exact(char *source, int index)
{
#ifdef SIMD_COEF_32
	uint32_t crypt_key[DIGEST_SIZE / 4];
	MD4_CTX ctx;
	char *key = get_key(index);

	MD4_Init(&ctx);
	MD4_Update(&ctx, key, strlen(key));
	MD4_Final((void*)crypt_key, &ctx);

#ifdef REVERSE_STEPS
	md4_reverse(crypt_key);
#endif
	return !memcmp(get_binary(source), crypt_key, DIGEST_SIZE);
#else
	return 1;
#endif
}

#ifdef SIMD_COEF_32
#define SIMD_INDEX (index&(SIMD_COEF_32-1))+(unsigned int)index/SIMD_COEF_32*SIMD_COEF_32*4+SIMD_COEF_32
static int get_hash_0(int index) { return ((uint32_t*)crypt_key)[SIMD_INDEX] & PH_MASK_0; }
static int get_hash_1(int index) { return ((uint32_t*)crypt_key)[SIMD_INDEX] & PH_MASK_1; }
static int get_hash_2(int index) { return ((uint32_t*)crypt_key)[SIMD_INDEX] & PH_MASK_2; }
static int get_hash_3(int index) { return ((uint32_t*)crypt_key)[SIMD_INDEX] & PH_MASK_3; }
static int get_hash_4(int index) { return ((uint32_t*)crypt_key)[SIMD_INDEX] & PH_MASK_4; }
static int get_hash_5(int index) { return ((uint32_t*)crypt_key)[SIMD_INDEX] & PH_MASK_5; }
static int get_hash_6(int index) { return ((uint32_t*)crypt_key)[SIMD_INDEX] & PH_MASK_6; }
#else
static int get_hash_0(int index) { return ((uint32_t*)crypt_key)[1] & PH_MASK_0; }
static int get_hash_1(int index) { return ((uint32_t*)crypt_key)[1] & PH_MASK_1; }
static int get_hash_2(int index) { return ((uint32_t*)crypt_key)[1] & PH_MASK_2; }
static int get_hash_3(int index) { return ((uint32_t*)crypt_key)[1] & PH_MASK_3; }
static int get_hash_4(int index) { return ((uint32_t*)crypt_key)[1] & PH_MASK_4; }
static int get_hash_5(int index) { return ((uint32_t*)crypt_key)[1] & PH_MASK_5; }
static int get_hash_6(int index) { return ((uint32_t*)crypt_key)[1] & PH_MASK_6; }
#endif

static int binary_hash_0(void * binary) { return ((uint32_t*)binary)[1] & PH_MASK_0; }
static int binary_hash_1(void * binary) { return ((uint32_t*)binary)[1] & PH_MASK_1; }
static int binary_hash_2(void * binary) { return ((uint32_t*)binary)[1] & PH_MASK_2; }
static int binary_hash_3(void * binary) { return ((uint32_t*)binary)[1] & PH_MASK_3; }
static int binary_hash_4(void * binary) { return ((uint32_t*)binary)[1] & PH_MASK_4; }
static int binary_hash_5(void * binary) { return ((uint32_t*)binary)[1] & PH_MASK_5; }
static int binary_hash_6(void * binary) { return ((uint32_t*)binary)[1] & PH_MASK_6; }

struct fmt_main fmt_rawMD4 = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		0,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		BINARY_ALIGN,
		SALT_SIZE,
		SALT_ALIGN,
		MIN_KEYS_PER_CRYPT,
		MAX_KEYS_PER_CRYPT,
#ifdef _OPENMP
		FMT_OMP | FMT_OMP_BAD |
#endif
		FMT_CASE | FMT_8_BIT | FMT_SPLIT_UNIFIES_CASE,
		{ NULL },
		{ FORMAT_TAG },
		tests
	}, {
		init,
		done,
		fmt_default_reset,
		fmt_default_prepare,
		valid,
		split,
		get_binary,
		fmt_default_salt,
		{ NULL },
		source,
		{
			binary_hash_0,
			binary_hash_1,
			binary_hash_2,
			binary_hash_3,
			binary_hash_4,
			binary_hash_5,
			binary_hash_6
		},
		fmt_default_salt_hash,
		NULL,
		fmt_default_set_salt,
		set_key,
		get_key,
		fmt_default_clear_keys,
		crypt_all,
		{
			get_hash_0,
			get_hash_1,
			get_hash_2,
			get_hash_3,
			get_hash_4,
			get_hash_5,
			get_hash_6
		},
		cmp_all,
		cmp_one,
		cmp_exact
	}
};

#endif /* plugin stanza */
