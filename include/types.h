// to prevent types.h conflict
#ifndef _JM_TYPES_H
#define _JM_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "common.h"

#define BUF_MINLEN 256
#define BUF_FPATHLEN 1024
#define BUF_MAXLEN 8192


#define FILES_MAX       256
#define FILE_PATH_MAXLEN 512



// cvt signatrue
#define CVTTOOLS_SIGNATRUE "skdlxnoe2390es98d9jlsa0932jkndlod"

// file magic

#define OFFICE_MAGIC  "\xd0\xcf\x11\xe0"
#define PDF_MAGIC     "\x25\x50\x44\x46"
#define SZIP_MAGIC    "\x37\x7a\xbc\xaf\x27\x1c"
#define RAR3_MAGIC    "\x52\x61\x72\x21\x1a\x07\x00"
#define RAR5_MAGIC    "\x52\x61\x72\x21\x1a\x07\x01"
#define PKZIP_MAGIC   "\x50\x4b\x03\x04"

#define TCPDUMP_MAGIC "\xa1\xb2\xc3\xd4"
#define TCPDUMP_CIGAM "\xd4\xc3\xb2\xa1"

// pptp des

#define TASK_ID_MAXLEN      20
#define USERNAME_MAXLEN     128
#define CHANLLENGE_DATA_LEN 32
#define RESPONSE_DATA_LEN   96

#define PEER_CHALLENGE_LEN  32
#define NTRESPONSE_LEN      48
#define AUTH_CHALLENGE_LEN  CHANLLENGE_DATA_LEN

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


enum file_encryption {
  FILE_UNENCRYPTED=0, FILE_ENCRYPTED=1, FILE_ENCRYPTION_UNKNOWN=-1
};

typedef enum file_encryption file_encryption_t;

enum hash_file_format {
  FORMAT_BINARY, FORMAT_TEXT
};

typedef enum hash_file_format hash_file_format_t;

struct pptpdes_filehdr {
  u8 type;
  char task_id[TASK_ID_MAXLEN];
  u8 level;
  u8 policy;
  u8 policy_num;
  u8 task_cmd;

  char username[USERNAME_MAXLEN];
  char challenge_data[CHANLLENGE_DATA_LEN + 1];
  char response_data[RESPONSE_DATA_LEN + 1];

  u32 task_id_len;
  u32 username_len;
  u32 challenge_data_len;
  u32 response_data_len;

  u32 time_stamp;
};

typedef struct pptpdes_filehdr pptpdes_filehdr_t;

struct pptpdes_des_ctx {
  u8 ciphertext[3][8];
  u8 plaintext[8];
  u64 key[3];
};

typedef struct pptpdes_des_ctx pptpdes_des_ctx_t;

struct pptpdes_response_ctx {
  char peerchall_char[PEER_CHALLENGE_LEN+1];
  char ntresponse_char[NTRESPONSE_LEN+1];

  u8 peerchall_u8[PEER_CHALLENGE_LEN/2];
  u8 ntresponse_u8[NTRESPONSE_LEN/2];
};

typedef struct pptpdes_response_ctx pptpdes_response_ctx_t;

struct pptpdes_authchall_ctx {
  char authchall_char[AUTH_CHALLENGE_LEN+1];

  u8 authchall_u8[AUTH_CHALLENGE_LEN/2];
};

typedef struct pptpdes_authchall_ctx pptpdes_authchall_ctx_t;


struct hccmd_ctx {
  // basic

  char task_id[BUF_MINLEN];
  char fixed_cmd[BUF_MINLEN];

  u32 attack_mode;
  u32 hash_mode;

  // char hash_val[BUF_MINLEN];
  char cvtd_fpath[FILE_PATH_MAXLEN];

  char tbc_fpath[FILE_PATH_MAXLEN];
  char out_fpath[FILE_PATH_MAXLEN];
  char masks_abspace_fpath[256 * (FILE_PATH_MAXLEN+1)];
  char dicts_abspace_fpath[256 * (FILE_PATH_MAXLEN+1)];
  // char mask_fpath[FILE_PATH_MAXLEN];
  // char dict1_fpath[FILE_PATH_MAXLEN];

  // pass to hclog

  char cmd[BUF_MAXLEN];
  char dev_status_fpath[FILE_PATH_MAXLEN];
};

typedef struct hccmd_ctx hccmd_ctx_t;

struct office_file_header {
  char magic[4];
};

typedef struct office_file_header office_file_header_t;

struct pdf_file_header {
  char magic[4];
};

typedef struct pdf_file_header pdf_file_header_t;

struct szip_file_header {
  char magic[6];
};

typedef struct szip_file_header szip_file_header_t;

struct rar3_file_header {
  char magic[7];
};

typedef struct rar3_file_header rar3_file_header_t;

struct rar5_file_header {
  char magic[7];
};

typedef struct rar5_file_header rar5_file_header_t;

struct pkzip_file_header {
  char magic[4];
};

typedef struct pkzip_file_header pkzip_file_header_t;

struct pcap_file_header {
  u32 magic;
  u16 version_major;
  u16 version_minor;
  u32 thiszone; /* gmt to local correction */
  u32 sigfigs;  /* accuracy of timestamps */
  u32 snaplen;  /* max length saved portion of each pkt */
  u32 linktype; /* data link type (LINKTYPE_*) */
};

struct unknown_file_header {
  char magic[10];
};

typedef struct unknown_file_header unknown_file_header_t;

struct hash_ctx
{
  bool  is_valid;

  char  hash_val[BUF_MAXLEN];
  int   len;
  int   hash_mode;

  // char  checksum[32];
};

typedef struct hash_ctx hash_ctx_t;

struct rfile_info_ctx {
  file_encryption_t file_encryption;

  char tmp_fpath[BUF_MINLEN];

  char type[16];
  char version[16];
  char path[FILE_PATH_MAXLEN];

  hash_ctx_t *hash_ctx;
};

typedef struct rfile_info_ctx rfile_info_ctx_t;


// hash_extr ctx

struct htr_user_options {
  int    op_mode;

  int    hash_mode;

  char  *config_fpath;

  char **tbc_fpaths;
  u32    tbc_fpaths_cnt;

  char  *out_fpath;

  bool usage;

  // char *unftd_hash_fpath;
  // bool is_fullinfo;
};

typedef struct htr_user_options htr_user_options_t;

// process main context

struct htr_ctx {
  const char *log_fpath;
  int valid_hashes_cnt;

  htr_user_options_t *user_options;
  rfile_info_ctx_t *rfile_info_ctx;
};

typedef struct htr_ctx htr_ctx_t;



struct htr_config_inicfg {
  char wpa2500[BUF_MINLEN];
  char office9400[BUF_MINLEN];
  char office9500[BUF_MINLEN];
  char office9600[BUF_MINLEN];
  char pdf10400[BUF_MINLEN];
  char pdf10410[BUF_MINLEN];
  char pdf10420[BUF_MINLEN];
  char pdf10500[BUF_MINLEN];
  char pdf10600[BUF_MINLEN];
  char pdf10700[BUF_MINLEN];
  char szip11600[BUF_MINLEN];
  char rar12500[BUF_MINLEN];
  char rar13000[BUF_MINLEN];
  char pkzip13600[BUF_MINLEN];
};

typedef struct htr_config_inicfg htr_config_inicfg_t;



#ifdef __cplusplus
}
#endif

#endif // _JM_TYPES_H
