#ifndef _HCCVT_H
#define _HCCVT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include <sys/stat.h>
// #include <sys/file.h>
// #include <dirent.h>
// #include <sys/types.h>
// #include <signal.h>

#include "hccvt.h"
#include "common.h"
#include "types.h"

#define CUT_CHALLENG_LEN 32
#define RESPONSEDATA     8
#define CHALLENGEDATA    7
#define USERNAME_LEN     6

#define DEVICE_TYPE_GPU  1
#define DEVICE_TYPE_FPGA 0

#define ERROR_NUM_WARNING 0x00000011



#if defined (_POSIX)

// #if defined (__unix__)
// #define SZIP_TO_JOHN_PATH      "./cvttools/posix/7z2hashcat_centos7"                              //    7ZIP
// #elif defined (__APPLE__)
#define SZIP_TO_JOHN_PATH      "./cvttools/posix/7z2hashcat.pl"                              //    7ZIP
// #endif

#define CAP_TO_HCCPAX_PATH     "./cvttools/posix/hashcat-utils-master/src/cap2hccapx.bin" //    WPA
#define RAR_TO_JOHN_PATH       "./cvttools/posix/JohnTheRipper/run/rar2john"              //    RAR
#define ZIP_TO_JOHN_PATH       "./cvttools/posix/JohnTheRipper/run/zip2john"              //    ZIP

#define OFFICE_TO_JOHN_PATH    "./cvttools/posix/office2john.py"                          //    GPU_OFFICE
#define PDF_TO_HASHCAT_PATH    "./cvttools/posix/pdf2hashcat.py"                          //    PDF
#define TRUECRYPT_TO_JOHN_PATH "./cvttools/posix/truecrypt2john.py"                       //    TRUECRYPT

#elif defined (_WIN)

#define RAR_TO_JOHN_PATH       ".\\cvttools\\windows\\rar2john.exe" //    RAR
#define ZIP_TO_JOHN_PATH       ".\\cvttools\\windows\\zip2john.exe" //    ZIP

#define CAP_TO_HCCPAX_PATH     ".\\cvttools\\windows\\cap2hccapx.exe"     //    WPA
#define OFFICE_TO_JOHN_PATH    ".\\cvttools\\windows\\office2john.exe"    //    GPU_OFFICE
#define TRUECRYPT_TO_JOHN_PATH ".\\cvttools\\windows\\truecrypt2john.exe" //    TRUECRYPT
#define PDF_TO_HASHCAT_PATH    ".\\cvttools\\windows\\pdf2hashcat.exe"    //    PDF
#define SZIP_TO_JOHN_PATH      ".\\cvttools\\windows\\7z2hashcat32.exe"        //    7ZIP

#define SYNC_EXE_PATH      ".\\cvttools\\windows\\sync32.exe"

#endif

// int vague_to_explicit_hashmode (rfile_info_ctx_t *rfile_info_ctx);
int extract_hchash_vaguemode (rfile_info_ctx_t *rfile_info_ctx);
int get_rw_rfile_ftype (rfile_info_ctx_t *rfile_info_ctx);

int hash_ctx_init (hash_ctx_t *hash_ctx);
int hash_ctx_destory (hash_ctx_t *hash_ctx);

#ifdef __cplusplus
}
#endif

#endif
