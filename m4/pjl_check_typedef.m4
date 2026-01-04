##
# SYNOPSIS
#
#     PJL_CHECK_TYPEDEF(TYPEDEF, HEADER [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]])
#
# DESCRIPTION
#
#     Check if the given typedef-name is recognized as a type.  Unlike
#     AX_CHECK_TYPEDEF, this macro actually does ACTION-IF-FOUND or ACTION-IF-
#     NOT-FOUND.
#
# LICENSE
#
#     Copyright (C) 2019-2026  Paul J. Lucas
#
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
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
