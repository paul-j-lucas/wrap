# Process this file with autoconf to produce a configure script.

##
#     wrap -- text reformatter
#     configure.ac
#
#     Copyright (C) 2013  Paul J. Lucas
#
#     This program is free software; you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation; either version 2 of the Licence, or
#     (at your option) any later version.
#     
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#     
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

AC_INIT([wrap], [3.0], [paul@lucasmail.org])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([ctype.h])
AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([fnmatch.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([pwd.h])
AC_CHECK_HEADERS([signal.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([unistd.h])
AC_HEADER_STDBOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct passwd.pw_dir],[],[],[[#include <pwd.h>]])

# Checks for library functions.
AC_FUNC_FNMATCH
AC_FUNC_FORK
AC_FUNC_REALLOC
AC_CHECK_FUNCS([geteuid getpwuid strerror strndup])

# If fnmatch(3) is defined, define an automake variable that can be used to
# compile pattern.c conditionally.
AM_CONDITIONAL([am_WITH_PATTERNS], [test x$ac_cv_func_fnmatch_works = xyes])

# If fnmatch(3) is defined, define a C macro in config.h that has a more
# intuitive name that HAS_FN_MATCH.
if test x$ac_cv_func_fnmatch_works = xyes
then AC_DEFINE([WITH_PATTERNS],[1],[Define to 1 if patterns via fnmatch(3) are supported.])
fi

# Miscellaneous.
AC_LANG(C)
AX_CFLAGS_WARN_ALL
AX_CHECK_COMPILE_FLAG([-Wextra],[CFLAGS+=" -Wextra"])
AX_CHECK_COMPILE_FLAG([-Wc++-compat],[CFLAGS+=" -Wc++-compat"])
AX_CHECK_COMPILE_FLAG([-Wredundant-decls],[CFLAGS+=" -Wredundant-decls"])
AX_CHECK_COMPILE_FLAG([-Wwrite-strings],[CFLAGS+=" -Wwrite-strings"])

# Generate files.
AH_TOP([#ifndef wrap_config_H
#define wrap_config_H])
AH_BOTTOM([#endif /* wrap_config_H */])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  test/Makefile
  man/Makefile
  man/man1/Makefile
  man/man5/Makefile
])
AC_OUTPUT

# vim:set et sw=2 ts=2:
