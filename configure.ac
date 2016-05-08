# Process this file with autoconf to produce a configure script.

##
#     wrap -- text reformatter
#     configure.ac
#
#     Copyright (C) 2013-2016  Paul J. Lucas
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

AC_INIT([wrap], [4.1], [paul@lucasmail.org],,[https://github.com/paul-j-lucas/wrap])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC_C99
gl_EARLY
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
AC_CHECK_HEADERS([sysexits.h])
AC_CHECK_HEADERS([unistd.h])
AC_HEADER_ASSERT
AC_HEADER_STDBOOL
gl_INIT

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct passwd.pw_dir],[],[],[[#include <pwd.h>]])

# Checks for library functions.
AC_FUNC_FNMATCH
AC_FUNC_FORK
AC_FUNC_REALLOC
AC_CHECK_FUNCS([geteuid getpwuid perror strerror strndup])

# Miscellaneous.
AC_LANG(C)
AX_CFLAGS_WARN_ALL
AX_CFLAGS_GCC_OPTION([-Wc++-compat])
AX_CFLAGS_GCC_OPTION([-Wcast-align])
AX_CFLAGS_GCC_OPTION([-Wextra])
AX_CFLAGS_GCC_OPTION([-Winline])
AX_CFLAGS_GCC_OPTION([-Wredundant-decls])
AX_CFLAGS_GCC_OPTION([-Wwrite-strings])

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
