#include "hccvt.h"

static void remove_colon (char *src_file_buffer, char *des_file_buffer, int *hchash_len)
{
  char *find_position = src_file_buffer;

  int prefix_fname_len = 0;

  while ((find_position = strchr (find_position, ':')) != NULL)
  {
    find_position++;

    if ((*find_position) == '$')
    {
      prefix_fname_len = find_position - src_file_buffer;

      *hchash_len = strlen (src_file_buffer) - prefix_fname_len;

      memcpy (des_file_buffer, find_position, *hchash_len);
      // memcpy (des_file_buffer, find_position, *file_len - (find_position - src_file_buffer));
      // *file_len -= (find_position - src_file_buffer);

      break;
    }
  }
}

static int office_vague2exp_mode (char *version, int *hash_mode)
{
  if (strcmp (version, "2007") == 0)
  {
    *hash_mode = 9400;
  }
  else if (strcmp (version, "2010") == 0)
  {
    *hash_mode = 9500;
  }
  else if (strcmp (version, "2013") == 0)
  {
    *hash_mode = 9600;
  }
  else
  {
    printf ("office version error\n");

    return -1;
  }
  return 0;
}

static void remove_double_colon (char *src_file_buffer, char *des_file_buffer, int *hchash_len)
{
  char *find_position = src_file_buffer;

  int len1 = 0, len2 = 0;

  int prefix_fname_len = 0;

  while ((find_position = strchr (find_position, ':')) != NULL)
  {
    find_position++;

    if ((*find_position) == '$')
    {
      break;
    }
  }

  prefix_fname_len = find_position - src_file_buffer;

  len1 = strlen (find_position);

  if ((find_position = strchr (find_position, ':')) != NULL)
    len2 = strlen (find_position);

  *hchash_len = len1 - len2;

  // memcpy (des_file_buffer, src_file_buffer + *file_len - len1 + 1, len1 - len2);
  memcpy (des_file_buffer, src_file_buffer + prefix_fname_len, *hchash_len);

}

static int rar_vague2exp_mode (char *src_path, char *version, int *hash_mode)
{
  // if (memcmp (version, "$rar5$", 6) == 0)
  if (strcmp (version, "$rar5$") == 0)
  {
    *hash_mode = 13000;
  }
  else if (strcmp (version, "$RAR3$") == 0)
  {
    char tmp[24] = { 0 };
    FILE *fp = NULL;

    if ((fp = fopen (src_path, "rb")) == NULL)
    {
      printf ("error:open file %s failed\n", src_path);
      return -1;
    }
    fread (tmp, 1, 12, fp);
    fclose (fp);
    if (tmp[10] & 0x80)
    {
      *hash_mode = 12500;
    }
    else
    {
      *hash_mode = 12501;
      printf ("warning:version RAR3.x file name unencrypted is not supported\n");
      return ERROR_NUM_WARNING;
    }
  }
  else
  {
    printf ("version error\n");
    return -1;
  }
  return 0;
}

static int pdf_vague2exp_mode (char *version, int *hash_mode)
{
  // if (memcmp (version, "1*2*40*", 7) == 0)
  if (strcmp (version, "1*2*40*") == 0)
  {
    *hash_mode = 10400;
  }
  else if (strcmp (version, "2*3*128") == 0)
  {
    *hash_mode = 10500;
  }
  else if (strcmp (version, "5*5*256") == 0)
  {
    *hash_mode = 10600;
  }
  else if (strcmp (version, "5*6*256") == 0)
  {
    *hash_mode = 10700;
  }
  else
  {
    printf ("version error\n");
    return -1;
  }
  return 0;
}

static int vague2exp_mode (char *src_path, char *des_file_buffer, int *hash_mode)
{
  char version[10] = { 0 };
  int ret = 0;

  switch (*hash_mode)
  {
  case 9500:
  case 9400:
  case 9600:
    memcpy (version, des_file_buffer + 9, 4);
    office_vague2exp_mode (version, hash_mode);
    break;
  case 12500:
  case 13000:
    memcpy (version, des_file_buffer, 6);
    if (rar_vague2exp_mode (src_path, version, hash_mode))
    {
      return ret;
    }
    break;
  case 10400:
  case 10500:
  case 10600:
  case 10700:
    memcpy (version, des_file_buffer + 5, 7);
    ret = pdf_vague2exp_mode (version, hash_mode);
    if (ret)
    {
      return ret;
    }
    break;
  }

  return 0;
}

static void file_len_without_newline (char *src_file_buffer, int *file_len)
{
  int i;

  for (i = *file_len - 1; i > 0; i--)
  {
    if (src_file_buffer[i] == '\r' || src_file_buffer[i] == '\n')
    {
      (*file_len)--;
      continue;
    }
    break;
  }
}

static int check_file_len (char *src_file_buffer, int file_len, int hash_mode)
{
  char *seporater = NULL;

  switch (hash_mode)
  {
  case 0:
  case 1000:
    if (file_len != 32)
    {
      printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
      return -1;
    }
    break;
  case 10:
  case 20:
  case 30:
  case 40:
  case 50:
  case 60:
    if ((seporater = strchr (src_file_buffer, ':')) != NULL)
    {
      if ((seporater - src_file_buffer) != 32)
      {
        printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
        return -1;
      }
    }
    break;
  case 100:
  case 110:
  case 120:
  case 130:
  case 140:
    if ((seporater = strchr (src_file_buffer, ':')) != NULL)
    {
      if ((seporater - src_file_buffer) != 40)
      {
        printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
        return -1;
      }
    }
    break;
  case 200:
    if (file_len != 16)
    {
      printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
      return -1;
    }
    break;
  case 300:
    if (file_len != 40)
    {
      printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
      return -1;
    }
    break;
  case 400:
  case 500:
    if (file_len != 34)
    {
      printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
      return -1;
    }
    break;
  case 1500:
    if (file_len != 13)
    {
      printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
      return -1;
    }
    break;
  case 3000:
    if (file_len != 16)
    {
      printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
      return -1;
    }
    break;
  case 8600:
    if (file_len != 32)
    {
      printf ("error:hash type %d file len %d out of range\n", hash_mode, file_len);
      return -1;
    }
    break;
  }
  return 0;
}

static int vague_to_explicit_hashmode (rfile_info_ctx_t *rfile_info_ctx)
{
  hash_ctx_t *hct             = rfile_info_ctx->hash_ctx;
  char       *temp_path       = rfile_info_ctx->tmp_fpath;
  char       *src_path        = rfile_info_ctx->path;

  int         file_len        = 0;
  char       *src_file_buffer = NULL;
  char       *des_file_buffer = NULL;
  FILE       *fp              = NULL;
  int         ret             = 0;
  int         hash_mode       = hct->hash_mode;

  if (get_file_len (temp_path, &file_len) == -1) return -1;

  src_file_buffer = (char *) calloc (file_len + 5, sizeof (char));
  des_file_buffer = (char *) calloc (file_len + 5, sizeof (char));

  if (NULL == src_file_buffer || NULL == des_file_buffer)
  {
    printf ("malloc error\n");
    return -1;
  }

  if (hash_mode == 2500)
  {
    if ((fp = fopen (temp_path, "rb")) == NULL)
    {
      printf ("error:Fail to open file %s\n", temp_path);
      free (src_file_buffer);
      free (des_file_buffer);
      return -1;
    }
  }
  else
  {
    if ((fp = fopen (temp_path, "r")) == NULL)
    {
      printf ("error:Fail to open file %s\n", temp_path);
      free (src_file_buffer);
      free (des_file_buffer);
      return -1;
    }
  }

  fread (src_file_buffer, file_len, 1, fp);

  // ascii text (end with '\n' or '\r\n')

  if (hash_mode != 2500)
  {
    file_len_without_newline (src_file_buffer, &file_len);

    check_file_len (src_file_buffer, file_len, hash_mode);
  }


  // get explicit hash mode

  switch (hash_mode)
  {
  case 200:
  case 300:
    memcpy (des_file_buffer, src_file_buffer, file_len);
    if (file_len == 16)
    {
      hash_mode = 200;
    }
    else if (file_len == 40)
    {
      hash_mode = 300;
    }
    else
    {
      printf ("error:hashtype 200/300 file_len %d out of range\n", file_len);
      return -1;
    }
    break;
  case 9500:
  case 9400:
  case 9600:
    remove_colon (src_file_buffer, des_file_buffer, &file_len);

    ret = vague2exp_mode (src_path, des_file_buffer, &hash_mode);
    if (ret != 0)
    {
      printf ("error:Fail to get version\n");
      free (src_file_buffer);
      free (des_file_buffer);
      fclose (fp);
      return -1;
    }
    break;
  case 10400:
  case 10500:
  case 10600:
  case 10700:
    memcpy (des_file_buffer, src_file_buffer, file_len);

    ret = vague2exp_mode (src_path, des_file_buffer, &hash_mode);

    if (ret != 0 && ret != ERROR_NUM_WARNING)
    {
      printf ("error:Fail to get version\n");
      free (src_file_buffer);
      free (des_file_buffer);
      fclose (fp);
      return -1;
    }
    break;
  case 12500:
  case 13000:
    remove_colon (src_file_buffer, des_file_buffer, &file_len);

    ret = vague2exp_mode (src_path, des_file_buffer, &hash_mode);
    if (ret != 0)
    {
      printf ("error:Fail to get version\n");
      free (src_file_buffer);
      free (des_file_buffer);
      fclose (fp);
      return -1;
    }
    break;
  case 13600:
    remove_double_colon (src_file_buffer, des_file_buffer, &file_len);
    break;
  default:
    memcpy (des_file_buffer, src_file_buffer, file_len);
    break;
  }

  memcpy (hct->hash_val, des_file_buffer, file_len);

  hct->len = file_len;
  hct->hash_mode = hash_mode;

  free (src_file_buffer);
  free (des_file_buffer);
  fclose (fp);

  return 1;
}

// need vague or specific hash_mode
int extract_hchash_vaguemode (rfile_info_ctx_t *rfile_info_ctx)
{
  char *src_path = rfile_info_ctx->path;
  char *temp_path = rfile_info_ctx->tmp_fpath;
  hash_ctx_t *hct = rfile_info_ctx->hash_ctx;

  int ret = 0;
  char command[2000] = { 0 };

  int temp_flen = 0;

  int hash_mode = hct->hash_mode;

  switch (hash_mode)
  {
  case 0:
  case 10:
  case 20:
  case 30:
  case 40:
  case 50:
  case 60:
  case 100:
  case 110:
  case 120:
  case 130:
  case 140:
  case 200:
  case 300:
  case 400:
  case 500:
  case 900:
  case 1000:
  case 1400:
  case 1500:
  case 1700:
  case 1711:
  case 3000:
  case 3100:
  case 5500:
  case 6000:
  case 6100:
  case 6900:
  case 8600:
  case 14000:
  case 14500:
  case 20000:
  case 20001:
    copy_file (src_path, temp_path);
    // sprintf (command, "cp \'%s\' \'%s\'", src_path, temp_path);
    break;

  case 2500:
    sprintf (command, "%s %s %s", CAP_TO_HCCPAX_PATH, src_path, temp_path);
    /* ret = hccap_crackfile(src_path, temp_path); */
    /* if(ret != 0) */
    /* { */
    /* printf("error:Fail to modify the crack file\n"); */
    /* return -1; */
    /* } */
    break;
  case 6213:
  case 6223:
  case 6233:
  case 6243:
    sprintf (command, "%s " CVTTOOLS_SIGNATRUE"%s > %s", TRUECRYPT_TO_JOHN_PATH, src_path, temp_path);
    break;
  case 9500:
  case 9400:
  case 9600:
    sprintf (command, "%s " CVTTOOLS_SIGNATRUE" %s > %s", OFFICE_TO_JOHN_PATH, src_path, temp_path);
    break;
  case 10400:
  case 10500:
  case 10600:
  case 10700:
    sprintf (command, "%s " CVTTOOLS_SIGNATRUE" %s > %s", PDF_TO_HASHCAT_PATH, src_path, temp_path);
    break;
  case 11600:
    sprintf (command, "%s %s > %s", SZIP_TO_JOHN_PATH, src_path, temp_path);
    break;
  case 12500:
  case 13000:
    sprintf (command, "%s %s > %s", RAR_TO_JOHN_PATH, src_path, temp_path);
    break;
  case 13600:
    sprintf (command, "%s %s > %s", ZIP_TO_JOHN_PATH, src_path, temp_path);
    break;
    /* varacrypt */
  case 13721:
    sprintf (command, "./hc2john %s > %s", src_path, temp_path);
    break;
  default:
    copy_file (src_path, temp_path);
    // sprintf (command, "cp \'%s\' \'%s\'", src_path, temp_path);
    break;
  }

  system (command);

  if (get_file_len (temp_path, &temp_flen) == -1)
  {
    printf ("%s:temp_path len 0\n", temp_path);

    return -1;
  }

  // printf ("tmp file len %d\n", temp_flen);

  if (temp_flen == 0) rfile_info_ctx->file_encryption = FILE_UNENCRYPTED;
  if (temp_flen != 0) rfile_info_ctx->file_encryption = FILE_ENCRYPTED;

  if (rfile_info_ctx->file_encryption == FILE_ENCRYPTED)
  {
    ret = vague_to_explicit_hashmode (rfile_info_ctx);

    // printf ("vague_to_explicit_hashmode(): %d\n", rfile_info_ctx->hash_ctx->hash_mode);

    if (ret == -1)
    {
      fprintf (stderr, "get hash value failed\n");

      return -1;
    }
  }

  if (remove(temp_path) == -1)
  {
    fprintf (stderr, "could not remove temp %s\n", temp_path);

    return -1;
  }

  return 1;
}

// hashes
// n_hash -> md5 md4 sha1...
// r_hash -> office rar...
// v_hash -> vpn
// w_hash -> wpa

// rawfiles (rfiles)
int get_rw_rfile_ftype (rfile_info_ctx_t *rfile_info_ctx)
{
  hash_ctx_t *hash_ctx = rfile_info_ctx->hash_ctx;

  char *fpath = rfile_info_ctx->path;

  FILE *rfp = fopen (fpath, "rb");

  if (rfp == NULL)
  {
    fprintf (stderr, "%s: %s\n", fpath, strerror (errno));

    return -1;
  }

  // check file header

  unknown_file_header_t unknown_file_header;

  const int nread = fread (&unknown_file_header, sizeof (unknown_file_header_t), 1, rfp);

  if (nread != 1)
  {
    fprintf (stderr, "%s: Could not read file header \n", fpath);

    return -1;
  }

  if (memcmp (unknown_file_header.magic, OFFICE_MAGIC, 4) == 0)
  {
    hash_ctx->hash_mode = 9400;
  }

  if (memcmp (unknown_file_header.magic, PDF_MAGIC, 4) == 0)
  {
    hash_ctx->hash_mode = 10400;
  }

  if (memcmp (unknown_file_header.magic, SZIP_MAGIC, 6) == 0)
  {
    hash_ctx->hash_mode = 11600;
  }

  if (memcmp (unknown_file_header.magic, RAR3_MAGIC, 7) == 0 || memcmp (unknown_file_header.magic, RAR5_MAGIC, 7) == 0)
  {
    hash_ctx->hash_mode = 12500;
  }

  if (memcmp (unknown_file_header.magic, PKZIP_MAGIC, 4) == 0)
  {
    hash_ctx->hash_mode = 13600;
  }

  if (memcmp (unknown_file_header.magic, TCPDUMP_MAGIC, 4) == 0 || memcmp (unknown_file_header.magic, TCPDUMP_CIGAM, 4) == 0)
  {
    hash_ctx->hash_mode = 2500;
  }

  if (hash_ctx->hash_mode == -1)
  {
    fprintf (stderr, "%s: unknown file type \n", fpath);

    return -1;
  }

  // printf ("%s -> vague hash mode: %d \n", fpath, hash_ctx->hash_mode);

  fclose (rfp);

  return 1;
}

int hash_ctx_init (hash_ctx_t *hash_ctx)
{
  hash_ctx->hash_mode = -1;
  hash_ctx->len = 0;

  memset (hash_ctx->hash_val, 0, sizeof (hash_ctx->hash_val));

  hash_ctx->is_valid = false;

  return 1;
}

int hash_ctx_destory (hash_ctx_t *hash_ctx)
{
  // jmfree (hash_ctx);

  return 1;
}
