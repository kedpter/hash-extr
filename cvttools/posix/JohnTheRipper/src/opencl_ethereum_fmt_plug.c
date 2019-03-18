/*
 * This software is Copyright (c) 2017 Dhiru Kholia <kholia at kth.se> and
 * Copyright (c) 2013 Lukas Odzioba <ukasz at openwall dot net> and it is
 * hereby released to the general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 */
#ifdef HAVE_OPENCL

#if FMT_EXTERNS_H
extern struct fmt_main fmt_opencl_ethereum;
#elif FMT_REGISTERS_H
john_register_one(&fmt_opencl_ethereum);
#else

#include <string.h>

#include "misc.h"
#include "arch.h"
#include "ethereum_common.h"
#include "common.h"
#include "formats.h"
#include "options.h"
#include "common-opencl.h"
#include "KeccakHash.h"

#define FORMAT_NAME             "Ethereum Wallet"
#define FORMAT_LABEL            "ethereum-opencl"
#define ALGORITHM_NAME          "PBKDF2-SHA256 OpenCL AES"
#define BENCHMARK_COMMENT       ""
#define BENCHMARK_LENGTH        -1001
#define MIN_KEYS_PER_CRYPT      1
#define MAX_KEYS_PER_CRYPT      1
#define BINARY_ALIGN            sizeof(uint32_t)
#define SALT_SIZE               sizeof(*cur_salt)
#define SALT_ALIGN              sizeof(int)
#define PLAINTEXT_LENGTH        55
#define KERNEL_NAME             "pbkdf2_sha256_kernel"
#define SPLIT_KERNEL_NAME       "pbkdf2_sha256_loop"

#define HASH_LOOPS              (13*71) // factors 13, 13, 71
#define ITERATIONS              12000

struct fmt_tests opencl_ethereum_tests[] = {
        // https://github.com/ethereum/wiki/wiki/Web3-Secret-Storage-Definition, v3 wallets
        {"$ethereum$p*262144*ae3cd4e7013836a3df6bd7241b12db061dbe2c6785853cce422d148a624ce0bd*5318b4d5bcd28de64ee5559e671353e16f075ecae9f99c7a79a38af5f869aa46*517ead924a9d0dc3124507e3393d175ce3ff7c1e96529c6c555ce9e51205e9b2", "testpassword"},
	// artificial hash for testing
	{"$ethereum$p*1024*30323330313838333730333538343831*6dcfca7cbd44eb3ca7f11162b996b98dd46fb68e1afb095686fe944fcbdb3b59*b3a766d8c1390462304af979c6f709ba54a23ff135d1ffcdef485dcc7f79b5f2", "6023"},
	{NULL}
};

typedef struct {
	uint8_t length;
	uint8_t v[PLAINTEXT_LENGTH];
} pass_t;

typedef struct {
	uint32_t hash[8];
} crack_t;

typedef struct {
	uint8_t length;
	uint8_t salt[115];
	uint32_t rounds;
} salt_t;

typedef struct {
	uint32_t ipad[8];
	uint32_t opad[8];
	uint32_t hash[8];
	uint32_t W[8];
	uint32_t rounds;
} state_t;

static pass_t *host_pass;			      /** plain ciphertexts **/
static salt_t *host_salt;			      /** salt **/
static crack_t *host_crack;			      /** hash**/
static cl_int cl_error;
static cl_mem mem_in, mem_out, mem_salt, mem_state;
static cl_kernel split_kernel;
static struct fmt_main *self;

static custom_salt *cur_salt;
static uint32_t (*crypt_out)[BINARY_SIZE * 2 / sizeof(uint32_t)];

#define STEP			0
#define SEED			1024

static const char * warn[] = {
        "xfer: ",  ", init: " , ", crypt: ", ", res xfer: "
};

static int split_events[] = { 2, -1, -1 };

// This file contains auto-tuning routine(s). Has to be included after formats definitions.
#include "opencl_autotune.h"
#include "memdbg.h"

static void create_clobj(size_t kpc, struct fmt_main *self)
{
#define CL_RO CL_MEM_READ_ONLY
#define CL_WO CL_MEM_WRITE_ONLY
#define CL_RW CL_MEM_READ_WRITE

#define CLCREATEBUFFER(_flags, _size, _string)\
	clCreateBuffer(context[gpu_id], _flags, _size, NULL, &cl_error);\
	HANDLE_CLERROR(cl_error, _string);

#define CLKERNELARG(kernel, id, arg, msg)\
	HANDLE_CLERROR(clSetKernelArg(kernel, id, sizeof(arg), &arg), msg);

	host_pass = mem_calloc(kpc, sizeof(pass_t));
	host_crack = mem_calloc(kpc, sizeof(crack_t));
	crypt_out = mem_calloc(kpc, sizeof(*crypt_out));
	host_salt = mem_calloc(1, sizeof(salt_t));

	mem_in = CLCREATEBUFFER(CL_RO, kpc * sizeof(pass_t),
	                        "Cannot allocate mem in");
	mem_salt = CLCREATEBUFFER(CL_RO, sizeof(salt_t),
	                          "Cannot allocate mem salt");
	mem_out = CLCREATEBUFFER(CL_WO, kpc * sizeof(crack_t),
	                         "Cannot allocate mem out");
	mem_state = CLCREATEBUFFER(CL_RW, kpc * sizeof(state_t),
	                           "Cannot allocate mem state");

	CLKERNELARG(crypt_kernel, 0, mem_in, "Error while setting mem_in");
	CLKERNELARG(crypt_kernel, 1, mem_salt, "Error while setting mem_salt");
	CLKERNELARG(crypt_kernel, 2, mem_state, "Error while setting mem_state");

	CLKERNELARG(split_kernel, 0, mem_state, "Error while setting mem_state");
	CLKERNELARG(split_kernel, 1 ,mem_out, "Error while setting mem_out");
}

/* ------- Helper functions ------- */
static size_t get_task_max_work_group_size()
{
	size_t s;

	s = autotune_get_task_max_work_group_size(FALSE, 0, crypt_kernel);
	s = MIN(s, autotune_get_task_max_work_group_size(FALSE, 0, split_kernel));
	return s;
}

static void release_clobj(void)
{
	if (host_crack) {
		HANDLE_CLERROR(clReleaseMemObject(mem_in), "Release mem in");
		HANDLE_CLERROR(clReleaseMemObject(mem_salt), "Release mem salt");
		HANDLE_CLERROR(clReleaseMemObject(mem_out), "Release mem out");
		HANDLE_CLERROR(clReleaseMemObject(mem_state), "Release mem state");

		MEM_FREE(host_pass);
		MEM_FREE(host_salt);
		MEM_FREE(host_crack);
		MEM_FREE(crypt_out);
	}
}

static void init(struct fmt_main *_self)
{
	self = _self;
	opencl_prepare_dev(gpu_id);
}

static void reset(struct db_main *db)
{
	if (!autotuned) {
		char build_opts[64];

		snprintf(build_opts, sizeof(build_opts),
		         "-DHASH_LOOPS=%u -DPLAINTEXT_LENGTH=%u",
		         HASH_LOOPS, PLAINTEXT_LENGTH);
		opencl_init("$JOHN/kernels/pbkdf2_hmac_sha256_kernel.cl",
		            gpu_id, build_opts);

		crypt_kernel =
			clCreateKernel(program[gpu_id], KERNEL_NAME, &cl_error);
		HANDLE_CLERROR(cl_error, "Error creating crypt kernel");

		split_kernel =
			clCreateKernel(program[gpu_id], SPLIT_KERNEL_NAME, &cl_error);
		HANDLE_CLERROR(cl_error, "Error creating split kernel");

		// Initialize openCL tuning (library) for this format.
		opencl_init_auto_setup(SEED, HASH_LOOPS, split_events, warn,
		                       2, self, create_clobj, release_clobj,
		                       sizeof(state_t), 0, db);

		// Auto tune execution from shared/included code.
		autotune_run(self, ITERATIONS, 0,
		             (cpu(device_info[gpu_id]) ?
		              1000000000 : 10000000000ULL));
	}
}

static void done(void)
{
	if (autotuned) {
		release_clobj();
		HANDLE_CLERROR(clReleaseKernel(crypt_kernel), "Release kernel 1");
		HANDLE_CLERROR(clReleaseKernel(split_kernel), "Release kernel 2");
		HANDLE_CLERROR(clReleaseProgram(program[gpu_id]),
		               "Release Program");

		autotuned--;
	}
}

static int ethereum_valid(char *ciphertext, struct fmt_main *self)
{
	char *ctcopy, *keeptr, *p;
	int extra;

	if (strncmp(ciphertext, FORMAT_TAG, TAG_LENGTH) != 0)
		return 0;

	ctcopy = strdup(ciphertext);
	keeptr = ctcopy;

	ctcopy += TAG_LENGTH;
	if ((p = strtokm(ctcopy, "*")) == NULL) // type
		goto err;
	if (*p != 'p')
		goto err;
	if (*p == 'p') {
		if ((p = strtokm(NULL, "*")) == NULL)   // iterations
			goto err;
		if (!isdec(p))
			goto err;
		if ((p = strtokm(NULL, "*")) == NULL)   // salt
			goto err;
		if (hexlenl(p, &extra) > 64 * 2 || extra)
			goto err;
		if ((p = strtokm(NULL, "*")) == NULL)   // ciphertext
			goto err;
		if (hexlenl(p, &extra) != 64 || extra)
			goto err;
		if ((p = strtokm(NULL, "*")) == NULL)   // mac
			goto err;
		if (hexlenl(p, &extra) != 64 || extra)
			goto err;
	}

	MEM_FREE(keeptr);
	return 1;

err:
	MEM_FREE(keeptr);
	return 0;
}

static void set_salt(void *salt)
{
	cur_salt = (custom_salt*)salt;

	memcpy(host_salt->salt, cur_salt->salt, cur_salt->saltlen);
	host_salt->length = cur_salt->saltlen;
	host_salt->rounds = cur_salt->iterations;

	HANDLE_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_salt,
		CL_FALSE, 0, sizeof(salt_t), host_salt, 0, NULL, NULL),
	    "Copy salt to gpu");
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	int i;
	const int count = *pcount;
	int index;
	int loops = (host_salt->rounds + HASH_LOOPS - 1) / HASH_LOOPS;
	size_t *lws = local_work_size ? &local_work_size : NULL;

	global_work_size = GET_MULTIPLE_OR_BIGGER(count, local_work_size);

	// Copy data to gpu
	BENCH_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_in,
		CL_FALSE, 0, global_work_size * sizeof(pass_t), host_pass, 0,
		NULL, multi_profilingEvent[0]), "Copy data to gpu");

	// Run kernel
	BENCH_CLERROR(clEnqueueNDRangeKernel(queue[gpu_id], crypt_kernel,
		1, NULL, &global_work_size, lws, 0, NULL, multi_profilingEvent[1]), "Run kernel");

	for (i = 0; i < (ocl_autotune_running ? 1 : loops); i++) {
		BENCH_CLERROR(clEnqueueNDRangeKernel(queue[gpu_id], split_kernel,
			1, NULL, &global_work_size, lws, 0, NULL, multi_profilingEvent[2]), "Run split kernel");
		BENCH_CLERROR(clFinish(queue[gpu_id]), "clFinish");
		opencl_process_event();
	}

	// Read the result back
	BENCH_CLERROR(clEnqueueReadBuffer(queue[gpu_id], mem_out,
		CL_TRUE, 0, global_work_size * sizeof(crack_t), host_crack, 0,
		NULL, multi_profilingEvent[3]), "Copy result back");

	if (!ocl_autotune_running) {
		for (index = 0; index < count; index++) {
			Keccak_HashInstance hash;

			Keccak_HashInitialize(&hash, 1088, 512, 256, 0x01); // delimitedSuffix is 0x06 for SHA-3, and 0x01 for Keccak
			Keccak_HashUpdate(&hash, (unsigned char*)host_crack[index].hash + 16, 16 * 8);
			Keccak_HashUpdate(&hash, cur_salt->ct, cur_salt->ctlen * 8);
			Keccak_HashFinal(&hash, (unsigned char*)crypt_out[index]);
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

static void set_key(char *key, int index)
{
	int saved_len = MIN(strlen(key), PLAINTEXT_LENGTH);

	memcpy(host_pass[index].v, key, saved_len);
	host_pass[index].length = saved_len;
}

static char *get_key(int index)
{
	static char ret[PLAINTEXT_LENGTH + 1];
	memcpy(ret, host_pass[index].v, PLAINTEXT_LENGTH);
	ret[host_pass[index].length] = 0;
	return ret;
}

struct fmt_main fmt_opencl_ethereum = {
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
		FMT_CASE | FMT_8_BIT | FMT_HUGE_INPUT,
		{
			"iteration count",
		},
		{ FORMAT_TAG },
		opencl_ethereum_tests
	}, {
		init,
		done,
		reset,
		fmt_default_prepare,
		ethereum_valid,
		fmt_default_split,
		ethereum_get_binary,
		ethereum_common_get_salt,
		{
			ethereum_common_iteration_count,
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
			fmt_default_get_hash  // required due to usage of FMT_HUGE_INPUT
		},
		cmp_all,
		cmp_one,
		cmp_exact
	}
};

#endif /* plugin stanza */

#endif /* HAVE_OPENCL */
