/*
**      wrap -- text reformatter
**      common.h
**
**      Copyright (C) 1996-2016  Paul J. Lucas
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
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for FILE */

///////////////////////////////////////////////////////////////////////////////

#define CONF_FILE_NAME            "." PACKAGE "rc"
#define LINE_BUF_SIZE             8192  /* hopefully, no one will exceed this */
#define LINE_WIDTH_DEFAULT        80    /* wrap text to this line width */
#define LINE_WIDTH_MINIMUM        1
#define NEWLINES_DELIMIT_DEFAULT  2     /* # newlines that delimit a para */
#define TAB_SPACES_DEFAULT        8     /* number of spaces a tab equals */

/**
 * From Wikipedia: The data link escape character (DLE) was intended to be a
 * signal to the other end of a data link that the following character is a
 * control character such as STX or ETX. For example a packet may be structured
 * in the following way (DLE)(STX)(PAYLOAD)(DLE)(ETX).
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
#define ASCII_ETB                 '\x17'

/**
 * Uncomment the line below to debug Markdown handling by printing state
 * information to stderr.
 */
//#define DEBUG_MARKDOWN

#ifdef DEBUG_MARKDOWN
# define MD_DEBUG(...)      PRINT_ERR( __VA_ARGS__ )
#else
# define MD_DEBUG(...)      (void)0     /* need something to eat the ';' */
#endif /* DEBUG_MARKDOWN */

typedef char line_buf_t[ LINE_BUF_SIZE ];

////////// extern functions ///////////////////////////////////////////////////

/**
 * Cleans up all data and closes files.
 */
void clean_up( void );

/**
 * Reads a newline-terminated line from \a ffrom.
 *
 * @param line The line buffer to read into.
 * @param ffrom The \c FILE to read from.
 * @return Returns the number of characters read.
 */
size_t buf_read( line_buf_t line, FILE *ffrom );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
