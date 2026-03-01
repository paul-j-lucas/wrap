# Process this file with autoconf to produce a configure script.

##
#       wrap -- text reformatter
#       configure.ac
#
#       Copyright (C) 2013-2026  Paul J. Lucas
#
#       This program is free software: you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation, either version 3 of the License, or
#       (at your option) any later version.
#
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#
#       You should have received a copy of the GNU General Public License
#       along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

AC_PREREQ([2.69])
AC_INIT([wrap], [5.11.1], [https://github.com/paul-j-lucas/wrap/issues],,[https://github.com/paul-j-lucas/wrap])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_LANG(C)
AC_PROG_CC
AS_IF([test "x$ac_cv_prog_cc_c11" = xno],
  [AC_MSG_ERROR([a C11 compiler is required to compile $PACKAGE_NAME])])
gl_EARLY
AC_PROG_INSTALL

# Program feature: --width=term (enabled by default)
AC_ARG_ENABLE([width-term],
  AS_HELP_STRING([--disable-width-term], [disable support for --width=term])
)
AS_IF([test "x$enable_width_term" != xno],
  [AC_DEFINE([ENABLE_WIDTH_TERM], [1],
    [Define to 1 if --width=term is enabled.])]
)

# Checks for libraries.

# Checks for header files.
AS_IF([test "x$enable_width_term" != xno],
  [
    AC_CHECK_HEADERS([curses.h ncurses.h], [],
      [AC_MSG_ERROR([curses library for --width=term not found; use --disable-width-term])]
    )
    AC_CHECK_HEADERS([term.h], [], [],
    [#if defined(HAVE_CURSES_H)
    #include <curses.h>
    #elif defined(HAVE_NCURSES_H)
    #include <ncurses.h>
    #endif
    ])
  ]
)
AC_CHECK_HEADERS([fnmatch.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([pwd.h])
AC_CHECK_HEADERS([regex.h], [],
  [AC_MSG_ERROR([regex.h required header not found])])
AC_CHECK_HEADERS([sysexits.h])
AC_HEADER_ASSERT
AC_HEADER_STDBOOL
gl_INIT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_TYPEOF
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
PJL_CHECK_TYPEDEF(char8_t,[uchar.h],
  AC_DEFINE([HAVE_CHAR8_T], [1], [Define to 1 if `char8_t' is supported.]),
  AC_DEFINE([HAVE_CHAR8_T], [0], [Define to 1 if `char8_t' is supported.])
)
PJL_CHECK_TYPEDEF(char32_t,[uchar.h],
  AC_DEFINE([HAVE_CHAR32_T], [1], [Define to 1 if `char32_t' is supported.]),
  AC_DEFINE([HAVE_CHAR32_T], [0], [Define to 1 if `char32_t' is supported.])
)
AC_CHECK_MEMBERS([struct passwd.pw_dir],[],[],[[#include <pwd.h>]])
PJL_COMPILE([__builtin_expect],[], [(void)__builtin_expect(1,1);])

# Checks for library functions.
AC_REPLACE_FNMATCH
AC_FUNC_FORK
AC_FUNC_REALLOC
AC_CHECK_FUNCS([geteuid getpwuid perror strerror strndup])
AS_IF([test "x$enable_width_term" != xno], [
  AC_SEARCH_LIBS([endwin],[curses ncurses], [],
    [AC_MSG_ERROR([curses library for --width=term not found; use --disable-width-term])]
  )
  AC_SEARCH_LIBS([tigetnum],[curses ncurses tinfo], [],
    [AC_MSG_ERROR([curses library for --width=term not found; use --disable-width-term])]
  )
])

# Makefile conditionals.
AM_CONDITIONAL([ENABLE_WIDTH_TERM], [test "x$enable_width_term" != xno ])

# Miscellaneous.
AX_C___ATTRIBUTE__

# Compiler warnings.
AC_SUBST([WRAP_CFLAGS])
WRAP_CFLAGS="-D_ISOC11_SOURCE"
AX_CFLAGS_WARN_ALL([WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wcast-align], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wcast-function-type], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wcomma], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wconditional-type-mismatch], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wconditional-uninitialized], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wconversion], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wduplicate-enum], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wembedded-directive], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wenum-enum-conversion], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wenum-float-conversion], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wextra], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wfloat-equal], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wfor-loop-analysis], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat-nonliteral], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat-signedness], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat-type-confusion], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wformat=2], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Widiomatic-parentheses], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wimplicit-fallthrough], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wlogical-op-parentheses], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wmisleading-indentation], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wnewline-eof], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wredundant-decls], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wreserved-identifier], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wshadow], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wshift-sign-overflow], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wshorten-64-to-32], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wsign-compare], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wsign-conversion], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wsometimes-uninitialized], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wstring-conversion], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wtautological-compare], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wtautological-type-limit-compare], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wundef], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wuninitialized], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunreachable-code-break], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunreachable-code-return], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunreachable-code], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wunused], [WRAP_CFLAGS])
PJL_CHECK_COMPILE_FLAG([-Wwrite-strings], [WRAP_CFLAGS])

# Generate files.
AH_TOP([#ifndef wrap_config_H
#define wrap_config_H])
AH_BOTTOM([#endif /* wrap_config_H */])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([
  Makefile
  lib/Makefile
  src/Makefile
  test/Makefile
  man/Makefile
  man/man1/Makefile
  man/man5/Makefile
])
AC_OUTPUT

# vim:set et sw=2 ts=2:
