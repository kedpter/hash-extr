/* LastPass sniffed session cracker patch for JtR. Hacked together during
 * November of 2012 by Dhiru Kholia <dhiru at openwall.com>.
 *
 * Burp Suite is awesome. Open-source it!
 *
 * This software is Copyright (c) 2012 Dhiru Kholia <dhiru at openwall.com>,
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * Jan, 2015, JimF.  Fixed salt-dupe problem. Now salt ONLY depends upon
 * unencrypted user name, so we have real salt-dupe removal.
 */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_sniffed_lastpass;
#elif FMT_REGISTERS_H
john_register_one(&fmt_sniffed_lastpass);
#else

#include <string.h>
#include <errno.h>
#include "arch.h"
#include "johnswap.h"
#include "misc.h"
#include "common.h"
#include "formats.h"
#include "params.h"
#include "options.h"
#include "base64_convert.h"
#include "aes.h"
#include "pbkdf2_hmac_sha256.h"
#ifdef _OPENMP
#include <omp.h>
#ifndef OMP_SCALE
#define OMP_SCALE               64
#endif
#endif
#include "memdbg.h"

#define FORMAT_LABEL		"LastPass"
#define FORMAT_NAME		"sniffed sessions"
#define FORMAT_TAG			"$lastpass$"
#define FORMAT_TAG_LEN		(sizeof(FORMAT_TAG)-1)
#ifdef SIMD_COEF_32
#define ALGORITHM_NAME		"PBKDF2-SHA256 AES " SHA256_ALGORITHM_NAME
#else
#define ALGORITHM_NAME		"PBKDF2-SHA256 AES 32/" ARCH_BITS_STR
#endif
#define BENCHMARK_COMMENT	""
#define BENCHMARK_LENGTH	0
#define PLAINTEXT_LENGTH	55
#define BINARY_SIZE		16
#define SALT_SIZE		sizeof(struct custom_salt)
#define BINARY_ALIGN		4
#define SALT_ALIGN			sizeof(int)
#ifdef SIMD_COEF_32
#define MIN_KEYS_PER_CRYPT	SSE_GROUP_SZ_SHA256
#define MAX_KEYS_PER_CRYPT	SSE_GROUP_SZ_SHA256
#else
#define MIN_KEYS_PER_CRYPT	1
#define MAX_KEYS_PER_CRYPT	1
#endif

/* sentms=1352643586902&xml=2&username=hackme%40mailinator.com&method=cr&hash=4c11d8717015d92db74c42bc1a2570abea3fa18ab17e58a51ce885ee217ccc3f&version=2.0.15&encrypted_username=i%2BhJCwPOj5eQN4tvHcMguoejx4VEmiqzOXOdWIsZKlk%3D&uuid=aHnPh8%40NdhSTWZ%40GJ2fEZe%24cF%40kdzdYh&lang=en-US&iterations=500&sessonly=0&otp=&sesameotp=&multifactorresponse=&lostpwotphash=07a286341be484fc3b96c176e611b10f4d74f230c516f944a008f960f4ec8870&requesthash=i%2BhJCwPOj5eQN4tvHcMguoejx4VEmiqzOXOdWIsZKlk%3D&requestsrc=cr&encuser=i%2BhJCwPOj5eQN4tvHcMguoejx4VEmiqzOXOdWIsZKlk%3D&hasplugin=2.0.15
 * decodeURIComponent("hackme%40mailinator.com")
 * decodeURIComponent("i%2BhJCwPOj5eQN4tvHcMguoejx4VEmiqzOXOdWIsZKlk%3D") */

/* C:\Users\Administrator\AppData\Local\Google\Chrome\User Data\Default\Extensions\hdokiejnpimakedhajhdlcegeplioahd\2.0.15_0
 * lpfulllib.js and server.js are main files involved */

static struct fmt_tests lastpass_tests[] = {
	{"$lastpass$hackme@mailinator.com$500$i+hJCwPOj5eQN4tvHcMguoejx4VEmiqzOXOdWIsZKlk=", "openwall"},
	{"$lastpass$pass_gen@generated.com$500$vgC0g8BxOi4MerkKfZYFFSAJi8riD7k0ROLpBEA3VJk=", "password"},
	// get one with salt under 16 bytes.
	{"$lastpass$1@short.com$500$2W/GA8d2N+Z4HGvRYs2R7w==", "password"},
	{NULL}
};

static char (*saved_key)[PLAINTEXT_LENGTH + 1];
static uint32_t (*crypt_key)[4];

static struct custom_salt {
	unsigned int iterations;
	unsigned int length;
	char username[129];
} *cur_salt;

static void init(struct fmt_main *self)
{

#if defined (_OPENMP)
	int omp_t = omp_get_max_threads();
	self->params.min_keys_per_crypt *= omp_t;
	omp_t *= OMP_SCALE;
	self->params.max_keys_per_crypt *= omp_t;
#endif
	saved_key = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*saved_key));
	crypt_key = mem_calloc(self->params.max_keys_per_crypt,
	                       sizeof(*crypt_key));
}

static void done(void)
{
	MEM_FREE(crypt_key);
	MEM_FREE(saved_key);
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *ctcopy, *keeptr, *p;
	if (strncmp(ciphertext, FORMAT_TAG, FORMAT_TAG_LEN) != 0)
		return 0;
	ctcopy = strdup(ciphertext);
	keeptr = ctcopy;
	ctcopy += FORMAT_TAG_LEN;
	if ((p = strtokm(ctcopy, "$")) == NULL)	/* username */
		goto err;
	if (strlen(p) > 128)
		goto err;
	if ((p = strtokm(NULL, "$")) == NULL)	/* iterations */
		goto err;
	if (!isdec(p))
		goto err;
	if ((p = strtokm(NULL, "$")) == NULL)	/* data */
		goto err;
	if (strlen(p) > 50) /* not exact! */
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
	static struct custom_salt cs;
	memset(&cs, 0, sizeof(cs));
	ctcopy += FORMAT_TAG_LEN;	/* skip over "$lastpass$" */
	p = strtokm(ctcopy, "$");
	i = strlen(p);
	if (i > 16)
		i = 16;
	cs.length = i; /* truncated length */
	strncpy(cs.username, p, 128);
	p = strtokm(NULL, "$");
	cs.iterations = atoi(p);
	MEM_FREE(keeptr);
	return (void *)&cs;
}

static void *get_binary(char *ciphertext)
{
	static unsigned int out[4];
	char Tmp[48];
	char *p;
	ciphertext += FORMAT_TAG_LEN;
	p = strchr(ciphertext, '$')+1;
	p = strchr(p, '$')+1;
	base64_convert(p, e_b64_mime, strlen(p), Tmp, e_b64_raw, sizeof(Tmp), 0, 0);
	memcpy(out, Tmp, 16);
	return out;
}

static void set_salt(void *salt)
{
	cur_salt = (struct custom_salt *)salt;
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index = 0;
#ifdef _OPENMP
#pragma omp parallel for
	for (index = 0; index < count; index += MAX_KEYS_PER_CRYPT)
#endif
	{
		uint32_t key[MAX_KEYS_PER_CRYPT][8];
		unsigned i;
#ifdef SIMD_COEF_32
		int lens[MAX_KEYS_PER_CRYPT];
		unsigned char *pin[MAX_KEYS_PER_CRYPT];
		union {
			uint32_t *pout[MAX_KEYS_PER_CRYPT];
			unsigned char *poutc;
		} x;
		for (i = 0; i < MAX_KEYS_PER_CRYPT; ++i) {
			lens[i] = strlen(saved_key[i+index]);
			pin[i] = (unsigned char*)saved_key[i+index];
			x.pout[i] = key[i];
		}
		pbkdf2_sha256_sse((const unsigned char **)pin, lens, (unsigned char*)cur_salt->username, strlen(cur_salt->username), cur_salt->iterations, &(x.poutc), 32, 0);
#else
		pbkdf2_sha256((unsigned char*)saved_key[index], strlen(saved_key[index]), (unsigned char*)cur_salt->username, strlen(cur_salt->username), cur_salt->iterations, (unsigned char*)(&key[0]),32,0);
#endif
		for (i = 0; i < MAX_KEYS_PER_CRYPT; ++i) {
			unsigned char *Key = (unsigned char*)key[i];
			AES_KEY akey;
			unsigned char iv[16];
			unsigned char out[32];
			if (AES_set_encrypt_key(Key, 256, &akey) < 0) {
				fprintf(stderr, "AES_set_encrypt_key failed in crypt!\n");
			}
			memset(iv, 0, sizeof(iv));
			AES_cbc_encrypt((const unsigned char*)cur_salt->username, out, 32, &akey, iv, AES_ENCRYPT);
			memcpy(crypt_key[index+i], out, 16);
		}
	}
	return count;
}

static int get_hash_0(int index) { return crypt_key[index][0] & PH_MASK_0; }
static int get_hash_1(int index) { return crypt_key[index][0] & PH_MASK_1; }
static int get_hash_2(int index) { return crypt_key[index][0] & PH_MASK_2; }
static int get_hash_3(int index) { return crypt_key[index][0] & PH_MASK_3; }
static int get_hash_4(int index) { return crypt_key[index][0] & PH_MASK_4; }
static int get_hash_5(int index) { return crypt_key[index][0] & PH_MASK_5; }
static int get_hash_6(int index) { return crypt_key[index][0] & PH_MASK_6; }

static int cmp_all(void *binary, int count) {
	int index;
	for (index = 0; index < count; index++)
		if ( ((uint32_t*)binary)[0] == crypt_key[index][0] )
			return 1;
	return 0;
}

static int cmp_one(void *binary, int index)
{
	return !memcmp(binary, crypt_key[index], BINARY_SIZE);
}

static int cmp_exact(char *source, int index)
{
	return 1;
}

static void lastpass_set_key(char *key, int index)
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

static unsigned int iteration_count(void *salt)
{
	struct custom_salt *my_salt;

	my_salt = salt;
	return (unsigned int) my_salt->iterations;
}

struct fmt_main fmt_sniffed_lastpass = {
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
		{
			"iteration count",
		},
		{ FORMAT_TAG },
		lastpass_tests
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
			iteration_count,
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
		lastpass_set_key,
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
