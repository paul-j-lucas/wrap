# Process this file with autoconf to produce a configure script.

##
#       wrap -- text reformatter
#       configure.ac
#
#       Copyright (C) 2013-2019  Paul J. Lucas
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
AC_INIT([wrap], [5.7], [https://github.com/paul-j-lucas/wrap/issues],,[https://github.com/paul-j-lucas/wrap])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_LANG(C)
AC_PROG_CC
AS_IF([test "x$ac_cv_prog_cc_c11" = xno],
  [AC_MSG_ERROR([a C11 compiler is required to compile $PACKAGE_NAME])])
gl_EARLY
AC_PROG_INSTALL

# Program feature: --width-term (enabled by default)
AC_ARG_ENABLE([width-term],
  AS_HELP_STRING([--disable-width-term], [disable support for --width=term]),
  [],
  [enable_width_term=yes]
)
AS_IF([test "x$enable_width_term" = xyes],
  [AC_DEFINE([WITH_WIDTH_TERM], [1],
    [Define to 1 if --width=term is enabled.])]
)

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([ctype.h])
AS_IF([test "x$enable_width_term" = xyes],
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
AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([fnmatch.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([pwd.h])
AC_CHECK_HEADERS([regex.h])
AC_CHECK_HEADERS([signal.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([sysexits.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([wctype.h])
AC_HEADER_ASSERT
AC_HEADER_STDBOOL
gl_INIT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
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
PJL_COMPILE([__typeof__],[],[__typeof__(1) x __attribute((unused)) = 1;])

# Checks for library functions.
AC_FUNC_FNMATCH
AC_FUNC_FORK
AC_FUNC_REALLOC
AC_CHECK_FUNCS([geteuid getpwuid perror strerror strndup])
AS_IF([test "x$enable_width_term" = xyes],
  [
    AC_SEARCH_LIBS([endwin],[curses ncurses], [],
      [AC_MSG_ERROR([curses library for --width=term not found; use --disable-width-term])]
    )
  ]
)

# Makefile conditionals.

# Miscellaneous.
AX_C___ATTRIBUTE__

# Compiler warnings.
AX_CFLAGS_WARN_ALL
AX_CHECK_COMPILE_FLAG([-Wcast-align], [CFLAGS="$CFLAGS -Wcast-align"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wcast-function-type], [CFLAGS="$CFLAGS -Wcast-function-type"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wcomma], [CFLAGS="$CFLAGS -Wcomma"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wconditional-type-mismatch], [CFLAGS="$CFLAGS -Wconditional-type-mismatch"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wconditional-uninitialized], [CFLAGS="$CFLAGS -Wconditional-uninitialized"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wconversion], [CFLAGS="$CFLAGS -Wconversion"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wduplicate-enum], [CFLAGS="$CFLAGS -Wduplicate-enum"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wembedded-directive], [CFLAGS="$CFLAGS -Wembedded-directive"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wenum-enum-conversion], [CFLAGS="$CFLAGS -Wenum-enum-conversion"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wenum-float-conversion], [CFLAGS="$CFLAGS -Wenum-float-conversion"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wextra], [CFLAGS="$CFLAGS -Wextra"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wfloat-equal], [CFLAGS="$CFLAGS -Wfloat-equal"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wfor-loop-analysis], [CFLAGS="$CFLAGS -Wfor-loop-analysis"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wformat-nonliteral], [CFLAGS="$CFLAGS -Wformat-nonliteral"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wformat-signedness], [CFLAGS="$CFLAGS -Wformat-signedness"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wformat-type-confusion], [CFLAGS="$CFLAGS -Wformat-type-confusion"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wformat=2], [CFLAGS="$CFLAGS -Wformat=2"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Widiomatic-parentheses], [CFLAGS="$CFLAGS -Widiomatic-parentheses"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wimplicit-fallthrough], [CFLAGS="$CFLAGS -Wimplicit-fallthrough"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wlogical-op-parentheses], [CFLAGS="$CFLAGS -Wlogical-op-parentheses"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wmisleading-indentation], [CFLAGS="$CFLAGS -Wmisleading-indentation"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wnewline-eof], [CFLAGS="$CFLAGS -Wnewline-eof"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wpedantic], [CFLAGS="$CFLAGS -Wpedantic"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wredundant-decls], [CFLAGS="$CFLAGS -Wredundant-decls"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wreserved-identifier], [CFLAGS="$CFLAGS -Wreserved-identifier"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wshadow], [CFLAGS="$CFLAGS -Wshadow"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wshift-sign-overflow], [CFLAGS="$CFLAGS -Wshift-sign-overflow"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wshorten-64-to-32], [CFLAGS="$CFLAGS -Wshorten-64-to-32"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wsign-compare], [CFLAGS="$CFLAGS -Wsign-compare"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wsign-conversion], [CFLAGS="$CFLAGS -Wsign-conversion"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wsometimes-uninitialized], [CFLAGS="$CFLAGS -Wsometimes-uninitialized"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wstring-conversion], [CFLAGS="$CFLAGS -Wstring-conversion"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wtautological-compare], [CFLAGS="$CFLAGS -Wtautological-compare"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wtautological-type-limit-compare], [CFLAGS="$CFLAGS -Wtautological-type-limit-compare"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wundef], [CFLAGS="$CFLAGS -Wundef"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wuninitialized], [CFLAGS="$CFLAGS -Wuninitialized"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wunreachable-code-break], [CFLAGS="$CFLAGS -Wunreachable-code-break"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wunreachable-code-return], [CFLAGS="$CFLAGS -Wunreachable-code-return"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wunreachable-code], [CFLAGS="$CFLAGS -Wunreachable-code"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wunused], [CFLAGS="$CFLAGS -Wunused"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wwrite-strings], [CFLAGS="$CFLAGS -Wwrite-strings"], [], [-Werror])
AX_CHECK_COMPILE_FLAG([-Wzero-as-null-pointer-constant], [CFLAGS="$CFLAGS -Wzero-as-null-pointer-constant"], [], [-Werror])

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
