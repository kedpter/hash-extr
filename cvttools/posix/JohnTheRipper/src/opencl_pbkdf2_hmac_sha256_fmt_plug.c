/*
 * This software is Copyright (c) 2013 Lukas Odzioba <ukasz at openwall dot net>
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 */
#ifdef HAVE_OPENCL

#if FMT_EXTERNS_H
extern struct fmt_main fmt_opencl_pbkdf2_hmac_sha256;
#elif FMT_REGISTERS_H
john_register_one(&fmt_opencl_pbkdf2_hmac_sha256);
#else

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "misc.h"
#include "arch.h"
#include "base64_convert.h"
#include "common.h"
#include "formats.h"
#include "options.h"
#include "common-opencl.h"
#include "pbkdf2_hmac_common.h"

#define FORMAT_LABEL		"PBKDF2-HMAC-SHA256-opencl"
#define FORMAT_NAME		""
#define ALGORITHM_NAME		"PBKDF2-SHA256 OpenCL"

#define SALT_ALIGN		1

#define PLAINTEXT_LENGTH	55
#define SALT_SIZE		sizeof(salt_t)

#define KERNEL_NAME		"pbkdf2_sha256_kernel"
#define SPLIT_KERNEL_NAME	"pbkdf2_sha256_loop"

#define HASH_LOOPS		(13*71) // factors 13, 13, 71
#define ITERATIONS		12000

typedef struct {
	uint8_t length;
	uint8_t v[PLAINTEXT_LENGTH];
} pass_t;

typedef struct {
	uint32_t hash[8];
} crack_t;

typedef struct {
	uint8_t length;
	uint8_t salt[PBKDF2_32_MAX_SALT_SIZE];
	uint32_t rounds;
} salt_t;

typedef struct {
	uint32_t ipad[8];
	uint32_t opad[8];
	uint32_t hash[8];
	uint32_t W[8];
	uint32_t rounds;
} state_t;

//#define DEBUG
static pass_t *host_pass;			      /** plain ciphertexts **/
static salt_t *host_salt;			      /** salt **/
static crack_t *host_crack;			      /** hash**/
static cl_int cl_error;
static cl_mem mem_in, mem_out, mem_salt, mem_state;
static cl_kernel split_kernel;
static struct fmt_main *self;

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

static void *get_salt(char *ciphertext)
{
	static salt_t salt;
	char *p, *c = ciphertext;
	uint32_t rounds;

	memset(&salt, 0, sizeof(salt));
	c += PBKDF2_SHA256_TAG_LEN;
	rounds = strtol(c, NULL, 10);
	c = strchr(c, '$') + 1;
	p = strchr(c, '$');
	if (p-c==14 && rounds==20000) {
		// for now, assume this is a cisco8 hash
		strnzcpy((char*)(salt.salt), c, 15);
		salt.length = 14;
		salt.rounds = rounds;
		return (void*)&salt;
	}
	salt.length = base64_convert(c, e_b64_mime, p-c, salt.salt, e_b64_raw, sizeof(salt.salt), flg_Base64_MIME_PLUS_TO_DOT, 0);
	salt.rounds = rounds;
	return (void *)&salt;
}

static void set_salt(void *salt)
{
	memcpy(host_salt, salt, SALT_SIZE);
	HANDLE_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_salt,
		CL_FALSE, 0, sizeof(salt_t), host_salt, 0, NULL, NULL),
	    "Copy salt to gpu");
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	int i;
	const int count = *pcount;
	int loops = (host_salt->rounds + HASH_LOOPS - 1) / HASH_LOOPS;
	size_t *lws = local_work_size ? &local_work_size : NULL;

	global_work_size = GET_MULTIPLE_OR_BIGGER(count, local_work_size);

#if 0
	printf("crypt_all(%d)\n", count);
	printf("LWS = %d, GWS = %d\n", (int)local_work_size, (int)global_work_size);
#endif

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

	return count;
}

static int cmp_all(void *binary, int count)
{
	int i;
	for (i = 0; i < count; i++)
		if (host_crack[i].hash[0] == ((uint32_t *) binary)[0])
			return 1;
	return 0;
}

static int cmp_one(void *binary, int index)
{
	int i;
	for (i = 0; i < 8; i++)
		if (host_crack[index].hash[i] != ((uint32_t *) binary)[i])
			return 0;
	return 1;
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

static int binary_hash_0(void *binary)
{
#if 0
	uint32_t i, *b = binary;
	puts("binary");
	for (i = 0; i < 8; i++)
		printf("%08x ", b[i]);
	puts("");
#endif
	return (((uint32_t *) binary)[0] & PH_MASK_0);
}

static int get_hash_0(int index)
{
#if 0
	uint32_t i;
	puts("get_hash");
	for (i = 0; i < 8; i++)
		printf("%08x ", ((uint32_t *) host_crack[index].hash)[i]);
	puts("");
#endif
	return host_crack[index].hash[0] & PH_MASK_0;
}

static int get_hash_1(int index)
{
	return host_crack[index].hash[0] & PH_MASK_1;
}

static int get_hash_2(int index)
{
	return host_crack[index].hash[0] & PH_MASK_2;
}

static int get_hash_3(int index)
{
	return host_crack[index].hash[0] & PH_MASK_3;
}

static int get_hash_4(int index)
{
	return host_crack[index].hash[0] & PH_MASK_4;
}

static int get_hash_5(int index)
{
	return host_crack[index].hash[0] & PH_MASK_5;
}

static int get_hash_6(int index)
{
	return host_crack[index].hash[0] & PH_MASK_6;
}

static unsigned int iteration_count(void *salt)
{
	salt_t *my_salt;

	my_salt = salt;
	return (unsigned int)my_salt->rounds;
}

struct fmt_main fmt_opencl_pbkdf2_hmac_sha256 = {
{
	FORMAT_LABEL,
	FORMAT_NAME,
	ALGORITHM_NAME,
	BENCHMARK_COMMENT,
	BENCHMARK_LENGTH,
	0,
	PLAINTEXT_LENGTH,
	PBKDF2_SHA256_BINARY_SIZE,
	PBKDF2_32_BINARY_ALIGN,
	SALT_SIZE,
	SALT_ALIGN,
	1,
	1,
	FMT_CASE | FMT_8_BIT,
		{
			"iteration count",
		},
		{ PBKDF2_SHA256_FORMAT_TAG, FORMAT_TAG_CISCO8 },
	pbkdf2_hmac_sha256_common_tests
}, {
	init,
	done,
	reset,
	pbkdf2_hmac_sha256_prepare,
	pbkdf2_hmac_sha256_valid,
	fmt_default_split,
	pbkdf2_hmac_sha256_binary,
	get_salt,
		{
			iteration_count,
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
	NULL,
	set_salt,
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
}};

#endif /* plugin stanza */

#endif /* HAVE_OPENCL */
