/*
 *  Modified by Dhiru Kholia <dhiru at openwall.com> for ODF AES format.
 *
 * This software is Copyright (c) 2012 Lukas Odzioba <ukasz@openwall.net>
 * and Copyright (c) 2017 magnum
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 */

#ifdef HAVE_OPENCL

#if FMT_EXTERNS_H
extern struct fmt_main fmt_opencl_odf_aes;
#elif FMT_REGISTERS_H
john_register_one(&fmt_opencl_odf_aes);
#else

#include <stdint.h>
#include <string.h>

#include "arch.h"
#include "formats.h"
#include "common.h"
#include "misc.h"
#include "options.h"
#include "common.h"
#include "formats.h"
#include "aes.h"
#include "common-opencl.h"

#define FORMAT_LABEL        "ODF-AES-opencl"
#define FORMAT_NAME         ""
#define FORMAT_TAG           "$odf$*"
#define FORMAT_TAG_LEN      (sizeof(FORMAT_TAG)-1)
#define ALGORITHM_NAME      "SHA256 PBKDF2-SHA1 AES OpenCL"
#define BENCHMARK_COMMENT   ""
#define BENCHMARK_LENGTH    -1
#define MIN_KEYS_PER_CRYPT  1
#define MAX_KEYS_PER_CRYPT  1
#define BINARY_SIZE         (256/8)
#define PLAINTEXT_LENGTH    63
#define SALT_SIZE           sizeof(odf_cpu_salt)
#define AES_LEN             1024

typedef struct {
	char    v[PLAINTEXT_LENGTH + 1];
} odf_password;

typedef struct {
	uint8_t v[256/8];
} odf_sha_key;

typedef struct {
	uint32_t iterations;
	uint32_t outlen;
	uint32_t skip_bytes;
	uint8_t  aes_ct[AES_LEN]; /* ciphertext */
	uint32_t aes_len;         /* actual data length (up to AES_LEN) */
	uint8_t  iv[16];
	uint8_t  salt[64];
	uint8_t  length;
} odf_salt;

typedef struct {
	uint v[BINARY_SIZE / sizeof(uint)]; /* output from final SHA-256 */
} odf_out;

static cl_int cl_error;
static cl_mem mem_in, mem_out, mem_setting, mem_key;
static odf_password *saved_key;
static odf_out *crypt_out;
static odf_salt currentsalt;

size_t insize, outsize, settingsize, cracked_size, sha_size;

typedef struct {
	int cipher_type;
	int checksum_type;
	int iterations;
	int key_size;
	int iv_length;
	int salt_length;
	int content_length;
	unsigned char iv[16];
	unsigned char salt[32];
	unsigned char content[AES_LEN];
} odf_cpu_salt;

static odf_cpu_salt *cur_salt;

static struct fmt_tests tests[] = {
	{"$odf$*1*1*1024*32*61802eba18eab842de1d053809ba40927fd40b26c69ddeca6a8a652ed9c16a28*16*c5c0815b931f313627100d592a9c972f*16*e9a48b7daff738deaabe442007fb2ec4*0*be3b65ea09642c2b4fdc23e553e1f5304bc5df222b624c6373d53e674f5df01fdb8873cdab7a5a685fa45ad5441a9d8869401b7fa076c488ad53fd9971e97244ecc9416484450d4fb2ee4ec08af4044d7def937e6545dea2ce36bd5c57b1f46b11b9cf90c8fb3accff149ce2d54820b181b9124db9aac131f6436d77cf716423f04d42438eed6f9ca14bd24b9b17d3478176addd5fa0254bf986fccd879e326485790e28b94ad5306868734b5ac1b1ddb3f876382dee6e9428e8230e84bf11b7e85ccbae8b4b424cd73160c380f874b37fbe3c7e88c13ef4bde74b56507d17095c2c32bb8bcded0637e4403107bb33252f72f5886a91b7720fe32a8659a09c217717e4c74a7c2e09fc40b46aa288309a36e86b9f1856e1bce176bc9690555431e05c7b67ff95df64f8f40053079bfc9dda021ab2714fecf74398b867ebef675958f29eaa15eb631845e358a0c5caff0b824a2a69a6eabee069d3d6236d77709fd60438c9e3ad9e42b26810375e1e587eff105ac295327ef8bf66f6462388b7727ec32d6abde2f8d6126b185124bb437753663f6ab1f321ddfdb36d9f1f528729492e0b1bb8d3b9eda3c86c1997c92b902f5160f77587c37e45b5c133b5d9709fea910a2e9b54c0960b0ebc870cdbb858aabe07ed27cba86d29a7e64c6e3863131859314a14e64c1168d4a2d5ca0697853fb1fe969ba968e31359881d51edce287eff415de8e60cec2068bb82157fbcf0cf9a95e92cb23f32e6156daced4bee6ba8c8b41174d01fcd7662911bcc10d5b4478f8209ce3b91075d10529780be4f17e841a1f1833d432c3dc854908643e58b03c8860dfbc710a29f79f75ea262cfcef9cd67fb67d73f55b300d42f4577445af2b9f224620204cfb88de2cbf57931ac0e0f8d98259a41d744cad6a58abc7761c266f4e93aca19356b07073c09ae9d1976f4f2e1a76c350cc7764c27ae257eb69ba4213dd0a7794fa83d220439a398efd988b6dbf0de4c08bc3e4830c9e482b9e0fd1679f14e6f132cf06bae1d763dde7ce6f525ff9a0ebad28aeca16496194f2a6263a20e7afeb43d83c8c936130d6508f2bf68b5ca50375948424193a7fb1106fdf63ff72896e1b2633907f01a693218e3303436542bcf2af24cc4a41621c36768ce9a84d32cc9f3c2b108bfc78c25b1c2ea94e6e0d65406f78bdb8bc33c94a9550e5cc3e995cfbd31da03afb929418acdc89b099415f9bdb7dab7a75d44a696e14b031d601ad8d907e14a28044706c0c2955df2cb34ffea82af367e487b6cc928dc87a33fc7555173e7faa5cfd1af6d3d6f496f23a9579db22dd4a2c16e950fdc90696d95a81183765a4fbddb42c488d40ac1de28483cf1cdddf821d3f859c57b13cb7f21a916bd0d89438a17634c68637f23e2544589e8ae5ee5bced91680c087cb3105cd74a09e88d3aae17d75e", "test"},
	/* CMIYC 2013 "pro" hard hash */
	{"$odf$*1*1*1024*32*7db40092b3857fa319bc0d717b60cefc40b1d51ef92ebc893c518ffebffdf200*16*5f7c8ab6e5d1c41dbd23c384fee957ed*16*9ff092f2dd29dab6ce5fb43ad7bbdd5a*0*bac8343436715b40aaf4690a7dc57b0f82b8f25f8ad0f9833e32468410d4dd02e387a067872b5847adc9a276c86a03113e11b903854202eec361c5b7ba74bcb254a4f76d97ca45dbe30fe49f78ce9cf7df0246ae4524b8f13ad28357838559c116d9ed59267f4df91da3ea9758c132e2ebc40fd4ee8e9978921a0847d7ca5c30ef911e0b88f9fc84039633eacf5e023c82dd1a573abd7663b8f36a039d42ed91b4a0665902f174be8cefefd367ba9b5da95768550e567242f1b2e2c3866eb8aa3c12d0b34277929616319ea29dd9a3b9addb963d45c7d4c2b54a99b0c1cf24cac3e981ed4e178e621938b83be30f54d37d6425a0b7ac9dff5504830fe1d1f136913c32d8f732eb55e6179ad2699fd851af3a44f8ca914117344e6fadf501bf6f6e0ae7970a2b58eb3af0d89c78411c6adde8aa1f0e8b69c261fd04835cdc3ddf0a6d67ddff33995b5cc7439db83f90c8a2e07e2513771fffcf8b55ce1a382b14ffbf22be9bdd6f83a9b7602995c9793dfffb32c9eb16930c0bb55e5a8364fa06a59fca5af27df4a02565db2b4718ed44405f67a052738692c189039a7fd63713207616eeeebace3c0a3963dd882c485523f49fa0bc2663fc6ef090a220dd5c6554bc0702da8c3122383ea8a009837d549d58ad688c9cc4b8461fe70f4600539cd1d82edd4e110b1c1472dae40adc3126e2a09dd2753dcd83799841745160e235652f601d1257268321f22d19bd9dc811afaf143765c7cb53717ea329e9e4064a3cf54b33d006e93b83102e2ad3327f6d995cb598bd96466b1287e6da9967f4f034c63fd06c6e5c7ec25008c122385f271d18918cff3823f9fbdb37791e7371ce1d6a4ab08c12eca5fceb7c9aa7ce25a8bd640a68c622ddd858973426cb28e65c4c3421b98ebf4916b8c2bfe71b2afec4ab2f99291a4c4d3312521850d46436aecd9e2e93a8619dbc3c1caf4507bb488ce921cd8d13a1640e6c49403e0416924b3b1a01c9939c7bcdec50f057d6f4dccf0afc8c2ad37c4f8429c77cf19ad49db5e5219e965a3ed5d56d799689bd93642602d7959df0493ea62cccff83e66d85bf45d6b5b03e8cfca84daf37ecfccb60f85f3c5102900a02a5df015b1bf1ef55dfb2ab20321bcf3325d1adce22d4456837dcc589ef36d4f06ccdcc96ef10ff806d76f0044e92e192b946ae0f09860a38c2a6052fe84c3e9bb9380e2b344812376c6bbd5c9858745dbd072798a3d7eff31ae5d509c11b5269ec6f2108cb6e72a5ab495ea7aed5bf3dabedbb517dc4ceff818a8e890a6ea9a91bab37e8a463a9d04993c5ba7e40e743e033842540806d4a65258d0f4d5988e1e0011f0e85fcae3b2819c1f17f5c7980ecd87aee425cdab4f34bfb7a31ee7936c60f2f4f52aea67aef4736a419dc9c559279b569f61995eb2d6b7c204c3e9f56ca5c8a889812a30c33", "juNK^r00M!"},
	{NULL}
};

#define STEP			0
#define SEED			256

static struct fmt_main *self;

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
	insize = sizeof(odf_password) * gws;
	settingsize = sizeof(odf_salt);
	sha_size = sizeof(odf_sha_key) * gws;
	outsize = sizeof(odf_out) * gws;

	saved_key = mem_calloc(1, insize);
	crypt_out = mem_alloc(outsize);

	/// Allocate memory
	mem_in =
	    clCreateBuffer(context[gpu_id], CL_MEM_READ_ONLY, insize, NULL,
	    &cl_error);
	HANDLE_CLERROR(cl_error, "Error allocating mem in");
	mem_key =
	    clCreateBuffer(context[gpu_id], CL_MEM_READ_WRITE, sha_size, NULL,
	    &cl_error);
	HANDLE_CLERROR(cl_error, "Error allocating mem key");
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
	HANDLE_CLERROR(clSetKernelArg(crypt_kernel, 1, sizeof(mem_setting),
		&mem_setting), "Error while setting mem_salt kernel argument");
	HANDLE_CLERROR(clSetKernelArg(crypt_kernel, 2, sizeof(mem_out),
		&mem_out), "Error while setting mem_out kernel argument");
	HANDLE_CLERROR(clSetKernelArg(crypt_kernel, 3, sizeof(mem_key),
		&mem_key), "Error while setting mem_key kernel argument");
}

static void release_clobj(void)
{
	if (crypt_out) {
		HANDLE_CLERROR(clReleaseMemObject(mem_in), "Release mem in");
		HANDLE_CLERROR(clReleaseMemObject(mem_setting), "Release mem setting");
		HANDLE_CLERROR(clReleaseMemObject(mem_out), "Release mem out");
		HANDLE_CLERROR(clReleaseMemObject(mem_key), "Release mem key");

		MEM_FREE(saved_key);
		MEM_FREE(crypt_out);
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

static void init(struct fmt_main *_self)
{
	self = _self;
	opencl_prepare_dev(gpu_id);
}

static void reset(struct db_main *db)
{
	if (!autotuned) {
		char build_opts[128];

		snprintf(build_opts, sizeof(build_opts),
		         "-DPLAINTEXT_LENGTH=%d -DKEYLEN=%d -DSALTLEN=%d -DOUTLEN=%d -DAES_LEN=%d",
		         PLAINTEXT_LENGTH,
		         (int)sizeof(odf_sha_key),
		         (int)sizeof(currentsalt.salt),
		         (int)sizeof(odf_out),
		         AES_LEN);
		opencl_init("$JOHN/kernels/odf_aes_kernel.cl",
		            gpu_id, build_opts);

		crypt_kernel = clCreateKernel(program[gpu_id], "dk_decrypt", &cl_error);
		HANDLE_CLERROR(cl_error, "Error creating kernel");

		// Initialize openCL tuning (library) for this format.
		opencl_init_auto_setup(SEED, 0, NULL, warn, 1, self,
		                       create_clobj, release_clobj,
		                       sizeof(odf_password), 0, db);

		// Auto tune execution from shared/included code.
		autotune_run(self, 1, 0, 1000);
	}
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *ctcopy;
	char *keeptr;
	char *p;
	int res, extra;

	if (strncmp(ciphertext, FORMAT_TAG, FORMAT_TAG_LEN))
		return 0;

	ctcopy = strdup(ciphertext);
	keeptr = ctcopy;
	ctcopy += FORMAT_TAG_LEN;
	if ((p = strtokm(ctcopy, "*")) == NULL)	/* cipher type */
		goto err;
	res = atoi(p);
	if (res != 1) {
		goto err;
	}
	if ((p = strtokm(NULL, "*")) == NULL)	/* checksum type */
		goto err;
	res = atoi(p);
	if (res != 0 && res != 1)
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* iterations */
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* key size */
		goto err;
	res = atoi(p);
	if (res != 16 && res != 32)
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* checksum field (skipped) */
		goto err;
	if (hexlenl(p, &extra) != res * 2 || extra)
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* iv length */
		goto err;
	res = atoi(p);
	if (res > 16)
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* iv */
		goto err;
	if (hexlenl(p, &extra) != res * 2 || extra)
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* salt length */
		goto err;
	res = atoi(p);
	if (res > 32)
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* salt */
		goto err;
	if (hexlenl(p, &extra) != res * 2 || extra)
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* something */
		goto err;
	if ((p = strtokm(NULL, "*")) == NULL)	/* content */
		goto err;
	res = strlen(p);
	if (res > 2 * AES_LEN || res & 1)
		goto err;
	if (!ishexlc(p))
		goto err;

	MEM_FREE(keeptr);
	return 1;

err:
	MEM_FREE(keeptr);
	return 0;
}

static void *get_salt(char *ciphertext)
{
	char *ctcopy = strdup(ciphertext);
	char *keeptr = ctcopy;
	int i;
	char *p;
	static odf_cpu_salt cs;

	ctcopy += FORMAT_TAG_LEN;	/* skip over "$odf$*" */
	p = strtokm(ctcopy, "*");
	cs.cipher_type = atoi(p);
	p = strtokm(NULL, "*");
	cs.checksum_type = atoi(p);
	p = strtokm(NULL, "*");
	cs.iterations = atoi(p);
	p = strtokm(NULL, "*");
	cs.key_size = atoi(p);
	p = strtokm(NULL, "*");
	/* skip checksum field */
	p = strtokm(NULL, "*");
	cs.iv_length = atoi(p);
	p = strtokm(NULL, "*");
	for (i = 0; i < cs.iv_length; i++)
		cs.iv[i] = atoi16[ARCH_INDEX(p[i * 2])] * 16
			+ atoi16[ARCH_INDEX(p[i * 2 + 1])];
	p = strtokm(NULL, "*");
	cs.salt_length = atoi(p);
	p = strtokm(NULL, "*");
	for (i = 0; i < cs.salt_length; i++)
		cs.salt[i] = atoi16[ARCH_INDEX(p[i * 2])] * 16
			+ atoi16[ARCH_INDEX(p[i * 2 + 1])];
	p = strtokm(NULL, "*");
	p = strtokm(NULL, "*");
	memset(cs.content, 0, sizeof(cs.content));
	for (i = 0; p[i * 2] && i < AES_LEN; i++)
		cs.content[i] = atoi16[ARCH_INDEX(p[i * 2])] * 16
			+ atoi16[ARCH_INDEX(p[i * 2 + 1])];
	cs.content_length = i;
	MEM_FREE(keeptr);
	return (void *)&cs;
}

static void *get_binary(char *ciphertext)
{
	static union {
		unsigned char c[BINARY_SIZE];
		ARCH_WORD dummy;
	} buf;
	unsigned char *out = buf.c;
	char *p;
	int i;
	char *ctcopy = strdup(ciphertext);
	char *keeptr = ctcopy;

	ctcopy += FORMAT_TAG_LEN;	/* skip over "$odf$*" */
	p = strtokm(ctcopy, "*");
	p = strtokm(NULL, "*");
	p = strtokm(NULL, "*");
	p = strtokm(NULL, "*");
	p = strtokm(NULL, "*");
	for (i = 0; i < BINARY_SIZE; i++) {
		out[i] =
			(atoi16[ARCH_INDEX(*p)] << 4) |
			atoi16[ARCH_INDEX(p[1])];
		p += 2;
	}
	MEM_FREE(keeptr);
	return out;
}

static void set_salt(void *salt)
{
	cur_salt = (odf_cpu_salt*)salt;
	memcpy(currentsalt.salt, cur_salt->salt, cur_salt->salt_length);
	memcpy(currentsalt.aes_ct, cur_salt->content, cur_salt->content_length);
	memcpy(currentsalt.iv, cur_salt->iv, 16);
	currentsalt.aes_len = cur_salt->content_length;
	currentsalt.length = cur_salt->salt_length;
	currentsalt.iterations = cur_salt->iterations;
	currentsalt.outlen = cur_salt->key_size;
	currentsalt.skip_bytes = 0;

	HANDLE_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_setting,
		CL_FALSE, 0, settingsize, &currentsalt, 0, NULL, NULL),
	    "Copy salt to gpu");
}

#undef set_key
static void set_key(char *key, int index)
{
	strnzcpy(saved_key[index].v, key, sizeof(saved_key[index].v));
}

static char *get_key(int index)
{
	return saved_key[index].v;
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	size_t *lws = local_work_size ? &local_work_size : NULL;

	global_work_size = GET_MULTIPLE_OR_BIGGER(count, local_work_size);

	/// Copy data to gpu
	BENCH_CLERROR(clEnqueueWriteBuffer(queue[gpu_id], mem_in, CL_FALSE, 0,
		insize, saved_key, 0, NULL, multi_profilingEvent[0]),
		"Copy data to gpu");

	/// Run kernel
	BENCH_CLERROR(clEnqueueNDRangeKernel(queue[gpu_id], crypt_kernel, 1,
		NULL, &global_work_size, lws, 0, NULL,
		multi_profilingEvent[1]), "Run kernel");

	/// Read the result back
	BENCH_CLERROR(clEnqueueReadBuffer(queue[gpu_id], mem_out, CL_TRUE, 0,
		outsize, crypt_out, 0, NULL, multi_profilingEvent[2]),
		"Copy result back");

	return count;
}

static int cmp_all(void *binary, int count)
{
	int index = 0;
	for (; index < count; index++)
		if (!memcmp(binary, crypt_out[index].v, ARCH_SIZE))
			return 1;
	return 0;
}

static int cmp_one(void *binary, int index)
{
	return !memcmp(binary, crypt_out[index].v, BINARY_SIZE);
}

static int cmp_exact(char *source, int index)
{
	return 1;
}

/*
 * The format tests all have iteration count 1024.
 * Just in case the iteration count is tunable, let's report it.
 */
static unsigned int iteration_count(void *salt)
{
	odf_cpu_salt *my_salt;

	my_salt = salt;
	return (unsigned int) my_salt->iterations;
}

struct fmt_main fmt_opencl_odf_aes = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		0,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		4,
		SALT_SIZE,
		4,
		MIN_KEYS_PER_CRYPT,
		MAX_KEYS_PER_CRYPT,
		FMT_CASE | FMT_8_BIT | FMT_HUGE_INPUT,
		{
			"iteration count",
		},
		{ FORMAT_TAG },
		tests
	}, {
		init,
		done,
		reset,
		fmt_default_prepare,
		valid,
		fmt_default_split,
		get_binary,
		get_salt,
		{
			iteration_count,
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
