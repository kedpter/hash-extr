# JtR configure Arm-NEON instruction active probe test
# Copyright (C) 2014 Jim Fougeron, for John Ripper project.
# This file put into public domain. unlimited permission to
# copy and/or distribute it, with or without modifications,
# as long as this notice is preserved.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.
dnl
dnl this code will probe test an ARM system, to see if it can handle
dnl NEON code.
dnl
AC_DEFUN([JTR_ARM_SPECIAL_LOGIC], [
CC_BACKUP=$CC
CFLAGS_BACKUP=$CFLAGS
dnl
#############################################################################
# Arm-NEON Active CPU probe test.  We check for NEON instructions
# At the very end of configure, we set gcc
# back to whatever the 'best' was.  During running in configure, $CC gets reset
# so the results of our tests must be remembered, and reset just before exit.
# Config probe test code copyright 2014, Jim Fougeron.  Placed into public domain.
#############################################################################
CFLAGS="$CFLAGS -O0"
if test "x$enable_native_tests" = xyes; then
  AC_MSG_NOTICE([Testing build host's native CPU features])
  CC="$CC_BACKUP -mfpu=neon"
  AC_MSG_CHECKING([for NEON])
  AC_RUN_IFELSE(
    [
    AC_LANG_SOURCE(
	  [[#include <arm_neon.h>
	#include <stdio.h>
	extern void exit(int);
	int main(){uint32x4_t t;*((long*)&t)=1;t=veorq_u32(t,t);if((*(unsigned*)&t)==88)printf(".");exit(0);}]]
    )]
    ,[CPU_BEST_FLAGS="-mfpu=neon"] dnl
     [CPU_STR="NEON"]
     [AC_MSG_RESULT([yes])]
    ,[CPU_NOTFOUND="1"]
     [AC_MSG_RESULT(no)]
  )
else
  ##########################################
  # cross-compile versions of the same tests
  ##########################################
  AC_MSG_CHECKING([for NEON])
  AC_LINK_IFELSE(
    [
    AC_LANG_SOURCE(
	  [[#include <arm_neon.h>
	#include <stdio.h>
	extern void exit(int);
	int main(){uint32x4_t t;*((long*)&t)=1;t=veorq_u32(t,t);if((*(unsigned*)&t)==88)printf(".");exit(0);}]]
    )]
    ,[CPU_BEST_FLAGS="-mfpu=neon"] dnl
     [CPU_STR="NEON"]
     [AC_MSG_RESULT([yes])]
    ,[CPU_NOTFOUND="1"]
     [AC_MSG_RESULT(no)]
  )
fi

CC="$CC_BACKUP"
CFLAGS="$CFLAGS_BACKUP"
])
