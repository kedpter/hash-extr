#ifndef _INICFG_H
#define _INICFG_H

#include <inttypes.h>

#include "ini/SimpleIni.h"
#include "types.h"
#include "common.h"

struct crack_info_inicfg {
  char is_raw_file_encrypted[BUF_MINLEN];
  char hash_type[BUF_MINLEN];
  char rawfile_type[BUF_MINLEN];
  char rawfile_subversion[BUF_MINLEN];
  char hash_mode[BUF_MINLEN];
  char hash_val[BUF_MAXLEN];
  char hash_len[BUF_MINLEN];
};

typedef struct crack_info_inicfg crack_info_inicfg_t;

struct inicfg_ctx {
  CSimpleIniA *inifile_ptr;
  FILE *tmplock;
};

typedef struct inicfg_ctx inicfg_ctx_t;

int read_htr_config_file (htr_config_inicfg_t *inicfg, char *htr_config_fpath);

#endif
