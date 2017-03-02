/*
**      wrap -- text reformatter
**      common.h
**
**      Copyright (C) 1996-2017  Paul J. Lucas
**
**      This program is free software: you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation, either version 3 of the License, or
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
#include "config.h"                     /* must go first */
#include "options.h"                    /* for opt_eol */

// standard
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for FILE */

/**
 * @file
 * Contains constants, macros, typedefs, and functions common to both wrap and
 * wrapc.
 */

_GL_INLINE_HEADER_BEGIN
#ifndef WRAP_COMMON_INLINE
# define WRAP_COMMON_INLINE _GL_INLINE
#endif /* WRAP_COMMON_INLINE */

///////////////////////////////////////////////////////////////////////////////

#define CONF_FILE_NAME            "." PACKAGE "rc"
#define EOS_SPACES_DEFAULT        2     /* # spaces after end-of-sentence */
#define LINE_BUF_SIZE             8192  /* hopefully, no one will exceed this */
#define LINE_WIDTH_DEFAULT        80    /* wrap text to this line width */
#define LINE_WIDTH_MINIMUM        1
#define NEWLINES_DELIMIT_DEFAULT  2     /* # newlines that delimit a para */
#define TAB_SPACES_DEFAULT        8     /* number of spaces a tab equals */

typedef char line_buf_t[ LINE_BUF_SIZE ];

////////// Interprocess Communication (IPC) ///////////////////////////////////

/**
 * From Wikipedia: The data link escape character (DLE) was intended to be a
 * signal to the other end of a data link that the following character is a
 * control character such as STX or ETX. For example a packet may be structured
 * in the following way (DLE)(STX)(PAYLOAD)(DLE)(ETX).
 */
#define ASCII_DLE                 '\x10'

/**
 * From Wikipedia: The end of transmission block character (ETB) was used to
 * indicate the end of a block of data, where data was divided into such blocks
 * for transmission purposes.
 */
#define ASCII_ETB                 '\x17'

/**
 * From Wikipedia: The start of heading (SOH) character was to mark a non-data
 * section of a data stream—the part of a stream containing addresses and other
 * housekeeping data.
 */
#define ASCII_SOH                 '\x01'

/**
 * IPC code to in-band signal the start of an IPC message.  It \e must be
 * immediately followed by another IPC code that indicates the type of message.
 */
#define WIPC_HELLO                ASCII_DLE

/**
 * IPC code to signal the end of the block of text to be wrapped.  Any text
 * sent after this is passed through verbatim.
 */
#define WIPC_END_WRAP             ASCII_ETB

/**
 * IPC code to signal a change in the leading comment characters and/or
 * whitespace.  It \e must be terminated by a newline.
 */
#define WIPC_NEW_LEADER           ASCII_SOH

/**
 * Formats and sends an Interprocess Communication (IPC) message.
 * @hideinitializer
 */
#define WIPC_SENDF(STREAM,CODE,FORMAT,...) \
  W_FPRINTF( (STREAM), ("%c%c" FORMAT), WIPC_HELLO, (CODE), __VA_ARGS__ )

/**
 * Character used to separate fields in an Interprocess Communication (IPC)
 * message.
 */
#define WIPC_SEP                  "|"

////////// extern functions ///////////////////////////////////////////////////

/**
 * Computes the width of a character where tabs have a width of
 * \c opt_tab_spaces spaces minus the number of spaces we're into a tab-stop;
 * all others characters have a width of 1.
 *
 * @param c The character to get the width of.
 * @param width The line width so far.
 * @return Returns said width.
 */
WRAP_COMMON_INLINE size_t char_width( char c, size_t width ) {
  return c == '\t' ? opt_tab_spaces - width % opt_tab_spaces : 1;
}

/**
 * Reads a newline-terminated line from \a ffrom.
 * If reading fails, prints an error message and exits.
 *
 * @param line The line buffer to read into.
 * @param ffrom The \c FILE to read from.
 * @return Returns the number of characters read.
 */
size_t check_readline( line_buf_t line, FILE *ffrom );

/**
 * Cleans up all data and closes files.
 */
void common_cleanup( void );

/**
 * Gets the end-of-line string to use.
 *
 * @return Returns said end-of-line string.
 */
WRAP_COMMON_INLINE char const* eol( void ) {
  return (char const*)"\r\n" + (opt_eol != EOL_WINDOWS);
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Uncomment the line below to debug Markdown handling by printing state
 * information to stderr.
 */
//#define DEBUG_MARKDOWN

#ifdef DEBUG_MARKDOWN
# define MD_DEBUG(...)      PRINT_ERR( __VA_ARGS__ )
#else
# define MD_DEBUG(...)      NO_OP
#endif /* DEBUG_MARKDOWN */

///////////////////////////////////////////////////////////////////////////////

_GL_INLINE_HEADER_END

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
