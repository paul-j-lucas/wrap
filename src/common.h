/*
**      wrap -- text reformatter
**      common.h
**
**      Copyright (C) 1996-2015  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the Licence, or
**      (at your option) any later version.
**
**      This program is distributed in the hope that it will be useful,
**      but WITHOUT ANY WARRANTY; without even the implied warranty of
**      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**      GNU General Public License for more details.
**
**      You should have received a copy of the GNU General Public License
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef wrap_common_H
#define wrap_common_H

// standard
#include <stdlib.h>                     /* for EXIT_SUCCESS */

///////////////////////////////////////////////////////////////////////////////

#define LINE_BUF_SIZE             8192  /* hopefully, no one will exceed this */
#define LINE_WIDTH_DEFAULT        80    /* wrap text to this line width */
#define LINE_WIDTH_MINIMUM        1
#define NEWLINES_DELIMIT_DEFAULT  2     /* # newlines that delimit a para */
#define TAB_SPACES_DEFAULT        8     /* number of spaces a tab equals */

#define EXIT_USAGE                1
#define EXIT_CONF_ERROR           2
#define EXIT_OUT_OF_MEMORY        3
#define EXIT_READ_OPEN            10
#define EXIT_READ_ERROR           11
#define EXIT_WRITE_OPEN           12
#define EXIT_WRITE_ERROR          13
#define EXIT_FORK_ERROR           20
#define EXIT_EXEC_ERROR           21
#define EXIT_CHILD_SIGNAL         30
#define EXIT_PIPE_ERROR           40

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
