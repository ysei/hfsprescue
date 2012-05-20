#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(hfsprescue, [0.2 20120520], [http://github.com/monaka/hfsprescue])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([hfsprescue.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for libraries.
AC_SEARCH_LIBS(iconv, iconv)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h signal.h stdint.h stdlib.h string.h unistd.h iconv.h])

# Check for system requirements.
AC_SYS_LARGEFILE

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([mkdir])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
