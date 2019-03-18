/*
 * TrueCrypt volume OpenCL support to John The Ripper (RIPEMD-160 only)
 *
 * Based on CPU format originally written by Alain Espinosa <alainesp at
 * gmail.com> in 2012.
 * Copyright (c) 2015, magnum
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 */

#if HAVE_OPENCL

#define FMT_STRUCT fmt_ocl_tc
#if FMT_EXTERNS_H
extern struct fmt_main FMT_STRUCT;
#elif FMT_REGISTERS_H
john_register_one(&FMT_STRUCT);
#else

#include <string.h>

#include "arch.h"
#include "misc.h"
#include "memory.h"
#include "common.h"
#include "options.h"
#include "formats.h"
#include "crc32.h"
#include "johnswap.h"
#include "aes.h"
#include "pbkdf2_hmac_ripemd160.h"
#include "loader.h"
#include "common-opencl.h"

#define FORMAT_LABEL            "truecrypt-opencl"
#define FORMAT_NAME             "TrueCrypt AES256_XTS"
#define ALGORITHM_NAME          "RIPEMD160 OpenCL"
#define BENCHMARK_COMMENT       ""
#define BENCHMARK_LENGTH        -1

/* 64 is the actual maximum used by Truecrypt software as of version 7.1a */
#define PLAINTEXT_LENGTH        64
#define MAX_CIPHERTEXT_LENGTH   (512*2+32)
#define SALT_SIZE               sizeof(struct cust_salt)
#define SALT_ALIGN              4
#define BINARY_SIZE             0
#define BINARY_ALIGN            1
#define MIN_KEYS_PER_CRYPT      1
#define MAX_KEYS_PER_CRYPT      1

#define TAG_RIPEMD160           "truecrypt_RIPEMD_160$"
#define TAG_RIPEMD160_LEN       (sizeof(TAG_RIPEMD160)-1)

#define IS_RIPEMD160            2

#define MAX_PASSSZ              64
#define PASS_BUFSZ              256
#define KPOOL_SZ                64
#define MAX_KFILE_SZ            1048576 /* 1 MB */
#define MAX_KEYFILES            256

static unsigned char (*first_block_dec)[16];
unsigned char (*keyfiles_data)[MAX_KFILE_SZ];
int (*keyfiles_length);

#define KEYLEN  PLAINTEXT_LENGTH
#define OUTLEN  64
#define SALTLEN 64

typedef struct {
	unsigned int length;
	unsigned char v[KEYLEN];
} pbkdf2_password;

typedef struct {
	unsigned int v[(OUTLEN+3)/4];
} pbkdf2_hash;

typedef struct {
	unsigned char salt[SALTLEN];
} pbkdf2_salt;

struct cust_salt {
	unsigned char salt[64];
	unsigned char bin[512-64];
	int loop_inc;
	int num_iterations;
	int hash_type;
	int nkeyfiles;
} *psalt;

static struct fmt_tests tests_ripemd160[] = {
	{"truecrypt_RIPEMD_160$b9f118f89d2699cbe42cad7bc2c61b0822b3d6e57e8d43e79f55666aa30572676c3aced5f0900af223e9fcdf43ac39637640977f546eb714475f8e2dbf5368bfb80a671d7796d4a88c36594acd07081b7ef0fbead3d3a0ff2b295e9488a5a2747ed97905436c28c636f408b36b0898aad3c4e9566182bd55f80e97a55ad9cf20899599fb775f314067c9f7e6153b9544bfbcffb53eef5a34b515e38f186a2ddcc7cd3aed635a1fb4aab98b82d57341ec6ae52ad72e43f41aa251717082d0858bf2ccc69a7ca00daceb5b325841d70bb2216e1f0d4dc936b9f50ebf92dbe2abec9bc3babea7a4357fa74a7b2bcce542044552bbc0135ae35568526e9bd2afde0fa4969d6dc680cf96f7d82ec0a75b6170c94e3f2b6fd98f2e6f01db08ce63f1b6bcf5ea380ed6f927a5a8ced7995d83ea8e9c49238e8523d63d6b669ae0d165b94f1e19b49922b4748798129eed9aa2dae0d2798adabf35dc4cc30b25851a3469a9ee0877775abca26374a4176f8d237f8191fcc870f413ffdbfa73ee22790a548025c4fcafd40f631508f1f6c8d4c847e409c839d21ff146f469feff87198bc184db4b5c5a77f3402f491538503f68e0116dac76344b762627ad678de76cb768779f8f1c35338dd9f72dcc1ac337319b0e21551b9feb85f8cac67a2f35f305a39037bf96cd61869bf1761abcce644598dad254990d17f0faa4965926acb75abf", "password" },
	{"truecrypt_RIPEMD_160$6ab053e5ebee8c56bce5705fb1e03bf8cf99e2930232e525befe1e45063aa2e30981585020a967a1c45520543847cdb281557e16c81cea9d329b666e232eeb008dbe3e1f1a181f69f073f0f314bc17e255d42aaa1dbab92231a4fb62d100f6930bae4ccf6726680554dea3e2419fb67230c186f6af2c8b4525eb8ebb73d957b01b8a124b736e45f94160266bcfaeda16b351ec750d980250ebb76672578e9e3a104dde89611bce6ee32179f35073be9f1dee8da002559c6fab292ff3af657cf5a0d864a7844235aeac441afe55f69e51c7a7c06f7330a1c8babae2e6476e3a1d6fb3d4eb63694218e53e0483659aad21f20a70817b86ce56c2b27bae3017727ff26866a00e75f37e6c8091a28582bd202f30a5790f5a90792de010aebc0ed81e9743d00518419f32ce73a8d3f07e55830845fe21c64a8a748cbdca0c3bf512a4938e68a311004538619b65873880f13b2a9486f1292d5c77116509a64eb0a1bba7307f97d42e7cfa36d2b58b71393e04e7e3e328a7728197b8bcdef14cf3f7708cd233c58031c695da5f6b671cc5066323cc86bb3c6311535ad223a44abd4eec9077d70ab0f257de5706a3ff5c15e3bc2bde6496a8414bc6a5ed84fe9462b65efa866312e0699e47338e879ae512a66f3f36fc086d2595bbcff2e744dd1ec283ba8e91299e62e4b2392608dd950ede0c1f3d5b317b2870ead59efe096c054ea1", "123" },
	{NULL}
};

static cl_int cl_error;
static pbkdf2_password *inbuffer;
static pbkdf2_hash *outbuffer;
static pbkdf2_salt currentsalt;
static cl_mem mem_in, mem_out, mem_setting;
static struct fmt_main *self;

static size_t insize, outsize, settingsize;

#define STEP			0
#define SEED			256

// This file contains auto-tuning routine(s). Has to be included after formats definitions.
#include "opencl_autotune.h"
#include "memdbg.h"

static const char * warn[] = {
	"xfer: ",  ", crypt: ",  ", xfer: "
};

/* ------- Helper functions ------- */
static size_t get_task_max_work_group_size()
{
	return autotune_get_task_max_work_group_size(FALSE, 0, crypt_kernel);
}

static void create_clobj(size_t gws, struct fmt_main *self)
{
	insize = sizeof(pbkdf2_password) * gws;
	outsize = sizeof(pbkdf2_hash) * gws;
	settingsize = sizeof(pbkdf2_salt);

	inbuffer = mem_calloc(1, insize);
	outbuffer = mem_alloc(outsize);

	/// Allocate memory
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

	first_block_dec = mem_calloc(gws, sizeof(*first_block_dec));
	keyfiles_data = mem_calloc(MAX_KEYFILES, sizeof(*keyfiles_data));
	keyfiles_length = mem_calloc(MAX_KEYFILES, sizeof(int));
}

static void release_clobj(void)
{
	HANDLE_CLERROR(clReleaseMemObject(mem_in), "Release mem in");
	HANDLE_CLERROR(clReleaseMemObject(mem_setting), "Release mem setting");
	HANDLE_CLERROR(clReleaseMemObject(mem_out), "Release mem out");

	MEM_FREE(inbuffer);
	MEM_FREE(outbuffer);
	MEM_FREE(first_block_dec);
	MEM_FREE(keyfiles_data);
	MEM_FREE(keyfiles_length);
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
		         "-DKEYLEN=%d -DSALTLEN=%d -DOUTLEN=%d",
		         (int)sizeof(inbuffer->v),
		         (int)sizeof(currentsalt.salt),
		         (int)sizeof(outbuffer->v));
		opencl_init("$JOHN/kernels/pbkdf2_ripemd160_kernel.cl",
		            gpu_id, build_opts);

		crypt_kernel = clCreateKernel(program[gpu_id], "pbkdf2_ripemd160",
		                              &cl_error);
		HANDLE_CLERROR(cl_error, "Error creating kernel");

		// Initialize openCL tuning (library) for this format.
		opencl_init_auto_setup(SEED, 0, NULL, warn, 1,
		                       self, create_clobj, release_clobj,
		                       sizeof(pbkdf2_password), 0, db);

		// Auto tune execution from shared/included code.
		autotune_run(self, 1, 0, 1000);
	}
}

static int valid(char* ciphertext, struct fmt_main *self)
{
	unsigned int i;
	char *p, *q;
	int nkeyfiles = -1;

	if (strncmp(ciphertext, TAG_RIPEMD160, TAG_RIPEMD160_LEN))
		return 0;

	ciphertext += TAG_RIPEMD160_LEN;
	p = ciphertext;
	q = strchr(p, '$');

	if (!q) { /* no keyfiles */
		if (strlen(ciphertext) != 512*2)
			return 0;
	} else {
		if (q - p != 512 * 2)
			return 0;
		/* check keyfile(s) */
		p = q + 1;
		nkeyfiles = atoi(p);
		if (nkeyfiles > MAX_KEYFILES || nkeyfiles < 1)
			return 0;
	}

	for (i = 0; i < 512*2; i++) {
		if (atoi16l[ARCH_INDEX(ciphertext[i])] == 0x7F)
			return 0;
	}

	return 1;
}

static void set_salt(void *salt)
{
	psalt = salt;

	memcpy((char*)currentsalt.salt, psalt->salt, SALTLEN);

	HANDLE_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_setting,
		CL_FALSE, 0, settingsize, &currentsalt, 0, NULL, NULL),
	    "Copy salt to gpu");
}

static void* get_salt(char *ciphertext)
{
	static char buf[sizeof(struct cust_salt)+4];
	struct cust_salt *s = (struct cust_salt*)mem_align(buf, 4);
	unsigned int i;
	char tpath[PATH_BUFFER_SIZE] = { 0 };
	char *p, *q;
	int idx;
	FILE *fp;
	size_t sz;

	memset(s, 0, sizeof(struct cust_salt));

	s->loop_inc = 1;
	ciphertext += TAG_RIPEMD160_LEN;
	s->hash_type = IS_RIPEMD160;
	s->num_iterations = 2000;

	// Convert the hexadecimal salt in binary
	for (i = 0; i < 64; i++)
		s->salt[i] = (atoi16[ARCH_INDEX(ciphertext[2*i])] << 4) | atoi16[ARCH_INDEX(ciphertext[2*i+1])];
	for (; i < 512; i++)
		s->bin[i-64] = (atoi16[ARCH_INDEX(ciphertext[2*i])] << 4) | atoi16[ARCH_INDEX(ciphertext[2*i+1])];

	p = ciphertext;
	q = strchr(p, '$');
	if (!q) /* no keyfiles */
		return s;

	// process keyfile(s)
	p = q + 1;
	s->nkeyfiles = atoi(p);

	for (idx = 0; idx < s->nkeyfiles; idx++) {
		p = strchr(p, '$') + 1; // at first filename
		q = strchr(p, '$');

		if (!q) { // last file
			memset(tpath, 0, sizeof(tpath) - 1);
			strncpy(tpath, p, sizeof(tpath));
		} else {
			memset(tpath, 0, sizeof(tpath) - 1);
			strncpy(tpath, p, q-p);
		}
		/* read this into keyfiles_data[idx] */
		fp = fopen(tpath, "rb");
		if (!fp)
			pexit("fopen %s", p);

		if (fseek(fp, 0L, SEEK_END) == -1)
			pexit("fseek");

		sz = ftell(fp);

		if (fseek(fp, 0L, SEEK_SET) == -1)
			pexit("fseek");

		if (fread(keyfiles_data[idx], 1, sz, fp) != sz)
			pexit("fread");

		keyfiles_length[idx] = sz;
		fclose(fp);
	}

	return s;
}

static void AES_256_XTS_first_sector(const unsigned char *double_key,
                                     unsigned char *out,
                                     const unsigned char *data,
                                     unsigned len) {
	unsigned char tweak[16] = { 0 };
	unsigned char buf[16];
	int i, j, cnt;
	AES_KEY key1, key2;
	AES_set_decrypt_key(double_key, 256, &key1);
	AES_set_encrypt_key(&double_key[32], 256, &key2);

	// first aes tweak (we do it right over tweak
	AES_encrypt(tweak, tweak, &key2);

	cnt = len/16;
	for (j=0;;) {
		for (i = 0; i < 16; ++i) buf[i] = data[i]^tweak[i];
		AES_decrypt(buf, out, &key1);
		for (i = 0; i < 16; ++i) out[i]^=tweak[i];
		++j;
		if (j == cnt)
			break;
		else {
			unsigned char Cin, Cout;
			unsigned x;
			Cin = 0;
			for (x = 0; x < 16; ++x) {
				Cout = (tweak[x] >> 7) & 1;
				tweak[x] = ((tweak[x] << 1) + Cin) & 0xFF;
				Cin = Cout;
			}
			if (Cout)
				tweak[0] ^= 135; //GF_128_FDBK;
		}
		data += 16;
		out += 16;
	}
}

static int apply_keyfiles(unsigned char *pass, size_t pass_memsz, int nkeyfiles)
{
	int pl, k;
	unsigned char *kpool;
	unsigned char *kdata;
	int kpool_idx;
	size_t i, kdata_sz;
	uint32_t crc;

	if (pass_memsz < MAX_PASSSZ) {
		error();
	}

	pl = strlen((char*)pass);
	memset(pass+pl, 0, MAX_PASSSZ-pl);

	if ((kpool = mem_calloc(1, KPOOL_SZ)) == NULL) {
		error();
	}

	for (k = 0; k < nkeyfiles; k++) {
		kpool_idx = 0;
		kdata_sz = keyfiles_length[k];
		kdata = keyfiles_data[k];
		crc = ~0U;

		for (i = 0; i < kdata_sz; i++) {
			crc = jtr_crc32(crc, kdata[i]);
			kpool[kpool_idx++] += (unsigned char)(crc >> 24);
			kpool[kpool_idx++] += (unsigned char)(crc >> 16);
			kpool[kpool_idx++] += (unsigned char)(crc >> 8);
			kpool[kpool_idx++] += (unsigned char)(crc);

			/* Wrap around */
			if (kpool_idx == KPOOL_SZ)
				kpool_idx = 0;
		}
	}

	/* Apply keyfile pool to passphrase */
	for (i = 0; i < KPOOL_SZ; i++)
		pass[i] += kpool[i];

	MEM_FREE(kpool);

	return 0;
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	int i;
	const int count = *pcount;
	size_t *lws = local_work_size ? &local_work_size : NULL;

	global_work_size = GET_MULTIPLE_OR_BIGGER(count, local_work_size);

	if (psalt->nkeyfiles) {
#if _OPENMP
#pragma omp parallel for
#endif
		for (i = 0; i < count; i++) {
			apply_keyfiles(inbuffer[i].v, 64, psalt->nkeyfiles);
			inbuffer[i].length = 64;
		}
	}

	/// Copy data to gpu
	BENCH_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_in, CL_FALSE, 0,
		insize, inbuffer, 0, NULL, multi_profilingEvent[0]),
	        "Copy data to gpu");

	/// Run kernel
	BENCH_CLERROR(clEnqueueNDRangeKernel(queue[gpu_id], crypt_kernel, 1,
		NULL, &global_work_size, lws, 0, NULL,
	        multi_profilingEvent[1]), "Run kernel");

	/// Read the result back
	BENCH_CLERROR(clEnqueueReadBuffer(queue[gpu_id], mem_out, CL_TRUE, 0,
		outsize, outbuffer, 0, NULL, multi_profilingEvent[2]), "Copy result back");

	if (ocl_autotune_running)
		return count;

#if _OPENMP
#pragma omp parallel for
#endif
	for (i = 0; i < count; i++) {
		AES_256_XTS_first_sector((unsigned char*)outbuffer[i].v, first_block_dec[i], psalt->bin, 16);
	}
	return count;
}

static int cmp_all(void* binary, int count)
{
	int i;
	for (i = 0; i < count; ++i) {
		if (!memcmp(first_block_dec[i], "TRUE", 4))
			return 1;
	}
	return 0;
}

static int cmp_one(void* binary, int index)
{
	if (!memcmp(first_block_dec[index], "TRUE", 4))
		return 1;
	return 0;
}

static int cmp_crc32s(unsigned char *given_crc32, CRC32_t comp_crc32) {
	return given_crc32[0] == ((comp_crc32>>24)&0xFF) &&
		given_crc32[1] == ((comp_crc32>>16)&0xFF) &&
		given_crc32[2] == ((comp_crc32>> 8)&0xFF) &&
		given_crc32[3] == ((comp_crc32>> 0)&0xFF);
}

static int cmp_exact(char *source, int idx)
{
	unsigned char key[64];
	unsigned char decr_header[512-64];
	CRC32_t check_sum;
	int ksz = inbuffer[idx].length;

	memcpy(key, inbuffer[idx].v, inbuffer[idx].length);

	/* process keyfile(s) */
	if (psalt->nkeyfiles) {
		apply_keyfiles(key, 64, psalt->nkeyfiles);
		ksz = 64;
	}

	pbkdf2_ripemd160(key, ksz, psalt->salt, 64, psalt->num_iterations, key, sizeof(key), 0);

	AES_256_XTS_first_sector(key, decr_header, psalt->bin, 512-64);

	if (memcmp(decr_header, "TRUE", 4))
		return 0;

	CRC32_Init(&check_sum);
	CRC32_Update(&check_sum, &decr_header[256-64], 256);
	if (!cmp_crc32s(&decr_header[8], ~check_sum))
		return 0;

	CRC32_Init(&check_sum);
	CRC32_Update(&check_sum, decr_header, 256-64-4);
	if (!cmp_crc32s(&decr_header[256-64-4], ~check_sum))
		return 0;

	return 1;
}

#undef set_key
static void set_key(char *key, int index)
{
	uint8_t length = strlen(key);
	if (length > PLAINTEXT_LENGTH)
		length = PLAINTEXT_LENGTH;
	inbuffer[index].length = length;
	memcpy(inbuffer[index].v, key, length);
}

static char *get_key(int index)
{
	static char ret[PLAINTEXT_LENGTH + 1];
	uint8_t length = inbuffer[index].length;
	memcpy(ret, inbuffer[index].v, length);
	ret[length] = '\0';
	return ret;
}

static int salt_hash(void *salt)
{
	unsigned v=0, i;
	struct cust_salt *psalt = (struct cust_salt*)salt;
	for (i = 0; i < 64; ++i) {
		v *= 11;
		v += psalt->salt[i];
	}
	return v & (SALT_HASH_SIZE - 1);
}

struct fmt_main FMT_STRUCT = {
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
		FMT_CASE | FMT_8_BIT | FMT_OMP | FMT_HUGE_INPUT,
		{ NULL },
		{ TAG_RIPEMD160 },
		tests_ripemd160
	}, {
		init,
		done,
		reset,
		fmt_default_prepare,
		valid,
		fmt_default_split,
		fmt_default_binary,
		get_salt,
		{ NULL },
		fmt_default_source,
		{
			fmt_default_binary_hash
		},
		salt_hash,
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
