/* Cracker for both MongoDB system and sniffed network hashes. Hacked together
 * during November of 2012 by Dhiru Kholia <dhiru at openwall.com>.
 *
 * Based on https://github.com/cyberpunkych/attacking_mongodb
 *
 * Hash format for MongoDB system hashes: user:$mongodb$0$user$hash
 * Hash format for MongoDB network hashes: user:$mongodb$1$user$salt$hash
 *
 * This software is Copyright (c) 2012, Dhiru Kholia <dhiru.kholia at gmail.com>,
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted. */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_mongodb;
#elif FMT_REGISTERS_H
john_register_one(&fmt_mongodb);
#else

#include "md5.h"
#include <string.h>
#include "arch.h"
#include "misc.h"
#include "common.h"
#include "formats.h"
#include "params.h"
#include "options.h"
#ifdef _OPENMP
static int omp_t = 1;
#include <omp.h>
#ifndef OMP_SCALE
#ifdef __MIC__
#define OMP_SCALE               512
#else
#define OMP_SCALE               16384	// Tuned on K8-dual HT
#endif // __MIC__
#endif // OMP_SCALE
#endif // _OPENMP
#include "memdbg.h"

#define FORMAT_LABEL		"MongoDB"
#define FORMAT_NAME		"system / network"
#define FORMAT_TAG			"$mongodb$"
#define FORMAT_TAG_LEN		(sizeof(FORMAT_TAG)-1)

#define ALGORITHM_NAME		"MD5 32/" ARCH_BITS_STR
#define BENCHMARK_COMMENT	""
#define BENCHMARK_LENGTH	-1
#define PLAINTEXT_LENGTH	32
#define BINARY_SIZE		16
#define SALT_SIZE		sizeof(struct custom_salt)
#define BINARY_ALIGN		sizeof(uint32_t)
#define SALT_ALIGN			sizeof(int)
#define MIN_KEYS_PER_CRYPT	1
#define MAX_KEYS_PER_CRYPT	1

static struct fmt_tests mongodb_tests[] = {
	{"$mongodb$0$sa$75692b1d11c072c6c79332e248c4f699", "sa"},
	{"$mongodb$1$sa$58d3229c83e3f87e$0c85e3f74adce5d037426791940c820a", "sa"},
	/* Ettercap generated test vectors */
	{"$mongodb$1$sa$10441db416a99ffc$797d7e18879446845f10ae9d519960b2", "longpassword"},
	{"$mongodb$1$longusername$86336266301fb552$1abe48bac6ad0bf567ab51b094f026a9", "longpassword"},
	/* Ettercap fixed salt MiTM attack generated test vectors */
	{"$mongodb$1$longusername$0000000000000000$53257e018399a241849cb04c70ba8daa", "longpassword"},
	{"$mongodb$1$longusername$0000000000000000$10290925d16d81e50db242c9f3572d91", "longpassword@12345678"},
	{"$mongodb$1$eight18_characters$8c82aec197929775$5c414259f7f7a42f8c4d1b6ffb37913a", "123"},
	{"$mongodb$1$Herman$9b90cf265f3194d7$a5ca2c517c06fdfb773144d53fb26f56", "123456789"},
	{"$mongodb$1$sz110$be8fa52f0e64c250$441d6ece7356c67dcc69dd26e7e0501f", "passWOrd"},
	{"$mongodb$1$jack$304b81adddfb4d6f$c95e106f1d9952c88044a0b21a6bd3fd", ""},
	// https://jira.mongodb.org/browse/SERVER-9476
	{"$mongodb$1$z$ce88504553b16752$6deb79af26ebcdd2b2c40438008cb7b0", "g"},
	// https://github.com/mongodb/specifications/blob/master/source/auth/auth.rst
	{"$mongodb$1$user$2375531c32080ae8$21742f26431831d5cfca035a08c5bdf6", "pencil"},
	{NULL}
};

static char (*saved_key)[PLAINTEXT_LENGTH + 1];
static uint32_t (*crypt_out)[BINARY_SIZE / sizeof(uint32_t)];

static struct custom_salt {
	int type;
	unsigned char salt[17];
	unsigned char username[128];
} *cur_salt;

static void init(struct fmt_main *self)
{
#ifdef _OPENMP
	omp_t = omp_get_max_threads();
	self->params.min_keys_per_crypt *= omp_t;
	omp_t *= OMP_SCALE;
	self->params.max_keys_per_crypt *= omp_t;
#endif
	saved_key = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*saved_key));
	crypt_out = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*crypt_out));
}

static void done(void)
{
	MEM_FREE(crypt_out);
	MEM_FREE(saved_key);
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *ptr, *ctcopy, *keeptr;
	int type, extra;

	if (strncmp(ciphertext, FORMAT_TAG, FORMAT_TAG_LEN))
		return 0;
	if (!(ctcopy = strdup(ciphertext)))
		return 0;
	keeptr = ctcopy;
	ctcopy += FORMAT_TAG_LEN;

	if (!(ptr = strtokm(ctcopy, "$"))) /* type */
		goto error;
	if (!isdec(ptr))
		goto error;
	type = atoi(ptr);
	if (type != 0 && type != 1)
		goto error;
	if (!(ptr = strtokm(NULL, "$"))) /* username */
		goto error;
	if (strlen(ptr) > 127)
		goto error;
	if (type == 0) {
		if (!(ptr = strtokm(NULL, "$"))) /* hash */
			goto error;
		if (hexlenl(ptr, &extra) != 32 || extra)
			goto error;
	} else {
		if (!(ptr = strtokm(NULL, "$"))) /* salt */
			goto error;
		if (hexlenl(ptr, &extra) != 16 || extra)
			goto error;
		if (!(ptr = strtokm(NULL, "$"))) /* hash */
			goto error;
		if (hexlenl(ptr, &extra) != 32 || extra)
			goto error;
	}

	MEM_FREE(keeptr);
	return 1;

error:
	MEM_FREE(keeptr);
	return 0;
}

static void *get_salt(char *ciphertext)
{
	char *ctcopy = strdup(ciphertext);
	char *keeptr = ctcopy;
	char *p;
	static struct custom_salt cs;
	memset(&cs, 0, sizeof(cs));
	ctcopy += FORMAT_TAG_LEN;	/* skip over "$mongodb$" */
	p = strtokm(ctcopy, "$");
	cs.type = atoi(p);
	p = strtokm(NULL, "$");
	strcpy((char*)cs.username, p);
	if (cs.type == 1) {
		p = strtokm(NULL, "$");
		strcpy((char*)cs.salt, p);
	}
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
	p = strrchr(ciphertext, '$') + 1;
	for (i = 0; i < BINARY_SIZE; i++) {
		out[i] =
		    (atoi16[ARCH_INDEX(*p)] << 4) |
		    atoi16[ARCH_INDEX(p[1])];
		p += 2;
	}

	return out;
}

static int get_hash_0(int index) { return crypt_out[index][0] & PH_MASK_0; }
static int get_hash_1(int index) { return crypt_out[index][0] & PH_MASK_1; }
static int get_hash_2(int index) { return crypt_out[index][0] & PH_MASK_2; }
static int get_hash_3(int index) { return crypt_out[index][0] & PH_MASK_3; }
static int get_hash_4(int index) { return crypt_out[index][0] & PH_MASK_4; }
static int get_hash_5(int index) { return crypt_out[index][0] & PH_MASK_5; }
static int get_hash_6(int index) { return crypt_out[index][0] & PH_MASK_6; }

static void set_salt(void *salt)
{
	cur_salt = (struct custom_salt *)salt;
}

inline static void hex_encode(unsigned char *str, int len, unsigned char *out)
{
	int i;
	for (i = 0; i < len; ++i) {
		out[0] = itoa16[str[i]>>4];
		out[1] = itoa16[str[i]&0xF];
		out += 2;
	}
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index = 0;
#ifdef _OPENMP
#pragma omp parallel for
	for (index = 0; index < count; index++)
#endif
	{
		if (cur_salt->type == 0) {
			MD5_CTX ctx;
			MD5_Init(&ctx);
			MD5_Update(&ctx, cur_salt->username, strlen((char*)cur_salt->username));
			MD5_Update(&ctx, ":mongo:", 7);
			MD5_Update(&ctx, saved_key[index], strlen(saved_key[index]));
			MD5_Final((unsigned char*)crypt_out[index], &ctx);
		}
		else {
			unsigned char hexout[32];
			unsigned char out[32];
			MD5_CTX ctx;
			MD5_Init(&ctx);
			MD5_Update(&ctx, cur_salt->username, strlen((char*)cur_salt->username));
			MD5_Update(&ctx, ":mongo:", 7);
			MD5_Update(&ctx, saved_key[index], strlen(saved_key[index]));
			MD5_Final(out, &ctx);
			hex_encode(out, 16, hexout);
			MD5_Init(&ctx);
			MD5_Update(&ctx, cur_salt->salt, 16);
			MD5_Update(&ctx, cur_salt->username, strlen((char*)cur_salt->username));
			MD5_Update(&ctx, hexout, 32);
			MD5_Final((unsigned char*)crypt_out[index], &ctx);
		}
	}
	return count;
}

static int cmp_all(void *binary, int count)
{
	int index = 0;
#ifdef _OPENMP
	for (; index < count; index++)
#endif
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

static void mongodb_set_key(char *key, int index)
{
	int saved_len = strlen(key);
	if (saved_len > PLAINTEXT_LENGTH)
		saved_len = PLAINTEXT_LENGTH;
	memcpy(saved_key[index], key, saved_len);
	saved_key[index][saved_len] = 0;
}

static char *get_key(int index)
{
	return saved_key[index];
}

/*
 * report salt type as first "tunable cost"
 */
static unsigned int mongodb_salt_type(void *salt)
{
	struct custom_salt *my_salt;

	my_salt = salt;
	return (unsigned int) my_salt->type;
}

struct fmt_main fmt_mongodb = {
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
		FMT_CASE | FMT_8_BIT | FMT_OMP | FMT_OMP_BAD,
		{
			"salt type",
			/* FIXME: report user name length as 2nd cost? */
		},
		{ FORMAT_TAG },
		mongodb_tests
	}, {
		init,
		done,
		fmt_default_reset,
		fmt_default_prepare,
		valid,
		fmt_default_split,
		get_binary,
		get_salt,
		{
			mongodb_salt_type,
		},
		fmt_default_source,
		{
			fmt_default_binary_hash_0,
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
		mongodb_set_key,
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
