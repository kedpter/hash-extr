/*
 * This software is Copyright (c) 2012 Lukas Odzioba <ukasz at openwall dot net>
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * Code is based on  Aircrack-ng source
 *
 * SSE2 code enhancement, Jim Fougeron, Jan, 2013.
 *  Also removed oSSL EVP code and coded what it does (which is simple), inline.
 */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_wpapsk;
#elif FMT_REGISTERS_H
john_register_one(&fmt_wpapsk);
#else

#include <string.h>
#include <assert.h>

#include "arch.h"
#include "simd-intrinsics.h"
#include "formats.h"
#include "common.h"
#include "misc.h"
//#define WPAPSK_DEBUG
#include "wpapsk.h"
#include "sha.h"

// if this is uncommented, we will force building of SSE to be 'off'. It is
// useful in testing but 99.9% of the builds should have this undef commented out.
//#undef SIMD_COEF_32

#ifdef SIMD_COEF_32
  #define NBKEYS	(SIMD_COEF_32 * SIMD_PARA_SHA1)
  #ifdef _OPENMP
    #include <omp.h>
  #endif
#else
  #define NBKEYS	1
  #ifdef _OPENMP
    #include <omp.h>
  #endif
#endif

#include "memdbg.h"

#define FORMAT_LABEL		"wpapsk"
#if !HAVE_OPENSSL_CMAC_H
#ifdef _MSC_VER
#pragma message("Notice: WPAPSK (CPU) format built without support for 802.11w. Upgrade your OpenSSL.")
#else
#warning Notice: WPAPSK (CPU) format built without support for 802.11w. Upgrade your OpenSSL.
#endif
#define FORMAT_NAME		"WPA/WPA2 PSK"
#else
#define FORMAT_NAME		"WPA/WPA2/PMF PSK"
#endif
#define ALGORITHM_NAME		"PBKDF2-SHA1 " SHA1_ALGORITHM_NAME

#define MIN_KEYS_PER_CRYPT	NBKEYS
#define MAX_KEYS_PER_CRYPT	NBKEYS

extern wpapsk_password *inbuffer;
extern wpapsk_hash *outbuffer;
extern wpapsk_salt currentsalt;
extern hccap_t hccap;
extern mic_t *mic;

#ifdef SIMD_COEF_32
// Ok, now we have our MMX/SSE2/intr buffer.
// this version works properly for MMX, SSE2 (.S) and SSE2 intrinsic.
#define GETPOS(i, index)	( (index&(SIMD_COEF_32-1))*4 + ((i)&(0xffffffff-3) )*SIMD_COEF_32 + (3-((i)&3)) + (unsigned int)index/SIMD_COEF_32*SHA_BUF_SIZ*SIMD_COEF_32*4 ) //for endianity conversion
static unsigned char (*sse_hash1);
static unsigned char (*sse_crypt1);
static unsigned char (*sse_crypt2);
static unsigned char (*sse_crypt);
#endif

static void init(struct fmt_main *self)
{
#ifdef _OPENMP
	int omp_t = omp_get_max_threads();
	self->params.min_keys_per_crypt *= omp_t;
	self->params.max_keys_per_crypt *= omp_t;
#endif

	assert(sizeof(hccap_t) == HCCAP_SIZE);

	inbuffer = mem_alloc(sizeof(*inbuffer) *
	                     self->params.max_keys_per_crypt);
	outbuffer = mem_alloc(sizeof(*outbuffer) *
	                      self->params.max_keys_per_crypt);
	mic = mem_alloc(sizeof(*mic) *
	                self->params.max_keys_per_crypt);

#if defined (SIMD_COEF_32)
	sse_hash1 = mem_calloc_align(self->params.max_keys_per_crypt,
	                             SHA_BUF_SIZ * 4 * sizeof(*sse_hash1),
	                             MEM_ALIGN_SIMD);
	sse_crypt1 = mem_calloc_align(self->params.max_keys_per_crypt,
	                              20 * sizeof(*sse_crypt1), MEM_ALIGN_SIMD);
	sse_crypt2 = mem_calloc_align(self->params.max_keys_per_crypt,
	                              20 * sizeof(*sse_crypt2), MEM_ALIGN_SIMD);
	sse_crypt = mem_calloc_align(self->params.max_keys_per_crypt,
	                             20 * sizeof(*sse_crypt), MEM_ALIGN_SIMD);
	{
		int index;
		for (index = 0; index < self->params.max_keys_per_crypt; ++index) {
			// set the length of all hash1 SSE buffer to 64+20 * 8 bits. The 64 is for the ipad/opad,
			// the 20 is for the length of the SHA1 buffer that also gets into each crypt.
			// Works for SSE2i and SSE2
			((unsigned int *)sse_hash1)[15*SIMD_COEF_32 + (index&(SIMD_COEF_32-1)) + (unsigned int)index/SIMD_COEF_32*SHA_BUF_SIZ*SIMD_COEF_32] = (84<<3); // all encrypts are 64+20 bytes.
			sse_hash1[GETPOS(20,index)] = 0x80;
		}
	}
	// From this point on, we ONLY touch the first 20 bytes (* SIMD_COEF_32) of each buffer 'block'.  If !SHA_PARA', then only the first
	// block is written to after this, if there are more that one SHA_PARA, then the start of each para block will be updated inside the inner loop.
#endif

/*
 * Zeroize the lengths in case crypt_all() is called with some keys still
 * not set.  This may happen during self-tests.
 */
	{
		int i;
		for (i = 0; i < self->params.max_keys_per_crypt; i++)
			inbuffer[i].length = 0;
	}
}

static void done(void)
{
#ifdef SIMD_COEF_32
	MEM_FREE(sse_crypt);
	MEM_FREE(sse_crypt2);
	MEM_FREE(sse_crypt1);
	MEM_FREE(sse_hash1);
#endif
	MEM_FREE(mic);
	MEM_FREE(outbuffer);
	MEM_FREE(inbuffer);
}

#ifndef SIMD_COEF_32
static MAYBE_INLINE void wpapsk_cpu(int count,
    wpapsk_password * in, wpapsk_hash * out, wpapsk_salt * salt)
{
	int j;
	int slen = salt->length + 4;

#ifdef _OPENMP
#pragma omp parallel for default(none) private(j) shared(count, slen, salt, in, out)
#endif
	for (j = 0; j < count; j++) {
		int i, k;
		unsigned char essid[32 + 4];
		union {
			unsigned char c[64];
			uint32_t i[16];
		} buffer;
		union {
			unsigned char c[40];
			uint32_t i[10];
		} outbuf;
		SHA_CTX ctx_ipad;
		SHA_CTX ctx_opad;
		SHA_CTX sha1_ctx;

		memset(essid, 0, 32 + 4);
		memcpy(essid, salt->salt, salt->length);
		memset(buffer.c, 0, 64);
		memcpy(buffer.c, in[j].v, in[j].length);

		SHA1_Init(&ctx_ipad);
		SHA1_Init(&ctx_opad);

		for (i = 0; i < 16; i++)
			buffer.i[i] ^= 0x36363636;
		SHA1_Update(&ctx_ipad, buffer.c, 64);

		for (i = 0; i < 16; i++)
			buffer.i[i] ^= 0x6a6a6a6a;
		SHA1_Update(&ctx_opad, buffer.c, 64);

		essid[slen - 1] = 1;
		memcpy(&sha1_ctx, &ctx_ipad, sizeof(sha1_ctx));
		SHA1_Update(&sha1_ctx, essid, slen);
		SHA1_Final(outbuf.c, &sha1_ctx);
		memcpy(&sha1_ctx, &ctx_opad, sizeof(sha1_ctx));
		SHA1_Update(&sha1_ctx, outbuf.c, 20);
		SHA1_Final(outbuf.c, &sha1_ctx);
		memcpy(buffer.c, outbuf.c, 20);
		for (i = 1; i < 4096; i++) {
			memcpy(&sha1_ctx, &ctx_ipad, sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, buffer.c, 20);
			SHA1_Final(buffer.c, &sha1_ctx);
			memcpy(&sha1_ctx, &ctx_opad, sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, buffer.c, 20);
			SHA1_Final(buffer.c, &sha1_ctx);
			for (k = 0; k < 5; k++)
				outbuf.i[k] ^= buffer.i[k];
		}
		essid[slen - 1] = 2;

		memcpy(&sha1_ctx, &ctx_ipad, sizeof(sha1_ctx));
		SHA1_Update(&sha1_ctx, essid, slen);
		SHA1_Final(&outbuf.c[20], &sha1_ctx);
		memcpy(&sha1_ctx, &ctx_opad, sizeof(sha1_ctx));
		SHA1_Update(&sha1_ctx, &outbuf.c[20], 20);
		SHA1_Final(&outbuf.c[20], &sha1_ctx);

		memcpy(buffer.c, &outbuf.c[20], 20);

		for (i = 1; i < 4096; i++) {
			memcpy(&sha1_ctx, &ctx_ipad, sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, buffer.c, 20);
			SHA1_Final(buffer.c, &sha1_ctx);
			memcpy(&sha1_ctx, &ctx_opad, sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, buffer.c, 20);
			SHA1_Final(buffer.c, &sha1_ctx);
			for (k = 5; k < 8; k++)
				outbuf.i[k] ^= buffer.i[k - 5];
		}

		memcpy(&out[j], outbuf.c, 32);
	}
}
#else
static MAYBE_INLINE void wpapsk_sse(int count, wpapsk_password * in, wpapsk_hash * out, wpapsk_salt * salt)
{
	int t; // thread count
	int slen = salt->length + 4;
	int loops = (count+NBKEYS-1) / NBKEYS;

#ifdef _OPENMP
#pragma omp parallel for default(none) private(t) shared(count, slen, salt, in, out, loops, sse_crypt1, sse_crypt2, sse_hash1)
#endif
	for (t = 0; t < loops; t++) {
		unsigned int i, k, j;
		unsigned char essid[32 + 4];
		union {
			unsigned char c[64];
			uint32_t i[16];
		} buffer[NBKEYS];
		union {
			unsigned char c[40];
			uint32_t i[10];
		} outbuf[NBKEYS];
		SHA_CTX ctx_ipad[NBKEYS];
		SHA_CTX ctx_opad[NBKEYS];

		SHA_CTX sha1_ctx;
		unsigned int *i1, *i2, *o1;
		unsigned char *t_sse_crypt1, *t_sse_crypt2, *t_sse_hash1;

		// All pointers get their offset for this thread here. No further offsetting below.
		t_sse_crypt1 = &sse_crypt1[t * NBKEYS * 20];
		t_sse_crypt2 = &sse_crypt2[t * NBKEYS * 20];
		t_sse_hash1 = &sse_hash1[t * NBKEYS * SHA_BUF_SIZ * 4];
		i1 = (unsigned int*)t_sse_crypt1;
		i2 = (unsigned int*)t_sse_crypt2;
		o1 = (unsigned int*)t_sse_hash1;

		memset(essid, 0, 32 + 4);
		memcpy(essid, salt->salt, salt->length);

		for (j = 0; j < NBKEYS; ++j) {
			memcpy(buffer[j].c, in[t*NBKEYS+j].v, in[t*NBKEYS+j].length);
			memset(&buffer[j].c[in[t*NBKEYS+j].length], 0, 64-in[t*NBKEYS+j].length);
			SHA1_Init(&ctx_ipad[j]);
			SHA1_Init(&ctx_opad[j]);

			for (i = 0; i < 16; i++)
				buffer[j].i[i] ^= 0x36363636;
			SHA1_Update(&ctx_ipad[j], buffer[j].c, 64);

			for (i = 0; i < 16; i++)
				buffer[j].i[i] ^= 0x6a6a6a6a;
			SHA1_Update(&ctx_opad[j], buffer[j].c, 64);

			// we memcopy from flat into SIMD_COEF_32 output buffer's (our 'temp' ctx buffer).
			// This data will NOT need to be BE swapped (it already IS BE swapped).
			i1[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+0*SIMD_COEF_32] = ctx_ipad[j].h0;
			i1[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+1*SIMD_COEF_32] = ctx_ipad[j].h1;
			i1[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+2*SIMD_COEF_32] = ctx_ipad[j].h2;
			i1[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+3*SIMD_COEF_32] = ctx_ipad[j].h3;
			i1[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+4*SIMD_COEF_32] = ctx_ipad[j].h4;

			i2[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+0*SIMD_COEF_32] = ctx_opad[j].h0;
			i2[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+1*SIMD_COEF_32] = ctx_opad[j].h1;
			i2[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+2*SIMD_COEF_32] = ctx_opad[j].h2;
			i2[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+3*SIMD_COEF_32] = ctx_opad[j].h3;
			i2[(j/SIMD_COEF_32)*SIMD_COEF_32*5+(j&(SIMD_COEF_32-1))+4*SIMD_COEF_32] = ctx_opad[j].h4;

			essid[slen - 1] = 1;
			memcpy(&sha1_ctx, &ctx_ipad[j], sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, essid, slen);
			SHA1_Final(outbuf[j].c, &sha1_ctx);
			memcpy(&sha1_ctx, &ctx_opad[j], sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, outbuf[j].c, SHA_DIGEST_LENGTH);
			SHA1_Final(outbuf[j].c, &sha1_ctx);
//			memcpy(buffer[j].c, &outbuf[j], 20);

			// now convert this from flat into SIMD_COEF_32 buffers.   (same as the memcpy() commented out in the last line)
			// Also, perform the 'first' ^= into the crypt buffer.  NOTE, we are doing that in BE format
			// so we will need to 'undo' that in the end.
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+0*SIMD_COEF_32] = outbuf[j].i[0] = sha1_ctx.h0;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+1*SIMD_COEF_32] = outbuf[j].i[1] = sha1_ctx.h1;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+2*SIMD_COEF_32] = outbuf[j].i[2] = sha1_ctx.h2;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+3*SIMD_COEF_32] = outbuf[j].i[3] = sha1_ctx.h3;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+4*SIMD_COEF_32] = outbuf[j].i[4] = sha1_ctx.h4;
		}

		for (i = 1; i < 4096; i++) {
			SIMDSHA1body((unsigned int*)t_sse_hash1, (unsigned int*)t_sse_hash1, (unsigned int*)t_sse_crypt1, SSEi_MIXED_IN|SSEi_RELOAD|SSEi_OUTPUT_AS_INP_FMT);
			SIMDSHA1body((unsigned int*)t_sse_hash1, (unsigned int*)t_sse_hash1, (unsigned int*)t_sse_crypt2, SSEi_MIXED_IN|SSEi_RELOAD|SSEi_OUTPUT_AS_INP_FMT);
			for (j = 0; j < NBKEYS; j++) {
				unsigned *p = &((unsigned int*)t_sse_hash1)[(((j/SIMD_COEF_32)*SHA_BUF_SIZ)*SIMD_COEF_32) + (j&(SIMD_COEF_32-1))];
				for (k = 0; k < 5; k++)
					outbuf[j].i[k] ^= p[(k*SIMD_COEF_32)];
			}
		}
		essid[slen - 1] = 2;

		for (j = 0; j < NBKEYS; ++j) {
			memcpy(&sha1_ctx, &ctx_ipad[j], sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, essid, slen);
			SHA1_Final(&outbuf[j].c[20], &sha1_ctx);
			memcpy(&sha1_ctx, &ctx_opad[j], sizeof(sha1_ctx));
			SHA1_Update(&sha1_ctx, &outbuf[j].c[20], 20);
			SHA1_Final(&outbuf[j].c[20], &sha1_ctx);
//			memcpy(&buffer[j], &outbuf[j].c[20], 20);

			// now convert this from flat into SIMD_COEF_32 buffers.  (same as the memcpy() commented out in the last line)
			// Also, perform the 'first' ^= into the crypt buffer.  NOTE, we are doing that in BE format
			// so we will need to 'undo' that in the end. (only 3 dwords of the 2nd block outbuf are worked with).
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+0*SIMD_COEF_32] = outbuf[j].i[5] = sha1_ctx.h0;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+1*SIMD_COEF_32] = outbuf[j].i[6] = sha1_ctx.h1;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+2*SIMD_COEF_32] = outbuf[j].i[7] = sha1_ctx.h2;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+3*SIMD_COEF_32] = sha1_ctx.h3;
			o1[(j/SIMD_COEF_32)*SIMD_COEF_32*SHA_BUF_SIZ+(j&(SIMD_COEF_32-1))+4*SIMD_COEF_32] = sha1_ctx.h4;
		}
		for (i = 1; i < 4096; i++) {
			SIMDSHA1body((unsigned int*)t_sse_hash1, (unsigned int*)t_sse_hash1, (unsigned int*)t_sse_crypt1, SSEi_MIXED_IN|SSEi_RELOAD|SSEi_OUTPUT_AS_INP_FMT);
			SIMDSHA1body((unsigned int*)t_sse_hash1, (unsigned int*)t_sse_hash1, (unsigned int*)t_sse_crypt2, SSEi_MIXED_IN|SSEi_RELOAD|SSEi_OUTPUT_AS_INP_FMT);
			for (j = 0; j < NBKEYS; j++) {
				unsigned *p = &((unsigned int*)t_sse_hash1)[(((j/SIMD_COEF_32)*SHA_BUF_SIZ)*SIMD_COEF_32) + (j&(SIMD_COEF_32-1))];
				for (k = 5; k < 8; k++)
					outbuf[j].i[k] ^= p[((k-5)*SIMD_COEF_32)];
			}
		}

		for (j = 0; j < NBKEYS; ++j) {
			// the BE() convert should be done in binary, BUT since we use 'common' code for
			// get_binary(), which is shared between CPU and OpenCL, we have to do it here.
			memcpy(out[t*NBKEYS+j].v, outbuf[j].c, 32);
			alter_endianity_to_BE(out[t*NBKEYS+j].v,8);
		}
	}
}
#endif

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	extern volatile int bench_running;

	if (new_keys || strcmp(last_ssid, hccap.essid) || bench_running) {
#ifndef SIMD_COEF_32
		wpapsk_cpu(count, inbuffer, outbuffer, &currentsalt);
#else
		wpapsk_sse(count, inbuffer, outbuffer, &currentsalt);
#endif
		new_keys = 0;
		strcpy(last_ssid, hccap.essid);
	}

	wpapsk_postprocess(count);

	return count;
}

struct fmt_main fmt_wpapsk = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		8,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		BINARY_ALIGN,
		SALT_SIZE,
		SALT_ALIGN,
		MIN_KEYS_PER_CRYPT,
		MAX_KEYS_PER_CRYPT,
		FMT_CASE | FMT_OMP,
		{
#if HAVE_OPENSSL_CMAC_H
			"key version [1:WPA 2:WPA2 3:802.11w]"
#else
			"key version [1:WPA 2:WPA2]"
#endif
		},
		{ FORMAT_TAG },
		tests
	},
	{
		init,
		done,
		fmt_default_reset,
		fmt_default_prepare,
		valid,
		fmt_default_split,
		get_binary,
		get_salt,
		{
			get_keyver,
		},
		fmt_default_source,
		{
			binary_hash_0,
			fmt_default_binary_hash_1,
			fmt_default_binary_hash_2,
			fmt_default_binary_hash_3,
			fmt_default_binary_hash_4,
			fmt_default_binary_hash_5,
			fmt_default_binary_hash_6
		},
		fmt_default_salt_hash,
		salt_compare,
		set_salt,
		set_key,
		get_key,
		clear_keys,
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
