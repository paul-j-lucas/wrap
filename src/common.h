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

// local
#include "config.h"

// standard
#include <stdlib.h>                     /* for EXIT_SUCCESS */

///////////////////////////////////////////////////////////////////////////////

#define CONF_FILE_NAME            "." PACKAGE "rc"
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

/**
 * From Wikipedia: The data link escape character (DLE) was intended to be a
 * signal to the other end of a data link that the following character is a
 * control character such as STX or ETX. For example a packet may be structured
 * in the following way (DLE)<STX><PAYLOAD>(DLE)<ETX>.
 *
 * Wrap and Wrapc use it to signal the start of an interprocess message between
 * them.
 */
#define ASCII_DLE                 '\x10'

/**
 * From Wikipedia: The end of transmission block character (ETB) was used to
 * indicate the end of a block of data, where data was divided into such blocks
 * for transmission purposes.
 *
 * Wrap and Wrapc use it immediately after DLE to indicate the end of the block
 * of text to be wrapped.  Any additional text is passed through verbatim.
 */
#define ASCII_ETB                 '\x03'

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
