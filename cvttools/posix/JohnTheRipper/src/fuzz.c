/*
 * This file is part of John the Ripper password cracker,
 * Copyright (c) 2015 by Kai Zhao
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */

#include "os.h"

#include <sys/stat.h>

#if _MSC_VER || __MINGW32__ || __MINGW64__ || __CYGWIN__ || HAVE_WINDOWS_H
#include "win32_memmap.h"
#undef MEM_FREE
#if !defined(__CYGWIN__) && !defined(__MINGW64__)
#include "mmap-windows.c"
#endif /* __CYGWIN */
#endif /* _MSC_VER ... */

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#include <io.h> /* mingW _mkdir */
#endif

#if defined(HAVE_MMAP)
#include <sys/mman.h>
#endif

#include "jumbo.h"
#include "misc.h"	// error()
#include "config.h"
#include "john.h"
#include "params.h"
#include "signals.h"
#include "unicode.h"

#define _STR_VALUE(arg)         #arg
#define STR_MACRO(n)            _STR_VALUE(n)

#define CHAR_FROM -128
#define CHAR_TO 127

// old value from params.h
#define FUZZ_LINE_BUFFER_SIZE 0x30000
static char fuzz_hash[FUZZ_LINE_BUFFER_SIZE];
static char status_file_path[PATH_BUFFER_SIZE + 1];

struct FuzzDic {
	struct FuzzDic *next;
	char *value;
};

static struct FuzzDic *rfd;

static FILE *s_file; // Status file which is ./fuzz_status/'format->params.label'

extern int pristine_gecos;
extern int single_skip_login;

static char *file_pos, *file_end;

static char *get_line()
{
	char *new_line, *line_start;

	line_start = file_pos;
	while (file_pos < file_end && *file_pos != '\n')
		file_pos++;

	if (file_pos == file_end)
		return NULL;
	file_pos++;

	new_line = mem_alloc(file_pos - line_start);
	strncpy(new_line, line_start, file_pos - line_start);
	new_line[file_pos - line_start - 1] = 0;

	return new_line;
}

static void fuzz_init_dictionary()
{
	FILE *file;
	char *line;
	struct FuzzDic *last_fd, *pfd;
	int64_t file_len = 0;
#ifdef HAVE_MMAP
	char *mem_map;
#else
	char *file_start;
#endif

	if (!options.fuzz_dic)
		return;

	if (!(file = jtr_fopen(options.fuzz_dic, "r")))
		pexit("fopen: %s", options.fuzz_dic);

	jtr_fseek64(file, 0, SEEK_END);
	if ((file_len = jtr_ftell64(file)) == -1)
		pexit(STR_MACRO(jtr_ftell64));
	jtr_fseek64(file, 0, SEEK_SET);
	if (file_len == 0) {
		if (john_main_process)
			fprintf(stderr, "Error, dictionary file is empty\n");
		error();
	}

#ifdef HAVE_MMAP
	mem_map = MAP_FAILED;
	if (file_len < ((1LL)<<32))
		mem_map = mmap(NULL, file_len, PROT_READ, MAP_SHARED,
			fileno(file), 0);
	if (mem_map == MAP_FAILED) {
		mem_map = NULL;
		fprintf(stderr, "fuzz: memory mapping failed (%s)\n",
		        strerror(errno));
		error();
	} else {
		file_pos = mem_map;
		file_end = mem_map + file_len;
	}
#else
	file_pos = file_start = mem_alloc(file_len);
	file_end = file_start + file_len;
	if (fread(file_pos, 1, (size_t)file_len, file) != file_len) {
		if (ferror(file))
			pexit("fread");
		fprintf(stderr, "fread: Unexpected EOF\n");
		error();
	}
#endif

	rfd = mem_alloc(sizeof(struct FuzzDic));
	rfd->next = NULL;
	last_fd = rfd;

	while ((line = get_line()) != NULL) {
		pfd = mem_alloc(sizeof(struct FuzzDic));
		pfd->next = NULL;
		pfd->value = line;
		last_fd->next = pfd;
		last_fd = pfd;
	}

#ifdef HAVE_MMAP
	if (mem_map)
		munmap(mem_map, file_len);
#else
	MEM_FREE(file_start);
#endif
	file_pos = file_end = NULL;

	if (ferror(file)) pexit("fgets");

	if (fclose(file)) pexit("fclose");
}

// Replace chars with '9$*#'
static char * replace_each_chars(char *ciphertext, int *is_replace_finish)
{
	static int replaced_chars_index = 0;
	static int cipher_index = 0;
	static char replaced_chars[5] = "\xFF" "9$*#";

	while (replaced_chars_index < sizeof(replaced_chars)) {
		if (ciphertext[cipher_index] != replaced_chars[replaced_chars_index]) {
			fuzz_hash[cipher_index] = replaced_chars[replaced_chars_index];
			replaced_chars_index++;
			return fuzz_hash;
		}
		replaced_chars_index++;
	}
	if (replaced_chars_index == sizeof(replaced_chars)) {
		replaced_chars_index = 0;
		cipher_index++;
	}
	if (cipher_index >= strlen(ciphertext)) {
		*is_replace_finish = 1;
		cipher_index = 0;
		replaced_chars_index = 0;
		return NULL;
	} else {
		while (replaced_chars_index < sizeof(replaced_chars)) {
			if (ciphertext[cipher_index] != replaced_chars[replaced_chars_index]) {
				fuzz_hash[cipher_index] = replaced_chars[replaced_chars_index];
				replaced_chars_index++;
				return fuzz_hash;
			}
			replaced_chars_index++;
		}
	}
	// It will never reach here
	return NULL;
}

// Swap two adjacent chars
// e.g
// ABCDE -> BACDE, ACBDE, ABDCE, ABCED
static char * swap_chars(char *origin_ctext, int *is_swap_finish)
{
	static int cipher_index = 1;

	while (cipher_index < strlen(fuzz_hash)) {
		if (origin_ctext[cipher_index - 1] != origin_ctext[cipher_index]) {
			fuzz_hash[cipher_index - 1] = origin_ctext[cipher_index];
			fuzz_hash[cipher_index] = origin_ctext[cipher_index - 1];
			cipher_index++;
			return fuzz_hash;
		}
		cipher_index++;
	}

	cipher_index = 1;
	*is_swap_finish = 1;
	return NULL;
}

// Append times of the last char
// times: 1, 2, 6, 42, 1806
static char * append_last_char(char *origin_ctext, int *is_append_finish)
{
	static int times = 1;
	static int i = 0;
	int origin_ctext_len = 0;
	int append_len = 0;

	origin_ctext_len = strlen(origin_ctext);

	if (origin_ctext_len == 0 || i == 5) {
		times = 1;
		i = 0;
		*is_append_finish = 1;
		return NULL;
	}

	if (origin_ctext_len + times < FUZZ_LINE_BUFFER_SIZE)
		append_len = times;
	else
		append_len = FUZZ_LINE_BUFFER_SIZE - origin_ctext_len - 1;

	memset(fuzz_hash + origin_ctext_len, origin_ctext[origin_ctext_len - 1], append_len);
	fuzz_hash[origin_ctext_len + append_len] = 0;

	i++;
	times *= times + 1;

	return fuzz_hash;
}

// Change hash cases, such as "abcdef" -> "Abcdef"
static char * change_case(char *origin_ctext, int *is_chgcase_finish)
{
	char c;
	char *pc;
	static int flag = 2;
	static int cipher_index = 0;

	while (origin_ctext[cipher_index]) {
		c = origin_ctext[cipher_index];
		if ('a' <= c && 'z' >= c) {
			fuzz_hash[cipher_index] = c - 'a' + 'A';
			cipher_index++;
			return fuzz_hash;
		} else if ('A' <= c && 'Z' >= c) {
			fuzz_hash[cipher_index] = c - 'A' + 'a';
			cipher_index++;
			return fuzz_hash;
		}
		cipher_index++;
	}

	if (flag == 2) {
		// Change all to upper cases
		pc = fuzz_hash;
		while (*pc) {
			if ('a' <= *pc && 'z' >= *pc)
				*pc = *pc - 'a' + 'A';
			pc++;
		}

		flag--;
		return fuzz_hash;
	} else if (flag == 1) {
		// Change all to lower cases
		pc = fuzz_hash;
		while (*pc) {
			if ('A' <= *pc && 'Z' >= *pc)
				*pc = *pc - 'A' + 'a';
			pc++;
		}

		flag--;
		return fuzz_hash;
	}

	flag = 2;
	cipher_index = 0;
	*is_chgcase_finish = 1;
	return NULL;
}

// Insert str before pos in origin_ctext, and copy the result
// to out
static void insert_str(char *origin_ctext, int pos, char *str, char *out)
{
	const int origin_ctext_len = strlen(origin_ctext);
	int str_len = strlen(str);

	if (str_len + origin_ctext_len >= FUZZ_LINE_BUFFER_SIZE)
		str_len = FUZZ_LINE_BUFFER_SIZE - origin_ctext_len - 1;

	strncpy(out, origin_ctext, pos);
	strncpy(out + pos, str, str_len);
	strcpy(out + pos + str_len, origin_ctext + pos);
}

// Insert strings from dictionary before each char
static char * insert_dic(char *origin_ctext, int *is_insertdic_finish)
{
	static int flag = 0;
	static struct FuzzDic *pfd = NULL;
	static int index = 0;
	static int flag_long = 0;

	if (!options.fuzz_dic)
		return NULL;

	if (!flag) {
		pfd = rfd->next;
		flag = 1;
	}

	if (!pfd) {
		flag = 0;
		*is_insertdic_finish = 1;
		return NULL;
	}

	if (100000 > strlen(origin_ctext)) {
		// Insert strings before each char
		insert_str(origin_ctext, index++, pfd->value, fuzz_hash);
		if (index >= strlen(origin_ctext) + 1) {
			index = 0;
			pfd = pfd->next;
		}
	} else {
		// Insert strings before and after these chars: ",.:#$*@"
		while (index < strlen(origin_ctext)) {
			switch (origin_ctext[index]) {
			case ',':
			case '.':
			case ':':
			case '#':
			case '$':
			case '*':
			case '@':
				if (!flag_long) {
					insert_str(origin_ctext, index, pfd->value, fuzz_hash);
					flag_long = 1;
				} else {
					insert_str(origin_ctext, index + 1, pfd->value, fuzz_hash);
					flag_long = 0;
					index++;
				}
			default:
				index++;
				break;
			}
		}
		if (index >= strlen(origin_ctext)) {
			index = 0;
			pfd = pfd->next;
		}
	}

	return fuzz_hash;
}

// Insert str before pos in origin_ctext, and copy the result
// to out
static void insert_char(char *origin_ctext, int pos, char c, int size, char *out)
{
	const int origin_ctext_len = strlen(origin_ctext);

	if (size + origin_ctext_len >= FUZZ_LINE_BUFFER_SIZE)
		size = FUZZ_LINE_BUFFER_SIZE- origin_ctext_len - 1;

	strncpy(out, origin_ctext, pos);
	memset(out + pos, c, size);
	strcpy(out + pos + size, origin_ctext + pos);
}

// Insert chars from -128 to 127
static char * insert_chars(char *origin_ctext, int *is_insertchars_finish)
{
	static int oc_index = 0;
	static int c_index = CHAR_FROM;
	static int flag_long = 0;
	static int times[5] = { 1, 10, 100, 1000, 10000 };
	static int times_index = 0;

//printf("%s:%d %s(oc='%s', times_index=%d, c_index=%d, oc_index=%d)\n",
//	__FILE__, __LINE__, __FUNCTION__, origin_ctext,
//	times_index, c_index, oc_index);

	if (times_index > 4) {
		times_index = 0;
		c_index++;
		if (c_index > CHAR_TO) {
			c_index = CHAR_FROM;
			oc_index++;
			flag_long = 0;
			if (oc_index > strlen(origin_ctext)) {
				oc_index = 0;
				*is_insertchars_finish = 1;
				return NULL;
			}
		}
	}

	if (100000 > strlen(origin_ctext)) {
		// Insert chars before each char
		insert_char(origin_ctext, oc_index, (char)c_index, times[times_index++], fuzz_hash);
	} else {
		// Insert chars before and after these chars: ",.:#$*"
		while (oc_index < strlen(origin_ctext)) {
			switch (origin_ctext[oc_index]) {
			case ',':
			case '.':
			case ':':
			case '#':
			case '$':
			case '*':
				if (!flag_long) {
					insert_char(origin_ctext, oc_index, (char)c_index, times[times_index], fuzz_hash);
					flag_long = 1;
				} else {
					insert_char(origin_ctext, oc_index + 1, (char)c_index, times[times_index], fuzz_hash);
					times_index++;
					flag_long = 0;
				}
				return fuzz_hash;
			default:
				oc_index++;
				break;
			}
		}
		oc_index = 0;
		c_index = CHAR_FROM;
		flag_long = 0;
		times_index = 0;
		return NULL;
	}

	return fuzz_hash;
}

static char * get_next_fuzz_case(char *label, char *ciphertext)
{
	static int is_replace_finish = 0; // is_replace_finish = 1 if all the replaced cases have been generated
	static int is_swap_finish = 0; // is_swap_finish = 1 if all the swaped cases have been generated
	static int is_append_finish = 0; // is_append_finish = 1 if all the appended cases have been generated
	static int is_chgcase_finish = 0; // is_chgcase_finish = 1 if all the change cases have been generated
	static int is_insertdic_finish = 0; // is_insertdic_finish = 1 if all the insert dictionary cases have been generated
	static int is_insertchars_finish = 0; // is_insertchars_finish = 1 if all the chars from -128 to 127 cases have been generated
	static char *last_label = NULL, *last_ciphertext = NULL;

	if (strlen(ciphertext) > FUZZ_LINE_BUFFER_SIZE) {
		fprintf(stderr, "ciphertext='%s' is bigger than the FUZZ_LINE_BUFFER_SIZE=%d\n",
			ciphertext, FUZZ_LINE_BUFFER_SIZE);
		error();
	}
	strcpy(fuzz_hash, ciphertext);

	if (!last_label)
		last_label = label;

	if (!last_ciphertext)
		last_ciphertext = ciphertext;

	if (strcmp(label, last_label) != 0 || strcmp(ciphertext, last_ciphertext) != 0) {
		is_replace_finish = 0;
		is_swap_finish = 0;
		is_append_finish = 0;
		is_chgcase_finish = 0;
		is_insertdic_finish = 0;
		is_insertchars_finish = 0;
		last_label = label;
		last_ciphertext = ciphertext;
	}

	if (!is_replace_finish)
		if (replace_each_chars(ciphertext, &is_replace_finish))
			return fuzz_hash;

	if (!is_swap_finish)
		if (swap_chars(ciphertext, &is_swap_finish))
			return fuzz_hash;

	if (!is_append_finish)
		if (append_last_char(ciphertext, &is_append_finish))
			return fuzz_hash;

	if (!is_chgcase_finish)
		if (change_case(ciphertext, &is_chgcase_finish))
			return fuzz_hash;

	if (!is_insertdic_finish)
		if (insert_dic(ciphertext, &is_insertdic_finish))
			return fuzz_hash;

	if (!is_insertchars_finish)
		if (insert_chars(ciphertext, &is_insertchars_finish))
			return fuzz_hash;

	return NULL;
}

static void init_status(char *format_label)
{
	sprintf(status_file_path, "%s", "fuzz_status");
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
	if (_mkdir(status_file_path)) { // MingW
#else
	if (mkdir(status_file_path, S_IRUSR | S_IWUSR | S_IXUSR)) {
#endif
		if (errno != EEXIST) pexit("mkdir: %s", status_file_path);
	} else
		fprintf(stderr, "Created directory: %s\n", status_file_path);

	sprintf(status_file_path, "%s/%s", "fuzz_status", format_label);
	if (!(s_file = fopen(status_file_path, "w")))
		pexit("fopen: %s", status_file_path);
}

static void save_index(const int index)
{
	fprintf(s_file, "%d\n", index);
	fflush(s_file);
}

static void fuzz_test(struct db_main *db, struct fmt_main *format)
{
	int index;
	char *ret, *line;
	struct fmt_tests *current;

	printf("Fuzzing: %s%s%s%s [%s]%s... ",
	       format->params.label,
	       format->params.format_name[0] ? ", " : "",
	       format->params.format_name,
	       format->params.benchmark_comment,
	       format->params.algorithm_name,
#ifndef BENCH_BUILD
	       (options.target_enc == UTF_8 &&
	       format->params.flags & FMT_UNICODE) ?
	       " in UTF-8 mode" : "");
#else
	       "");
#endif
	fflush(stdout);


	// validate that there are no NULL function pointers
	if (format->methods.prepare == NULL)    return;
	if (format->methods.valid == NULL)      return;
	if (format->methods.split == NULL)      return;
	if (format->methods.init == NULL)       return;

	index = 0;
	current = format->params.tests;

	init_status(format->params.label);
	db->format = format;

	while (1) {
		ret = get_next_fuzz_case(format->params.label, current->ciphertext);
		save_index(index++);
		line = fuzz_hash;
		db->format = format;
		ldr_load_pw_line(db, line);

		if (!ret) {
			if (!(++current)->ciphertext)
				break;
		}
	}
	if (fclose(s_file)) pexit("fclose");
	remove(status_file_path);
	printf("   Completed\n");
}

// Dump fuzzed hashes which index is between from and to, including from and excluding to
static void fuzz_dump(struct fmt_main *format, const int from, const int to)
{
	int index;
	char *ret;
	struct fmt_tests *current;
	char file_name[PATH_BUFFER_SIZE];
	FILE *file;
	size_t len = 0;

	sprintf(file_name, "pwfile.%s", format->params.label);

	printf("Generating %s for %s%s%s%s ... ",
	       file_name,
	       format->params.label,
	       format->params.format_name[0] ? ", " : "",
	       format->params.format_name,
	       format->params.benchmark_comment);
	fflush(stdout);

	if (!(file = fopen(file_name, "w")))
		pexit("fopen: %s", file_name);

	index = 0;
	current = format->params.tests;

	while (1) {
		ret = get_next_fuzz_case(format->params.label, current->ciphertext);
		if (index >= from) {
			if (index == to)
				break;
			fprintf(file, "%s\n", fuzz_hash);
			len += 1 + strlen(fuzz_hash);
		}
		index++;
		if (!ret) {
			if (!(++current)->ciphertext)
				break;
		}
	}
	printf(LLu" bytes\n", (unsigned long long) len);
	if (fclose(file)) pexit("fclose");
}


int fuzz(struct db_main *db)
{
	char *p;
	int from, to;
	unsigned int total;
	struct fmt_main *format;

	pristine_gecos = cfg_get_bool(SECTION_OPTIONS, NULL,
	        "PristineGecos", 0);
	single_skip_login = cfg_get_bool(SECTION_OPTIONS, NULL,
		"SingleSkipLogin", 0);

	if (options.flags & FLG_FUZZ_DUMP_CHK) {
		from = -1;
		to = -1;

		if (options.fuzz_dump) {
			p = strtok(options.fuzz_dump, ",");
			if (p) {
				sscanf(p, "%d", &from);
				p = strtok(NULL, ",");

				if (p)
					sscanf(p, "%d", &to);
			}
		}
		if (from > to) {
			fprintf(stderr, "--fuzz-dump from=%d is bigger than to=%d\n",
				from, to);
			error();
		}
	}

	fuzz_init_dictionary();

	total = 0;
	if ((format = fmt_list))
	do {
/* Silently skip formats for which we have no tests, unless forced */
		if (!format->params.tests && format != fmt_list)
			continue;

		if (options.flags & FLG_FUZZ_DUMP_CHK)
			fuzz_dump(format, from, to);
		else
			fuzz_test(db, format);

		total++;
	} while ((format = format->next) && !event_abort);

	if (options.flags & FLG_FUZZ_DUMP_CHK)
		printf("Generated pwfile.<format> for %u formats\n", total);
	else
		printf("All %u formats passed fuzzing test!\n", total);

	return 0;
}
