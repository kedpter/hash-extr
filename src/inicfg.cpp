#include "inicfg.h"
#include "common.h"
// #include <sys/file.h>

#define INI_LOAD_VAL(buf, file, title, key, buf_size) \
  if (cpy_inivalue (buf, file, title, key , buf_size) < 0) { \
    printf ("ini load value failed: [title] %s, [key] %s", #title, #key); \
    fclose (tmplock); \
    return -1; \
  }

#define INI_SET_VAL(title, key, buf) \
  if (inifile.SetValue (title, key, buf) < 0) { \
    printf ("ini set value failed: [title] %s, [key] %s", #title, #key); \
    fclose (tmplock); \
    return -1; \
  }

static int ini_file_init (inicfg_ctx_t *inicfg_ctx, const char *filepath)
{
  CSimpleIniA *inifile_ptr = inicfg_ctx->inifile_ptr;

  (*inifile_ptr).SetUnicode ();

  const char *node_info_fpath = filepath;

  FILE *tmplock = inicfg_ctx->tmplock;

  //flock (fileno (tmplock), LOCK_SH);

  SI_Error rc_file = (*inifile_ptr).LoadFile (node_info_fpath);

  if (rc_file < 0)
  {
    printf ("loading failed: %s\n", node_info_fpath);

    //flock (fileno (tmplock), LOCK_UN);
    fclose (tmplock);
    return -1;
  }

  return 1;
}

static void ini_file_destory (inicfg_ctx_t *inicfg_ctx)
{
  FILE *tmplock = inicfg_ctx->tmplock;

  fclose (tmplock);
}

static int cpy_inivalue (char *buffer, CSimpleIniA & inifile, const char *section, const char *key, size_t LIMIT_LEN)
{
  const char *value = inifile.GetValue (section, key);

  if (value && LIMIT_LEN >= strlen (value) + 1)
  {
    strncpy (buffer, value, strlen (value));
  }
  else
  {
    return -1;
  }

  return 1;
}

static int filepath_commasplit (char *comma_string, char buf[256][FILE_PATH_MAXLEN])
{
  int fpath_cnt = 0; int fpath_idx = 0;

  if (strlen(comma_string) > 0)
  {
    fpath_cnt = 1;
  }

  for (size_t i=0; i<strlen(comma_string); i++)
  {
    if (comma_string[i] == ',')
    {
			buf[fpath_cnt-1][fpath_idx] = '\0';

      fpath_idx = 0;
      fpath_cnt++;

      continue;
    }

    buf[fpath_cnt-1][fpath_idx] = comma_string[i];

    fpath_idx++;

		// the end of the comma_string

		if (i == strlen(comma_string) - 1)
		{
			buf[fpath_cnt-1][fpath_idx] = '\0';
		}
  }

  return fpath_cnt;
}


// hash_extr config file

int read_htr_config_file (htr_config_inicfg_t *inicfg, char *htr_config_fpath)
{
  CSimpleIniA inifile;
  inicfg_ctx_t inicfg_ctx;

  FILE *tmplock = fopen (htr_config_fpath, "r");

  inicfg_ctx.inifile_ptr = &inifile;
  inicfg_ctx.tmplock = tmplock;

  ini_file_init (&inicfg_ctx, htr_config_fpath);

  INI_LOAD_VAL (inicfg->wpa2500, inifile, "Output_HCHash_Files", "wpa", BUF_MINLEN);

  INI_LOAD_VAL (inicfg->office9400, inifile, "Output_HCHash_Files", "office2007", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->office9500, inifile, "Output_HCHash_Files", "office2010", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->office9600, inifile, "Output_HCHash_Files", "office2013", BUF_MINLEN);

  INI_LOAD_VAL (inicfg->pdf10400, inifile, "Output_HCHash_Files", "pdf1.1-1.3Acrobat2-4", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->pdf10410, inifile, "Output_HCHash_Files", "pdf1.1-1.3Acrobat2-4collider1", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->pdf10420, inifile, "Output_HCHash_Files", "pdf1.1-1.3Acrobat2-4collider2", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->pdf10500, inifile, "Output_HCHash_Files", "pdf1.4-1.6Acrobat5-8", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->pdf10600, inifile, "Output_HCHash_Files", "pdf1.7Level3Acrobat9", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->pdf10700, inifile, "Output_HCHash_Files", "pdf1.7Level8Acrobat10-11", BUF_MINLEN);

  INI_LOAD_VAL (inicfg->szip11600, inifile, "Output_HCHash_Files", "7zip", BUF_MINLEN);

  INI_LOAD_VAL (inicfg->rar12500, inifile, "Output_HCHash_Files", "rar3", BUF_MINLEN);
  INI_LOAD_VAL (inicfg->rar13000, inifile, "Output_HCHash_Files", "rar5", BUF_MINLEN);

  INI_LOAD_VAL (inicfg->pkzip13600, inifile, "Output_HCHash_Files", "pkzip", BUF_MINLEN);

  SI_Error rc = inifile.SaveFile (htr_config_fpath);

  if (rc < 0)
  {
    //flock (fileno (tmplock), LOCK_UN);
    fclose (tmplock);
    return -1;
  }

  ini_file_destory (&inicfg_ctx);

  return 1;

}
