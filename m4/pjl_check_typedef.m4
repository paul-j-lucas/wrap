# SYNOPSIS
#
#   PJL_CHECK_TYPEDEF(TYPEDEF, HEADER [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]])
#
# DESCRIPTION
#
#   Check if the given typedef-name is recognized as a type.  Unlike
#   AX_CHECK_TYPEDEF, this macro actually does ACTION-IF-FOUND or ACTION-IF-
#   NOT-FOUND.
#
# LICENSE
#
#   Copyright (C) 2019 Paul J. Lucas <paul@lucasmail.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

AC_DEFUN([PJL_CHECK_TYPEDEF],
[
  AC_REQUIRE([AX_CHECK_TYPEDEF])
  AC_MSG_CHECKING([for $1 in $2])
  AX_CHECK_TYPEDEF_($1,$2,
    [
      AC_MSG_RESULT([yes])
      $3
    ],
    [
      AC_MSG_RESULT([no])
      $4
    ]
  )
])

# vim:set et sw=2 ts=2:
