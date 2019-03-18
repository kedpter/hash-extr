/*
 * Format for brute-forcing PGP Virtual Disk images.
 *
 * This software is Copyright (c) 2017 Dhiru Kholia <dhiru at openwall.net> and
 * it is hereby released to the general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 */

#ifdef HAVE_OPENCL

#if FMT_EXTERNS_H
extern struct fmt_main fmt_opencl_pgpdisk;
#elif FMT_REGISTERS_H
john_register_one(&fmt_opencl_pgpdisk);
#else

#include <stdint.h>
#include <string.h>
#include <openssl/cast.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "arch.h"
#include "params.h"
#include "common.h"
#include "formats.h"
#include "misc.h"
#include "aes.h"
#include "twofish.h"
#include "sha.h"
#include "common-opencl.h"
#include "options.h"
#include "pgpdisk_common.h"

#define FORMAT_LABEL            "pgpdisk-opencl"
#define ALGORITHM_NAME          "SHA1 OpenCL"
#define BINARY_SIZE             16
#define BINARY_ALIGN            sizeof(uint32_t)
#define SALT_SIZE               sizeof(struct custom_salt)
#define SALT_ALIGN              sizeof(uint32_t)
#define PLAINTEXT_LENGTH        124
#define MIN_KEYS_PER_CRYPT      1
#define MAX_KEYS_PER_CRYPT      1
#define BENCHMARK_COMMENT       ""
#define BENCHMARK_LENGTH        -1001

typedef struct {
	uint32_t length;
	uint8_t v[PLAINTEXT_LENGTH];
} pgpdisk_password;

typedef struct {
	uint8_t v[32];
} pgpdisk_hash;

typedef struct {
	uint32_t saltlen;
	uint32_t iterations;
	uint32_t key_len;
	uint8_t salt[16];
} pgpdisk_salt;

static uint32_t (*crypt_out)[BINARY_SIZE * 2 / sizeof(uint32_t)];
static struct custom_salt *cur_salt;

static cl_int cl_error;
static pgpdisk_password *inbuffer;
static pgpdisk_hash *outbuffer;
static pgpdisk_salt currentsalt;
static cl_mem mem_in, mem_out, mem_setting;
static struct fmt_main *self;

size_t insize, outsize, settingsize;

// This file contains auto-tuning routine(s). Has to be included after formats definitions.
#include "opencl_autotune.h"
#include "memdbg.h"

static const char *warn[] = {
	"xfer: ",  ", crypt: ",  ", xfer: "
};

static size_t get_task_max_work_group_size()
{
	return autotune_get_task_max_work_group_size(FALSE, 0, crypt_kernel);
}

static void create_clobj(size_t gws, struct fmt_main *self)
{
	insize = sizeof(pgpdisk_password) * gws;
	outsize = sizeof(pgpdisk_hash) * gws;
	settingsize = sizeof(pgpdisk_salt);
	crypt_out = mem_calloc(gws, sizeof(*crypt_out));

	inbuffer = mem_calloc(1, insize);
	outbuffer = mem_alloc(outsize);

	// Allocate memory
	mem_in =
	    clCreateBuffer(context[gpu_id], CL_MEM_READ_ONLY, insize, NULL,
	    &cl_error);
	HANDLE_CLERROR(cl_error, "Error allocating mem in");
	mem_setting =
	    clCreateBuffer(context[gpu_id], CL_MEM_READ_ONLY, settingsize,
	    NULL, &cl_error);
	HANDLE_CLERROR(cl_error, "Error allocating mem setting");
	mem_out =
	    clCreateBuffer(context[gpu_id], CL_MEM_WRITE_ONLY, outsize, NULL,
	    &cl_error);
	HANDLE_CLERROR(cl_error, "Error allocating mem out");

	HANDLE_CLERROR(clSetKernelArg(crypt_kernel, 0, sizeof(mem_in),
		&mem_in), "Error while setting mem_in kernel argument");
	HANDLE_CLERROR(clSetKernelArg(crypt_kernel, 1, sizeof(mem_out),
		&mem_out), "Error while setting mem_out kernel argument");
	HANDLE_CLERROR(clSetKernelArg(crypt_kernel, 2, sizeof(mem_setting),
		&mem_setting), "Error while setting mem_salt kernel argument");
}

static void release_clobj(void)
{
	if (inbuffer) {
		HANDLE_CLERROR(clReleaseMemObject(mem_in), "Release mem in");
		HANDLE_CLERROR(clReleaseMemObject(mem_setting), "Release mem setting");
		HANDLE_CLERROR(clReleaseMemObject(mem_out), "Release mem out");

		MEM_FREE(inbuffer);
		MEM_FREE(outbuffer);
	}
}

static void init(struct fmt_main *_self)
{
	self = _self;
	opencl_prepare_dev(gpu_id);
	Twofish_initialise();
}

static void reset(struct db_main *db)
{
	if (!autotuned) {
		char build_opts[64];

		snprintf(build_opts, sizeof(build_opts),
		         "-DPLAINTEXT_LENGTH=%d",
		         PLAINTEXT_LENGTH);
		opencl_init("$JOHN/kernels/pgpdisk_kernel.cl",
		            gpu_id, build_opts);

		crypt_kernel = clCreateKernel(program[gpu_id], "pgpdisk", &cl_error);
		HANDLE_CLERROR(cl_error, "Error creating kernel");

		// Initialize openCL tuning (library) for this format.
		opencl_init_auto_setup(SEED, 0, NULL, warn, 1, self,
		                       create_clobj, release_clobj,
		                       sizeof(pgpdisk_password), 0, db);

		// Auto tune execution from shared/included code.
		autotune_run(self, 1, 0, 300);
	}
}

static void done(void)
{
	if (autotuned) {
		release_clobj();

		HANDLE_CLERROR(clReleaseKernel(crypt_kernel), "Release kernel");
		HANDLE_CLERROR(clReleaseProgram(program[gpu_id]), "Release Program");

		autotuned--;
	}
}

static void *get_binary(char *ciphertext)
{
	static union {
		unsigned char c[BINARY_SIZE];
		uint32_t dummy;
	} buf;
	unsigned char *out = buf.c;
	char *p;
	int i;
	p = strrchr(ciphertext, '*') + 1;
	for (i = 0; i < BINARY_SIZE; i++) {
		out[i] = (atoi16[ARCH_INDEX(*p)] << 4) | atoi16[ARCH_INDEX(p[1])];
		p += 2;
	}

	return out;
}

static void set_salt(void *salt)
{
	cur_salt = (struct custom_salt *)salt;

	currentsalt.iterations = cur_salt->iterations;
	if (cur_salt->algorithm == 3) {
		currentsalt.key_len = 16;
		currentsalt.saltlen= 8;
	} else {
		currentsalt.key_len = 32;
		currentsalt.saltlen = 16;
	}
	memcpy((char*)currentsalt.salt, cur_salt->salt, currentsalt.saltlen);

	HANDLE_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_setting,
		CL_FALSE, 0, settingsize, &currentsalt, 0, NULL, NULL),
	    "Copy setting to gpu");
}

#undef set_key
static void set_key(char *key, int index)
{
	uint32_t length = strlen(key);

	if (length > PLAINTEXT_LENGTH)
		length = PLAINTEXT_LENGTH;
	inbuffer[index].length = length;
	memcpy(inbuffer[index].v, key, length);
}

static char *get_key(int index)
{
	static char ret[PLAINTEXT_LENGTH + 1];
	uint32_t length = inbuffer[index].length;
	memcpy(ret, inbuffer[index].v, length);
	ret[length] = '\0';
	return ret;
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index = 0;
	size_t *lws = local_work_size ? &local_work_size : NULL;

	global_work_size = GET_MULTIPLE_OR_BIGGER(count, local_work_size);

	// Copy data to gpu
	BENCH_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_in, CL_FALSE, 0,
		insize, inbuffer, 0, NULL, multi_profilingEvent[0]),
		"Copy data to gpu");

	// Run kernel
	BENCH_CLERROR(clEnqueueNDRangeKernel(queue[gpu_id], crypt_kernel, 1,
		NULL, &global_work_size, lws, 0, NULL,
		multi_profilingEvent[1]),
		"Run kernel");

	// Read the result back
	BENCH_CLERROR(clEnqueueReadBuffer(queue[gpu_id], mem_out, CL_TRUE, 0,
		outsize, outbuffer, 0, NULL, multi_profilingEvent[2]),
		"Copy result back");

	if (ocl_autotune_running)
		return count;

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (index = 0; index < count; index++) {
		unsigned char key[40];
		memcpy(key, outbuffer[index].v, 32);

		if (cur_salt->algorithm == 5 || cur_salt->algorithm == 6 || cur_salt->algorithm == 7) {
			AES_KEY aes_key;

			AES_set_encrypt_key(key, 256, &aes_key);
			AES_ecb_encrypt(key, (unsigned char*)crypt_out[index], &aes_key, AES_ENCRYPT);
		} else if (cur_salt->algorithm == 4) {
			Twofish_key tkey;

			Twofish_prepare_key(key, 32, &tkey);
			Twofish_encrypt(&tkey, key, (unsigned char*)crypt_out[index]);
		} else if (cur_salt->algorithm == 3) {
			CAST_KEY ck;

			CAST_set_key(&ck, 16, key);
			memset((unsigned char*)crypt_out[index], 0, BINARY_SIZE);
			CAST_ecb_encrypt(key, (unsigned char*)crypt_out[index], &ck, CAST_ENCRYPT);
		}
	}

	return count;
}

static int cmp_all(void *binary, int count)
{
	int index = 0;
	for (; index < count; index++)
		if (!memcmp(binary, crypt_out[index], ARCH_SIZE))
			return 1;
	return 0;
}

static int cmp_one(void *binary, int index)
{
	return !memcmp(binary, crypt_out[index], BINARY_SIZE);
}

static int cmp_exact(char *source, int index)
{
	return 1;
}

struct fmt_main fmt_opencl_pgpdisk = {
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
		FMT_CASE | FMT_8_BIT | FMT_OMP,
		{ NULL },
		{ FORMAT_TAG },
		pgpdisk_tests,
	},
	{
		init,
		done,
		reset,
		fmt_default_prepare,
		pgpdisk_common_valid,
		fmt_default_split,
		get_binary,
		pgpdisk_common_get_salt,
		{
			0
		},
		fmt_default_source,
		{
			fmt_default_binary_hash
		},
		fmt_default_salt_hash,
		NULL,
		set_salt,
		set_key,
		get_key,
		fmt_default_clear_keys,
		crypt_all,
		{
			fmt_default_get_hash
		},
		cmp_all,
		cmp_one,
		cmp_exact
	}
};

#endif /* plugin stanza */

#endif /* HAVE_OPENCL */
