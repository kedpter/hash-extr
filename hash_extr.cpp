#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <iostream>

#include "types.h"
#include "common.h"
#include "hccvt.h"
#include "inicfg.h"

/**
 * Name........: ini_infor.cpp
 * Author......: kedpter
 * License.....: MIT
 */

using namespace std;

typedef enum htr_op_mode
{
  HTR_OP_MODE_DEFAULT = 0,
  HTR_OP_MODE_SINGLE = 1,
  HTR_OP_MODE_CONFIG = 10,
} htr_op_mode_t;


static void print_usage ()
{
  /* printf ("Usage: hash_extr -t task-type -f orig-file -o hash-file --hash-type\n"); */

  printf ("Usage: hash_extr -s op file...[MAX:256] [options] \n\n" "support: office pdf szip rar pkzip \n " "\n" "-s select operation: \n " "\t0      [default mode]extract hash, print to the console\n" "\t1      [single mode]extract from single file, then rename[-o]\n"
          // "1 extract tbc_files to [hchash | ihchash] file"
          // "1 extract single type[same version] of files \n "
          // "2 extract mixed types of files \n "
          // "5 category hchash file by hash_mode [one hash one line] \n "
          // "6 category ihchash file by hash_mode [full hash info] \n "
          "\t10     [config mode]category by config file\n" "");
}

static int rfile_init (htr_ctx_t * htr_ctx)
{
  rfile_info_ctx_t *rfile_info_ctx = htr_ctx->rfile_info_ctx;

  rfile_info_ctx->hash_ctx = (hash_ctx_t *) jmmalloc (sizeof (hash_ctx_t));

  rfile_info_ctx->file_encryption = FILE_ENCRYPTION_UNKNOWN;

  memset (rfile_info_ctx->tmp_fpath, 0, sizeof (rfile_info_ctx->tmp_fpath));
  memset (rfile_info_ctx->type, 0, sizeof (rfile_info_ctx->type));
  memset (rfile_info_ctx->version, 0, sizeof (rfile_info_ctx->version));
  memset (rfile_info_ctx->path, 0, sizeof (rfile_info_ctx->path));

  strcpy (rfile_info_ctx->tmp_fpath, "extract_hchash_tmp.txt");

  hash_ctx_init (rfile_info_ctx->hash_ctx);

  return 1;
}

static void rfile_destory (htr_ctx_t * htr_ctx)
{
  rfile_info_ctx_t *rfile_info_ctx = htr_ctx->rfile_info_ctx;

  jmfree (rfile_info_ctx->hash_ctx);

  hash_ctx_destory (rfile_info_ctx->hash_ctx);
}

static int htr_user_options_init (htr_ctx_t * htr_ctx)
{
  htr_user_options_t *user_options = htr_ctx->user_options;

  user_options->hash_mode = -1;

  user_options->config_fpath = NULL;

  user_options->tbc_fpaths_cnt = 0;
  user_options->tbc_fpaths = (char **) jmcalloc (256, sizeof (char *));

  return 1;
}

static void htr_user_options_destory (htr_ctx_t * htr_ctx)
{
  htr_user_options_t *user_options = htr_ctx->user_options;

  jmfree (user_options->tbc_fpaths);
}

static int htr_init (htr_ctx_t * htr_ctx)
{
  htr_ctx->rfile_info_ctx = (rfile_info_ctx_t *) jmmalloc (sizeof (rfile_info_ctx_t));
  htr_ctx->user_options = (htr_user_options_t *) jmmalloc (sizeof (htr_user_options_t));

  return 1;
}

static int htr_destory (htr_ctx_t * htr_ctx)
{
  free (htr_ctx->rfile_info_ctx);

  return 1;
}

static int htr_session_init (htr_ctx_t * htr_ctx)
{
  htr_ctx->log_fpath = "hash_extr.log";
  htr_ctx->valid_hashes_cnt = 0;

  rfile_init (htr_ctx);

  return 1;
}

static int htr_session_destory (htr_ctx_t * htr_ctx)
{
  rfile_destory (htr_ctx);

  return 1;
}

static int user_options_get_opt (htr_ctx_t * htr_ctx, int argc, char **argv)
{
  int c = 0;

  int option_index = 0;

  htr_user_options_t *user_options = htr_ctx->user_options;

  static struct option long_options[] = {
    {"selectop", required_argument, 0, 's'},
    {"configfile", required_argument, 0, 'c'},
    {"outputfile", required_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    // {"hashmode", required_argument, 0, 'm'},
    // {"unftdhashfile", required_argument, 0, 'u'},
    // {"outfile", required_argument, 0, 'o'},
    // {"fullinfo", no_argument, 0, 'i'},
    {0, 0, 0, 0}
  };


  while ((c = getopt_long (argc, argv, "s:c:o:h", long_options, &option_index)) != -1)
  {
    switch (c)
    {
    case 's':
      user_options->op_mode = atoi (optarg);
      break;
    case 'c':
      user_options->config_fpath = optarg;
      break;
    case 'o':
      user_options->out_fpath = optarg;
      break;
    case 'h':
      user_options->usage = true;
      break;
    default:
      user_options->usage = true;
      return EXIT_FAILURE;
    }
  }

  if (user_options->usage)
  {
    print_usage();
  }

  for (option_index = optind; option_index < argc; option_index++)
  {
    // is path valid

    if (is_file_exist (argv[option_index]) == -1)
    {
      fprintf (stderr, "%s: file does not exist, see --help\n", argv[option_index]);

      exit (EXIT_FAILURE);
    }

    user_options->tbc_fpaths[user_options->tbc_fpaths_cnt++] = argv[option_index];

    // printf ("tbc_paths: %s\n", argv[option_index]);
  }

  // printf ("user_options %d \n", user_options->op_mode);

  if (user_options->op_mode != 0 && user_options->op_mode != 1 && user_options->op_mode != 10)
  {
    fprintf (stderr, "see --help\n");

    exit (EXIT_FAILURE);
  }

  if (user_options->tbc_fpaths_cnt == 0)
  {
    fprintf (stderr, "please specify at least one tbc file, see --help\n");

    exit (EXIT_FAILURE);
  }

  if (user_options->op_mode == 1)
  {

  }
  else
  {

  }


  return 1;
}

static int extract_hash_by_config (htr_ctx_t * htr_ctx)
{
  htr_user_options *user_options = htr_ctx->user_options;

  rfile_info_ctx_t *rfile_info_ctx = htr_ctx->rfile_info_ctx;

  hash_ctx_t *hash_ctx = rfile_info_ctx->hash_ctx;

  // config mode logging

  FILE *log_fp = fopen (htr_ctx->log_fpath, "a");

  if (log_fp == NULL)
  {
    fprintf (stderr, "%s: open file failed \n", htr_ctx->log_fpath);

    return -1;
  }

  // read config

  htr_config_inicfg_t htr_config_inicfg;

  memset (&htr_config_inicfg, 0, sizeof (htr_config_inicfg_t));

  read_htr_config_file (&htr_config_inicfg, user_options->config_fpath);

  size_t tbc_file_idx = 0;

  for (tbc_file_idx = 0; tbc_file_idx < user_options->tbc_fpaths_cnt; tbc_file_idx++)
  {
    strcpy (rfile_info_ctx->path, user_options->tbc_fpaths[tbc_file_idx]);

    hash_ctx_init (rfile_info_ctx->hash_ctx);

    // get vague mode by checking file header

    if (get_rw_rfile_ftype (rfile_info_ctx) == -1)
    {
      fprintf (stderr, "%s: convert failed, not supported file type, skip\n", rfile_info_ctx->path);
      fprintf (log_fp, "%s: convert failed, not supported file type, skip\n", rfile_info_ctx->path);

      continue;
    }

    if (extract_hchash_vaguemode (rfile_info_ctx) == -1)
    {
      fprintf (stderr, "%s: convert failed, extract_hchash_vaguemode failed, skip", rfile_info_ctx->path);
      fprintf (log_fp, "%s: convert failed, extract_hchash_vaguemode failed, skip", rfile_info_ctx->path);

      continue;
    }

    if (rfile_info_ctx->file_encryption == FILE_UNENCRYPTED)
    {
      fprintf (stderr, "%s: convert failed, file length 0, might be unencrypted, skip\n", rfile_info_ctx->path);
      fprintf (log_fp, "%s: convert failed, file length 0, might be unencrypted, skip\n", rfile_info_ctx->path);

      continue;
    }

    char *out_fpath = NULL;

    switch (hash_ctx->hash_mode)
    {
    case 2500:
      out_fpath = htr_config_inicfg.wpa2500;
      break;

    case 9400:
      out_fpath = htr_config_inicfg.office9400;
      break;
    case 9500:
      out_fpath = htr_config_inicfg.office9500;
      break;
    case 9600:
      out_fpath = htr_config_inicfg.office9600;
      break;

    case 10400:
      out_fpath = htr_config_inicfg.pdf10400;
      break;
    case 10410:
      out_fpath = htr_config_inicfg.pdf10410;
      break;
    case 10420:
      out_fpath = htr_config_inicfg.pdf10420;
      break;
    case 10500:
      out_fpath = htr_config_inicfg.pdf10500;
      break;
    case 10600:
      out_fpath = htr_config_inicfg.pdf10600;
      break;
    case 10700:
      out_fpath = htr_config_inicfg.pdf10700;
      break;

    case 11600:
      out_fpath = htr_config_inicfg.szip11600;
      break;
    case 12500:
      out_fpath = htr_config_inicfg.rar12500;
      break;
    case 13000:
      out_fpath = htr_config_inicfg.rar13000;
      break;
    case 13600:
      out_fpath = htr_config_inicfg.pkzip13600;
      break;
    default:
      break;
    }

    // write to file

    if (out_fpath == NULL)
    {
      fprintf (stderr, "%s: convert failed, out_fpath == NULL, skip\n", rfile_info_ctx->path);
      fprintf (log_fp, "%s: convert failed, out_fpath == NULL, skip\n", rfile_info_ctx->path);

      continue;
    }

    FILE *out = fopen (out_fpath, "ab");

    if (out == NULL)
    {
      fprintf (stderr, "%s: convert failed, open file failed, skip\n", rfile_info_ctx->path);
      fprintf (log_fp, "%s: convert failed, open file[%s] failed, skip\n", rfile_info_ctx->path, out_fpath);

      continue;
    }

    hash_ctx_t *hash_ctx = rfile_info_ctx->hash_ctx;

    if (hash_ctx->len > 0)
    {
      fwrite (hash_ctx->hash_val, hash_ctx->len, 1, out);

      if (hash_ctx->hash_mode != 2500)
      {
        fprintf (out, EOL);
      }

      printf ("%s: convert success !\n", rfile_info_ctx->path);

      fprintf (log_fp, "%s: convert success ! [%d] [len: %d] [", rfile_info_ctx->path, hash_ctx->hash_mode, hash_ctx->len);
      fwrite (hash_ctx->hash_val, hash_ctx->len, 1, log_fp);
      fprintf (log_fp, "]\n");

      htr_ctx->valid_hashes_cnt++;
    }
    else
    {
      fprintf (stderr, "%s: convert failed, hash_len <= 0 \n, skip", rfile_info_ctx->path);
      fprintf (log_fp, "%s: convert failed, hash_len <= 0 \n, skip", rfile_info_ctx->path);
    }

    fclose (out);

    hash_ctx_destory (rfile_info_ctx->hash_ctx);
  }

  printf ("[hash_extr]: converted %d/%d hashes\n", htr_ctx->valid_hashes_cnt, htr_ctx->user_options->tbc_fpaths_cnt);

  fclose (log_fp);

  return 1;
}

static int extract_hash_by_single (htr_ctx_t * htr_ctx)
{
  htr_user_options *user_options = htr_ctx->user_options;

  rfile_info_ctx_t *rfile_info_ctx = htr_ctx->rfile_info_ctx;

  strcpy (rfile_info_ctx->path, user_options->tbc_fpaths[0]);

  // get vague mode by checking file header

  if (get_rw_rfile_ftype (rfile_info_ctx) == -1)
  {
    exit (EXIT_FAILURE);
  }

  extract_hchash_vaguemode (rfile_info_ctx);

  // write to file

  FILE *out = fopen (user_options->out_fpath, "w");

  if (out == NULL)
  {
    fprintf (stderr, "%s: open file failed \n", user_options->out_fpath);

    exit (EXIT_FAILURE);
  }

  hash_ctx_t *hash_ctx = rfile_info_ctx->hash_ctx;

  fwrite (hash_ctx->hash_val, hash_ctx->len, 1, out);

  fclose (out);

  return 1;
}

static int extract_hash_by_default (htr_ctx_t * htr_ctx)
{
  htr_user_options *user_options = htr_ctx->user_options;

  rfile_info_ctx_t *rfile_info_ctx = htr_ctx->rfile_info_ctx;

  size_t tbc_file_idx = 0;

  for (tbc_file_idx = 0; tbc_file_idx < user_options->tbc_fpaths_cnt; tbc_file_idx++)
  {
    strcpy (rfile_info_ctx->path, user_options->tbc_fpaths[tbc_file_idx]);

    char *tbc_fpath_nos = get_filename_nosuffix (rfile_info_ctx->path);


    if (get_rw_rfile_ftype (rfile_info_ctx) == -1)
    {
      fprintf (stderr, "%s: not supported file type\n", rfile_info_ctx->path);

      continue;
    }

    extract_hchash_vaguemode (rfile_info_ctx);

    // write to stdout

    hash_ctx_t *hash_ctx = rfile_info_ctx->hash_ctx;

    fwrite (hash_ctx->hash_val, hash_ctx->len, 1, stdout);
    printf("\n");

    free (tbc_fpath_nos);
  }

  return 1;
}

static int htr_session_execute (htr_ctx_t * htr_ctx)
{
  htr_user_options_t *user_options = htr_ctx->user_options;

  rfile_info_ctx_t *rfile_info_ctx = htr_ctx->rfile_info_ctx;

  // category all rw_files by reading type-out_fpath config file

  if (user_options->op_mode == 0)
  {
    extract_hash_by_default (htr_ctx);
  }

  if (user_options->op_mode == 1)
  {
    if (user_options->tbc_fpaths_cnt > 1)
    {
      fprintf (stderr, "[single mode]: too many tbc files\n");

      exit (EXIT_FAILURE);
    }

    if (user_options->out_fpath == NULL)
    {
      fprintf (stderr, "[single mode]: -o output_file is required\n");

      exit (EXIT_FAILURE);
    }
    extract_hash_by_single (htr_ctx);
  }

  if (user_options->op_mode == 10)
  {
    if (user_options->config_fpath == NULL)
    {
      fprintf (stderr, "[config mode]: -c config_file is required\n");

      exit (EXIT_FAILURE);
    }
    extract_hash_by_config (htr_ctx);
  }

  return 1;
}

int main (int argc, char **argv)
{
  htr_ctx_t htr_ctx;

  // init struct

  htr_init (&htr_ctx);

  // get opt

  htr_user_options_init (&htr_ctx);

  user_options_get_opt (&htr_ctx, argc, argv);

  // choose op extract hashes

  // use sscanf for each line
  htr_session_init (&htr_ctx);

  htr_session_execute (&htr_ctx);

  htr_session_destory (&htr_ctx);
  htr_user_options_destory (&htr_ctx);

  htr_destory (&htr_ctx);

  exit (0);

}
