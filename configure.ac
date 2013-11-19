AC_PREREQ([2.67])
AC_INIT([hixo], [1.0])
AM_INIT_AUTOMAKE
AC_DEFINE([_GNU_SOURCE])
AC_CHECK_SIZEOF([long])
AS_IF([test "$ac_cv_sizeof_long" -eq 4],
          [AC_DEFINE([BITS_32], [1])],
          [AC_DEFINE([BITS_32], [0])])
AC_PROG_CC([cc])
AM_PROG_CC_C_O
AC_CONFIG_SRCDIR([src/hixo.c])
AC_CONFIG_FILES([Makefile]
                [include/Makefile]
                [src/Makefile]
                [tests/Makefile])
AC_OUTPUT
