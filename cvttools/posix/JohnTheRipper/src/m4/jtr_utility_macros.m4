# This file is Copyright (C) 2014 JimF, and magnum
# and is hereby released to the general public under the following terms:
# Redistribution and use in source and binary forms, with or without
# modifications, are permitted.
#
# Here are contained numerous utility macros

# JTR_LIST_ADD(variable, value(s))
# Add space separated value(s) to variable unless already present.
AC_DEFUN([JTR_LIST_ADD], [
   for i in $2; do
      jtr_list_add_dupe=0
      for j in $$1; do
         if test "x$i" = "x$j"; then
            jtr_list_add_dupe=1
            break
         fi
      done
      if test $jtr_list_add_dupe = 0; then
         $1="$$1 $i"
         jtr_list_add_result="$jtr_list_add_result $i"
      fi
   done
])

AC_DEFUN([JTR_LIST_ADD_RESULT], [
   AS_IF([test -z "$jtr_list_add_result"],AC_MSG_RESULT([none]),AC_MSG_RESULT([$jtr_list_add_result]))
   jtr_list_add_result=""
])

# @synopsis JTR_FLAG_CHECK [compiler flags]
# @summary check whether compiler supports given
#          C flags or not. The var CFLAGS_EX is
#          added to with each 'valid' command.
AC_DEFUN([JTR_FLAG_CHECK],
[dnl
  AS_IF([test "$2" = 1], [AC_MSG_CHECKING([if $CC supports $1])])
  AC_LANG_PUSH([C])
  ac_saved_cflags="$CFLAGS"
  CFLAGS="-Werror $1"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
    [AS_IF([test "$2" = 1], [AC_MSG_RESULT([yes])])]
      [CFLAGS_EX="$CFLAGS_EX $1"]
    ,[AS_IF([test "$2" = 1], [AC_MSG_RESULT([no])])]
  )
  CFLAGS="$ac_saved_cflags"
  AC_LANG_POP([C])
])

# @synopsis JTR_FLAG_CHECK_LINK [compiler flags]
# @summary check whether compiler supports given
#          C flags or not. The var CFLAGS_EX is
#          added to with each 'valid' command.
#          This macro adds linkage, since some
#          functions will compile, but fail at
#          link time (-faddress_sanitize is one)
AC_DEFUN([JTR_FLAG_CHECK_LINK],
[dnl
  AS_IF([test "$2" = 1], [AC_MSG_CHECKING([if $CC supports $1])])
  AC_LANG_PUSH([C])
  ac_saved_cflags="$CFLAGS"
  CFLAGS="-Werror $1"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
    [AS_IF([test "$2" = 1], [AC_MSG_RESULT([yes])])]
      [CFLAGS_EX="$CFLAGS_EX $1"]
    ,[AS_IF([test "$2" = 1], [AC_MSG_RESULT([no])])]
  )
  CFLAGS="$ac_saved_cflags"
  AC_LANG_POP([C])
])

# @synopsis SET_NORMAL_INCLUDES
# @summary check and set many normal include paths
# This might be a Bad Idea[tm] if cross compiling.
AC_DEFUN([JTR_SET_NORMAL_INCLUDES],
[
  AC_MSG_CHECKING([additional paths])
  ADD_LDFLAGS=""
  ADD_CFLAGS=""
if test -d /usr/local/lib; then
   ADD_LDFLAGS="$ADD_LDFLAGS -L/usr/local/lib"
fi
if test -d /usr/local/include; then
   ADD_CFLAGS="$ADD_CFLAGS -I/usr/local/include"
fi
JTR_LIST_ADD(CPPFLAGS, [$ADD_CFLAGS]) # no typo here
jtr_list_add_result=""
JTR_LIST_ADD(LDFLAGS, [$ADD_LDFLAGS])
JTR_LIST_ADD(CFLAGS, [$ADD_CFLAGS])
JTR_LIST_ADD_RESULT
])

# @synopsis SET_NORMAL_SSL_INCLUDES(base path)
# @summary check and set include/library paths for OpenSSL
# This might be a Bad Idea[tm] if cross compiling.
AC_DEFUN([JTR_SET_NORMAL_SSL_INCLUDES],
[
  AC_MSG_CHECKING([additional paths for OpenSSL])
  ADD_LDFLAGS=""
  ADD_CFLAGS=""
if test -d $1/lib; then
   ADD_LDFLAGS="$ADD_LDFLAGS -L$1/lib"
fi
if test -d $1/include; then
   ADD_CFLAGS="$ADD_CFLAGS -I$1/include"
fi
JTR_LIST_ADD(CPPFLAGS, [$ADD_CFLAGS]) # no typo here
jtr_list_add_result=""
JTR_LIST_ADD(LDFLAGS, [$ADD_LDFLAGS])
JTR_LIST_ADD(CFLAGS, [$ADD_CFLAGS])
JTR_LIST_ADD_RESULT
])

# JTR_MSG_RESULT_FAILIF_FORCED(success, forced, forced_fail_msg)
# success and forced should be xvar data, "x$enable_foobar", so they
# will be xno, xyes, xauto, etc.  forced_fail_msg is a message that
# will be output, and the script will abort, IF forced is xyes which
# means the user used --enable-foobar
AC_DEFUN([JTR_MSG_RESULT_FAILIF_FORCED], [
  if test "$1" == xyes; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
    if test "$2" == xyes; then
      AC_MSG_FAILURE([$3])
    fi
  fi
])

# JTR_MSG_CHECKING_AND_RESULT_FAILIF_FORCED(chk_msg, success, forced, forced_fail_msg)
# will output a checking 'chk_msg', then calls JTR_MSG_RESULT_FAILIF_FORCED
AC_DEFUN([JTR_MSG_CHECKING_AND_RESULT_FAILIF_FORCED], [
  AC_MSG_CHECKING([$1])
  JTR_MSG_RESULT_FAILIF_FORCED($2,$3,$4)
])

# @synopsis JTR_SET_OPENCL_INCLUDES
# @summary check and set many normal include paths
AC_DEFUN([JTR_SET_OPENCL_INCLUDES],
[
   AC_MSG_CHECKING([additional paths for OpenCL])
   ADD_LDFLAGS=""
   ADD_CFLAGS=""
   if test -n "$AMDAPPSDKROOT"; then
      if test -d "$AMDAPPSDKROOT/include"; then
         ADD_CFLAGS="$ADD_CFLAGS -I$AMDAPPSDKROOT/include"
      fi
      if test $CPU_BIT_STR = 64 -a -d "$AMDAPPSDKROOT/lib/x86_64" ; then
         ADD_LDFLAGS="$ADD_LDFLAGS -L$AMDAPPSDKROOT/lib/x86_64"
      elif test  $CPU_BIT_STR = 32 -a -d "$AMDAPPSDKROOT/lib/x86" ; then
         ADD_LDFLAGS="$ADD_LDFLAGS -L$AMDAPPSDKROOT/lib/x86"
      elif test -d "$AMDAPPSDKROOT/lib"; then
         ADD_LDFLAGS="$ADD_LDFLAGS -L$AMDAPPSDKROOT/lib"
      fi
   fi
   if test -n "$ATISTREAMSDKROOT"; then
      if test -d "$ATISTREAMSDKROOT/include"; then
         ADD_CFLAGS="$ADD_CFLAGS -I$ATISTREAMSDKROOT/include"
      fi
      if test $CPU_BIT_STR = 64 -a -d "$ATISTREAMSDKROOT/lib/x86_64" ; then
         ADD_LDFLAGS="$ADD_LDFLAGS -L$ATISTREAMSDKROOT/lib/x86_64"
      elif test  $CPU_BIT_STR = 32 -a -d "$ATISTREAMSDKROOT/lib/x86" ; then
         ADD_LDFLAGS="$ADD_LDFLAGS -L$ATISTREAMSDKROOT/lib/x86"
      elif test -d "$ATISTREAMSDKROOT/lib"; then
         ADD_LDFLAGS="$ADD_LDFLAGS -L$ATISTREAMSDKROOT/lib"
      fi
   fi
   JTR_LIST_ADD(CPPFLAGS, [$ADD_CFLAGS]) # no typo here
   jtr_list_add_result=""
   JTR_LIST_ADD(LDFLAGS, [$ADD_LDFLAGS])
   JTR_LIST_ADD(CFLAGS, [$ADD_CFLAGS])
   JTR_LIST_ADD_RESULT
])

dnl @synopsis ACX_HEADER_STRING
dnl @summary See whether we can include both string.h and strings.h.
dnl @usage:
dnl #if STRING_WITH_STRINGS
dnl #include <string.h>
dnl #include <strings.h>
dnl #elif HAVE_STRING_H
dnl #include <string.h>
dnl #elif HAVE_STRINGS_H
dnl #include <strings.h>
dnl #endif
AC_DEFUN([ACX_HEADER_STRING],
[AC_CACHE_CHECK([whether string.h and strings.h may both be included],
  gcc_cv_header_string,
[AC_TRY_COMPILE([#include <string.h>
#include <strings.h>], , gcc_cv_header_string=yes, gcc_cv_header_string=no)])
if test $gcc_cv_header_string = yes; then
  AC_DEFINE(STRING_WITH_STRINGS, 1, [Define if you can safely include both <string.h> and <strings.h>.])
fi
])
