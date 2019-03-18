/*
 * Kerberos 5 "PA ENC TIMESTAMP" by magnum (modified by Dhiru)
 *
 * Pcap file -> input file:
 * 1. tshark -r capture.pcapng -T pdml  > ~/capture.pdml
 * 2. krbng2john.py ~/capture.pdml > krb5.in
 * 3. Run john on krb5.in
 *
 * http://www.ietf.org/rfc/rfc4757.txt
 * http://www.securiteam.com/windowsntfocus/5BP0H0A6KM.html
 *
 * Input format is 'user:$krb5pa$etype$user$realm$salt$timestamp+checksum'
 *
 * NOTE: Checksum implies last 12 bytes of PA_ENC_TIMESTAMP value in AS-REQ
 * packet.
 *
 * Default Salt: realm + user
 *
 * AES-256 encryption & decryption of AS-REQ timestamp in Kerberos v5
 * See the following RFC for more details about the crypto & algorithms used:
 *
 * RFC3961 - Encryption and Checksum Specifications for Kerberos 5
 * RFC3962 - Advanced Encryption Standard (AES) Encryption for Kerberos 5
 *
 * march 09 / kevin devine <wyse101 0x40 gmail.com>
 *
 * This software is Copyright (c) 2011 magnum, and it is hereby released to the
 * general public under the following terms:  Redistribution and use in source
 * and binary forms, with or without modification, are permitted.
 *
 * This software is Copyright (c) 2012 Dhiru Kholia (dhiru at openwall.com) and
 * released under same terms as above
 */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_krb5pa;
#elif FMT_REGISTERS_H
john_register_one(&fmt_krb5pa);
#else

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef _OPENMP
static int omp_t = 1;
#include <omp.h>
#ifndef OMP_SCALE
#define OMP_SCALE               64
#endif
#endif
#include "arch.h"
#include "misc.h"
#include "formats.h"
#include "options.h"
#include "common.h"
#include "unicode.h"
#include "johnswap.h"
#include "aes.h"
#include "hmac_sha.h"
#include "pbkdf2_hmac_sha1.h"
#include "loader.h"
#include "krb5_common.h"
#include "memdbg.h"

#define FORMAT_LABEL       "krb5pa-sha1"
#define FORMAT_NAME        "Kerberos 5 AS-REQ Pre-Auth etype 17/18" /* aes-cts-hmac-sha1-96 */
#define FORMAT_TAG         "$krb5pa$"
#define FORMAT_TAG_LEN     (sizeof(FORMAT_TAG)-1)
#ifdef SIMD_COEF_32
#define ALGORITHM_NAME     "PBKDF2-SHA1 " SHA1_ALGORITHM_NAME
#else
#define ALGORITHM_NAME     "PBKDF2-SHA1 32/" ARCH_BITS_STR
#endif
#define BENCHMARK_COMMENT  ""
#define BENCHMARK_LENGTH   -1
#define BINARY_SIZE		12
#define BINARY_ALIGN		4
#define PLAINTEXT_LENGTH	125
#define SALT_SIZE		sizeof(struct custom_salt)
#define SALT_ALIGN		4
#ifdef SIMD_COEF_32
#define MIN_KEYS_PER_CRYPT  SSE_GROUP_SZ_SHA1
#define MAX_KEYS_PER_CRYPT  SSE_GROUP_SZ_SHA1
#else
#define MIN_KEYS_PER_CRYPT	1
#define MAX_KEYS_PER_CRYPT	1
#endif
#define MAX_SALTLEN             128
#define MAX_REALMLEN            64
#define MAX_USERLEN             64
#define TIMESTAMP_SIZE          44
#define CHECKSUM_SIZE           BINARY_SIZE
#define TOTAL_LENGTH            (14 + 2 * (CHECKSUM_SIZE + TIMESTAMP_SIZE) + MAX_REALMLEN + MAX_USERLEN + MAX_SALTLEN)

static struct fmt_tests tests[] = {
	{"$krb5pa$18$user1$EXAMPLE.COM$$2a0e68168d1eac344da458599c3a2b33ff326a061449fcbc242b212504e484d45903c6a16e2d593912f56c93883bf697b325193d62a8be9c", "openwall"},
	{"$krb5pa$18$user1$EXAMPLE.COM$$a3918bd0381107feedec8db0022bdf3ac56e534ed54d13c62a7013a47713cfc31ef4e7e572f912fa4164f76b335e588bf29c2d17b11c5caa", "openwall"},
	{"$krb5pa$18$l33t$EXAMPLE.COM$$98f732b309a1d7ef2355a974842a32894d911e97150f5d57f248e1c2632fbd3735c5f156532ccae0341e6a2d779ca83a06021fe57dafa464", "openwall"},
	{"$krb5pa$18$aduser$AD.EXAMPLE.COM$$64dfeee04be2b2e0423814e0df4d0f960885aca4efffe6cb5694c4d34690406071c4968abd2c153ee42d258c5e09a41269bbcd7799f478d3", "password@123"},
	{"$krb5pa$18$aduser$AD.EXAMPLE.COM$$f94f755a8b4493d925094a4eb1cec630ac40411a14c9733a853516fe426637d9daefdedc0567e2bb5a83d4f89a0ad1a4b178662b6106c0ff", "password@12345678"},
	{"$krb5pa$18$aduser$AD.EXAMPLE.COM$AD.EXAMPLE.COMaduser$f94f755a8b4493d925094a4eb1cec630ac40411a14c9733a853516fe426637d9daefdedc0567e2bb5a83d4f89a0ad1a4b178662b6106c0ff", "password@12345678"},
	/* etype 17 hash obtained using MiTM etype downgrade attack */
	{"$krb5pa$17$user1$EXAMPLE.COM$$c5461873dc13665771b98ba80be53939e906d90ae1ba79cf2e21f0395e50ee56379fbef4d0298cfccfd6cf8f907329120048fd05e8ae5df4", "openwall"},
	{NULL},
};

static char (*saved_key)[PLAINTEXT_LENGTH + 1];
static uint32_t (*crypt_out)[BINARY_SIZE / sizeof(uint32_t)];

static struct custom_salt {
	int etype;
	unsigned char realm[64];
	unsigned char user[64];
	unsigned char salt[128]; /* realm + user */
	unsigned char ct[44];
} *cur_salt;

static unsigned char constant[16];
static unsigned char ke_input[16];
static unsigned char ki_input[16];

static void init(struct fmt_main *self)
{
	unsigned char usage[5];
#ifdef _OPENMP
	omp_t = omp_get_max_threads();
	self->params.min_keys_per_crypt *= omp_t;
	omp_t *= OMP_SCALE;
	self->params.max_keys_per_crypt *= omp_t;
#endif
	saved_key = mem_calloc(sizeof(*saved_key), self->params.max_keys_per_crypt);
	crypt_out = mem_calloc(sizeof(*crypt_out), self->params.max_keys_per_crypt);

	// generate 128 bits from 40 bits of "kerberos" string
	nfold(8 * 8, (unsigned char*)"kerberos", 128, constant);

	memset(usage, 0, sizeof(usage));
	usage[3] = 0x01;        // key number in big-endian format
	usage[4] = 0xAA;        // used to derive Ke
	nfold(sizeof(usage) * 8, usage, sizeof(ke_input) * 8, ke_input);

	memset(usage, 0, sizeof(usage));
	usage[3] = 0x01;        // key number in big-endian format
	usage[4] = 0x55;        // used to derive Ki
	nfold(sizeof(usage) * 8, usage, sizeof(ki_input) * 8, ki_input);
}

static void done(void)
{
	MEM_FREE(crypt_out);
	MEM_FREE(saved_key);
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *p, *data = ciphertext;
	int type, saltlen = 0;

	// tag is mandatory
	if (strncmp(ciphertext, FORMAT_TAG, FORMAT_TAG_LEN) != 0)
		return 0;
	data += FORMAT_TAG_LEN;

	// etype field, 17 or 18
	p = strchr(data, '$');
	if (!p || p - data != 2)
		return 0;
	type = atoi(data);
	if (type < 17 || type > 18)
		return 0;
	data = p + 1;

	// user field
	p = strchr(data, '$');
	if (!p || p - data > MAX_USERLEN)
		return 0;
	saltlen += p - data;
	data = p + 1;

	// realm field
	p = strchr(data, '$');
	if (!p || p - data > MAX_REALMLEN)
		return 0;
	saltlen += p - data;
	data = p + 1;

	// salt field
	p = strchr(data, '$');
	if (!p)
		return 0;
	// if salt is empty, realm.user is used instead
	if (p - data)
		saltlen = p - data;
	data = p + 1;

	// We support a max. total salt length of 52.
	// We could opt to emit a warning if rejected here.
	if (saltlen > MAX_SALTLEN) {
		static int warned = 0;

		if (!ldr_in_pot)
		if (!warned++)
			fprintf(stderr, "%s: One or more hashes rejected due to salt length limitation\n", FORMAT_LABEL);

		return 0;
	}


	// 56 bytes (112 hex chars) encrypted timestamp + checksum
	if (strlen(data) != 2 * (TIMESTAMP_SIZE + CHECKSUM_SIZE) ||
	    strspn(data, HEXCHARS_all) != strlen(data))
		return 0;

	return 1;
}

static void *get_salt(char *ciphertext)
{
	char *ctcopy = strdup(ciphertext);
	char *keeptr = ctcopy;
	char *p;
	int i;
	static struct custom_salt cs;

	memset(&cs, 0, sizeof(cs));
	ctcopy += FORMAT_TAG_LEN;
	p = strtokm(ctcopy, "$");
	cs.etype = atoi(p);
	p = strtokm(NULL, "$");
	if (p[-1] == '$')
		cs.user[0] = 0;
	else {
		strcpy((char*)cs.user, p);
		p = strtokm(NULL, "$");
	}
	if (p[-1] == '$')
		cs.realm[0] = 0;
	else {
		strcpy((char*)cs.realm, p);
		p = strtokm(NULL, "$");
	}
	if (p[-1] == '$') {
		strcpy((char*)cs.salt, (char*)cs.realm);
		strcat((char*)cs.salt, (char*)cs.user);
	} else {
		strcpy((char*)cs.salt, p);
		p = strtokm(NULL, "$");
	}
	for (i = 0; i < TIMESTAMP_SIZE; i++)
		cs.ct[i] = atoi16[ARCH_INDEX(p[i * 2])] * 16
			+ atoi16[ARCH_INDEX(p[i * 2 + 1])];
	MEM_FREE(keeptr);
	return (void *)&cs;
}

static void set_key(char *key, int index)
{
	int saved_len = strlen(key);
	if (saved_len > PLAINTEXT_LENGTH)
		saved_len = PLAINTEXT_LENGTH;
	memcpy(saved_key[index], key, saved_len);
	saved_key[index][saved_len] = 0;
}

static char *split(char *ciphertext, int index, struct fmt_main *pFmt)
{
	static char out[TOTAL_LENGTH + 1];
	char in[TOTAL_LENGTH + 1];
	char salt[MAX_SALTLEN + 1];
	char *data;
	char *e, *u, *r, *s, *tc;

	strnzcpy(in, ciphertext, sizeof(in));

	tc = strrchr(in, '$'); *tc++ = 0;
	s = strrchr(in, '$'); *s++ = 0;
	r = strrchr(in, '$'); *r++ = 0;
	u = strrchr(in, '$'); *u++ = 0;
	e = in + 8;

	/* Default salt is user.realm */
	if (!*s) {
		snprintf(salt, sizeof(salt), "%s%s", r, u);
		s = salt;
	}
	snprintf(out, sizeof(out), "%s%s$%s$%s$%s$%s", FORMAT_TAG, e, u, r, s, tc);

	data = out + strlen(out) - 2 * (CHECKSUM_SIZE + TIMESTAMP_SIZE) - 1;
	strlwr(data);

	return out;
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

	p = strrchr(ciphertext, '$') + 1 + TIMESTAMP_SIZE * 2; /* skip to checksum field */
	for (i = 0; i < BINARY_SIZE; i++) {
		out[i] =
		    (atoi16[ARCH_INDEX(*p)] << 4) |
		    atoi16[ARCH_INDEX(p[1])];
		p += 2;
	}

	return out;
}

static char *get_key(int index)
{
	return saved_key[index];
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

/* See "pbkdf2_string_to_key" and "krb5int_dk_decrypt" from krb5-1.15.2 software */
static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index = 0;
#ifdef _OPENMP
#pragma omp parallel for
	for (index = 0; index < count; index += MAX_KEYS_PER_CRYPT)
#endif
	{
		unsigned char tkey[MAX_KEYS_PER_CRYPT][32];
		unsigned char base_key[32];
		unsigned char Ke[32];
		unsigned char plaintext[44];
		int key_size, i;
		int len[MAX_KEYS_PER_CRYPT];
#ifdef SIMD_COEF_32
		unsigned char *pin[MAX_KEYS_PER_CRYPT], *pout[MAX_KEYS_PER_CRYPT];
		for (i = 0; i < MAX_KEYS_PER_CRYPT; ++i) {
			len[i] = strlen(saved_key[i+index]);
			pin[i] = (unsigned char*)saved_key[i+index];
			pout[i] = tkey[i];
		}
		pbkdf2_sha1_sse((const unsigned char **)pin, len, cur_salt->salt,strlen((char*)cur_salt->salt), 4096, pout, 32, 0);
#else
		for (i = 0; i < MAX_KEYS_PER_CRYPT; ++i) {
			len[i] = strlen(saved_key[index+i]);
		}
		pbkdf2_sha1((const unsigned char*)saved_key[index], len[0],
		       cur_salt->salt,strlen((char*)cur_salt->salt),
		       4096, tkey[0], 32, 0);
#endif
		for (i = 0; i < MAX_KEYS_PER_CRYPT; ++i) {
			// generate 128 bits from 40 bits of "kerberos" string
			// This is precomputed in init()
			//nfold(8 * 8, (unsigned char*)"kerberos", 128, constant);
			if (cur_salt->etype == 17)
				key_size = 16;
			else
				key_size = 32;

			dk(base_key, tkey[i], key_size, constant, 32);

			/* The "well-known constant" used for the DK function is the key usage number,
			 * expressed as four octets in big-endian order, followed by one octet indicated below.
			 * Kc = DK(base-key, usage | 0x99);
			 * Ke = DK(base-key, usage | 0xAA);
			 * Ki = DK(base-key, usage | 0x55); */

			// derive Ke for decryption/encryption
			// This is precomputed in init()
			//memset(usage, 0, sizeof(usage));
			//usage[3] = 0x01;        // key number in big-endian format
			//usage[4] = 0xAA;        // used to derive Ke

			//nfold(sizeof(usage) * 8, usage, sizeof(ke_input) * 8, ke_input);
			dk(Ke, base_key, key_size, ke_input, 32);

			// decrypt the AS-REQ timestamp encrypted with 256-bit AES
			// here is enough to check the string, further computation below is required
			// to fully verify the checksum
			krb_decrypt(cur_salt->ct, 44, plaintext, Ke, key_size);

			// Check a couple bytes from known plain (YYYYMMDDHHMMSSZ) and
			// bail out if we are out of luck.
			if (plaintext[22] == '2' && plaintext[23] == '0' && plaintext[36] == 'Z') {
				unsigned char Ki[32];
				unsigned char checksum[20];
				// derive Ki used in HMAC-SHA-1 checksum
				// This is precomputed in init()
				//memset(usage, 0, sizeof(usage));
				//usage[3] = 0x01;        // key number in big-endian format
				//usage[4] = 0x55;        // used to derive Ki
				//nfold(sizeof(usage) * 8, usage, sizeof(ki_input) * 8, ki_input);
				dk(Ki, base_key, key_size, ki_input, 32);
				// derive checksum of plaintext
				hmac_sha1(Ki, key_size, plaintext, 44, checksum, 20);
				memcpy(crypt_out[index+i], checksum, BINARY_SIZE);
			} else {
				memset(crypt_out[index+i], 0, BINARY_SIZE);
			}
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

struct fmt_main fmt_krb5pa = {
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
		FMT_CASE | FMT_8_BIT | FMT_SPLIT_UNIFIES_CASE | FMT_OMP,
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
		get_salt,
		{ NULL },
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
