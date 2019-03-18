#include "common.h"

int strlist_init (char ***outputs, int num_of_outputs, int output_len)
{
  char **t = (char **) malloc (num_of_outputs * sizeof (char *));

  if (t == NULL)
  {
    printf ("alloc memory for pipe output failed\n");
    return -1;
  }
  for (int i = 0; i < num_of_outputs; ++i)
  {
    t[i] = (char *) malloc (output_len);
    if (t[i] == NULL)
    {
      printf ("alloc memory for pipe output failed\n");
      return -1;
    }
  }
  *outputs = t;
  return 1;
}

void strlist_setempty (char ***outputs, int num_of_outputs, int output_len)
{
  char **t = *outputs;

  for (int i = 0; i < num_of_outputs; i++)
  {
    memcpy (t[i], "\0", output_len);
  }
  *outputs = t;
}

void strlist_copy (char ***outputs, char **old, int start, int end, int output_len)
{
  char **n = *outputs;

  for (int i = start; i < end; i++)
  {
    memcpy (n[i], old[i], output_len);
  }
  *outputs = n;
}

void strlist_free (char ***outputs, int num_of_outputs)
{
  char **t = *outputs;

  for (int i = 0; i < num_of_outputs; i++)
  {
    free (t[i]);
  }
  free (t);
  *outputs = NULL;
}


/* 
 * this function trims double quote from leading and trailing of a string,
 * return a pointor to a substring of the original.
 * the return value must not be deallocated using free()
 */
char *trimdquote (char *str)
{
  char *end;

  /* trim leading */
  while (*str == '"')
    str++;

  if (*str == '"')              // all ?
    return str;

  /* trim trailing */
  end = str + strlen (str) - 1;
  while (end > str && (*end == '"'))
    end--;

  /* write null terminator */
  *(end + 1) = 0;

  return str;
}

void remove_substring(char *s,const char *toremove)
{
  while((s = strstr (s,toremove)))
    memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
}

/* remove filename extension */
/* free after used */
char *remove_ext (char *mystr, char dot, char sep)
{
  char *retstr, *lastdot, *lastsep;

  // Error checks and allocate string.
  if (mystr == NULL)
    return NULL;
  if ((retstr = (char *) malloc (strlen (mystr) + 1)) == NULL)
    return NULL;

  // Make a copy and find the relevant characters.
  strcpy (retstr, mystr);
  lastdot = strrchr (retstr, dot);
  lastsep = (sep == 0) ? NULL : strrchr (retstr, sep);

  // If it has an extension separator.
  if (lastdot != NULL)
  {
    // and it's before the extenstion separator.
    if (lastsep != NULL)
    {
      if (lastsep < lastdot)
      {
        // then remove it.
        *lastdot = '\0';
      }
    }
    else
    {
      // Has extension separator with no path separator.
      *lastdot = '\0';
    }
  }

  // Return the modified string.
  return retstr;
}

char *get_basename (char *path)
{
  char buffer[FILE_PATH_MAXLEN] = { 0 };

  char *path_tmp = path;

  int last_slash_pos = 0;

  size_t i = 0;

  for (; i < strlen (path); i++)
  {
    if (*path_tmp == '/')
      last_slash_pos = i + 1;

    path_tmp++;
  }

  char *name_ptr = path + last_slash_pos;

  strncpy (buffer, name_ptr, strlen (path) - last_slash_pos);

  memcpy (path, buffer, strlen (buffer) + 1);

  return path;
}

// free after used
char *get_filename_nosuffix (char *path)
{
  char *fn_withsuffix = get_basename (strdup (path));

  return remove_ext (fn_withsuffix, '.', '/');
}

// free after used
char *jm_base_to_absolute_path (const char *base, const char *relative)
{
  char *abpath = (char *) malloc (FILE_PATH_MAXLEN * sizeof (char));

// #if defined (_POSIX)
//   abpath = realpath ()
// 
// #elif defined (_WIN)

  snprintf (abpath, FILE_PATH_MAXLEN, "%s/%s", base, relative);
  return abpath;
}



int is_file_exist (char *path)
{
  if (access (path, F_OK) == -1)
  {
    return -1;
  }
  return 1;
}

/* #ifndef HAVE_STRSEP */
char *k_strsep (char **sp, const char *sep)
{
  char *p, *s;

  if (sp == NULL || *sp == NULL || **sp == '\0')
    return (NULL);
  s = *sp;
  p = s + strcspn (s, sep);
  if (*p != '\0')
    *p++ = '\0';
  *sp = p;
  return (s);
}

void *jmcalloc (const size_t nmemb, const size_t sz)
{
  void *p = calloc (nmemb, sz);

  if (p == NULL)
  {
    fprintf (stderr, "%s\n", MSG_ENOMEM);

    return (NULL);
  }

  return (p);
}

void *jmmalloc (const size_t sz)
{
  // calloc is faster than malloc with big allocations, so just use that.
  void *p = jmcalloc (sz, 1);

  return (p);
}

void jmfree (void *ptr)
{
  if (ptr == NULL)
    return;

  free (ptr);
}

int get_file_len (const char *fpath, int *file_len)
{
  FILE *fp = fopen (fpath, "rb");

  if (fp == NULL)
  {
    fprintf (stderr, "%s: open file failed \n", fpath);
    
    return -1;
  }

  fseek(fp, 0L, SEEK_END);

  *(file_len) = ftell(fp);

  fclose(fp);

  return 1;
}

int copy_file (char *old_filename, char *new_filename)
{
  FILE *fptr1, *fptr2;

  char c;

  // Open one file for reading
  fptr1 = fopen (old_filename, "r");

  if (fptr1 == NULL)
  {
    printf ("Cannot open file %s \n", old_filename);

    return -1;
  }

  // Open another file for writing
  fptr2 = fopen (new_filename, "w");

  if (fptr2 == NULL)
  {
    printf ("Cannot open file %s \n", new_filename);

    return -1;
  }

  // Read contents from file

  c = fgetc (fptr1);

  while (c != EOF)
  {
    fputc (c, fptr2);
    c = fgetc (fptr1);
  }

  fflush (fptr1);
  fflush (fptr2);

  fclose (fptr1);
  fclose (fptr2);

  return 1;
}

// linux and windows compatible

FILE *jmpopen (const char *cmd, const char *mode)
{
  FILE *p;

#if defined (_POSIX)

  p = popen (cmd, mode);

#elif defined (_WIN)

  p = _popen (cmd, mode);

#endif

  return (p);
}

FILE *jmpclose (FILE * p)
{
#if defined (_POSIX)

  pclose (p);

#elif defined (_WIN)

  _pclose (p);

#endif

  return (p);
}
