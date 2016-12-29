/*
**      wrap -- text reformatter
**      util.h
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

#ifndef wrap_util_H
#define wrap_util_H

// local
#include "common.h"
#include "config.h"
#include "options.h"

// standard
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for FILE */
#include <stdlib.h>                     /* for exit(3) */
#include <string.h>                     /* for strspn(3) */
#include <sysexits.h>

_GL_INLINE_HEADER_BEGIN
#ifndef WRAP_UTIL_INLINE
# define WRAP_UTIL_INLINE _GL_INLINE
#endif /* WRAP_UTIL_INLINE */

///////////////////////////////////////////////////////////////////////////////

#define BLOCK(...)          do { __VA_ARGS__ } while (0)
#define ERROR_STR           strerror( errno )
#define NO_OP               ((void)0)
#define PERROR_EXIT(STATUS) BLOCK( perror( me ); exit( STATUS ); )
#define PRINT_ERR(...)      fprintf( stderr, __VA_ARGS__ )
#define WS_ST               " \t"       /* Space Tab */
#define WS_STR              WS_ST "\r"  /* Space Tab Return */
#define WS_STRN             WS_STR "\n" /* Space Tab Return Newline */

#define PMESSAGE_EXIT(STATUS,FORMAT,...) \
  BLOCK( PRINT_ERR( "%s: " FORMAT, me, __VA_ARGS__ ); exit( STATUS ); )

#define DUP(FD) \
  BLOCK( if ( dup( FD ) == -1 ) PERROR_EXIT( EX_OSERR ); )

#define FERROR(STREAM) \
  BLOCK( if ( ferror( STREAM ) ) PERROR_EXIT( EX_IOERR ); )

#define FPRINTF(STREAM,...) \
  BLOCK( if ( fprintf( (STREAM), __VA_ARGS__ ) < 0 ) PERROR_EXIT( EX_IOERR ); )

#define FPUTC(C,STREAM) \
  BLOCK( if ( putc( (C), (STREAM) ) == EOF ) PERROR_EXIT( EX_IOERR ); )

#define FPUTS(S,STREAM) \
  BLOCK( if ( fputs( (S), (STREAM) ) == EOF ) PERROR_EXIT( EX_IOERR ); )

#define FWRITE(BUF,SIZE,NITEMS,STREAM) \
  BLOCK( if ( fwrite( (BUF), (SIZE), (NITEMS), (STREAM) ) < (NITEMS) ) PERROR_EXIT( EX_IOERR ); )

#define MALLOC(TYPE,N) \
  (TYPE*)check_realloc( NULL, sizeof(TYPE) * (N) )

#define PIPE(FDS) \
  BLOCK( if ( pipe( FDS ) == -1 ) PERROR_EXIT( EX_OSERR ); )

#define REALLOC(PTR,TYPE,N) \
  (PTR) = (TYPE*)check_realloc( (PTR), sizeof(TYPE) * (N) )

#define SKIP_CHARS(S,CHARS) ((S) += strspn( (S), (CHARS) ))

#define UNGETC(C,STREAM) \
  BLOCK( if ( ungetc( (C), (STREAM) ) == EOF ) PERROR_EXIT( EX_IOERR ); )

typedef char line_buf_t[ LINE_BUF_SIZE ];

// extern variable definitions
extern char const  *me;                 // executable name

////////// extern functions ///////////////////////////////////////////////////

/**
 * Extracts the base portion of a path-name.
 * Unlike \c basename(3):
 *  + Trailing \c '/' characters are not deleted.
 *  + \a path_name is never modified (hence can therefore be \c const).
 *  + Returns a pointer within \a path_name (hence is multi-call safe).
 *
 * @param path_name The path-name to extract the base portion of.
 * @return Returns a pointer to the last component of \a path_name.
 * If \a path_name consists entirely of '/' characters,
 * a pointer to the string "/" is returned.
 */
char const* base_name( char const *path_name );

/**
 * Converts an ASCII string to an unsigned integer.
 * Unlike \c atoi(3), insists that all characters in \a s are digits.
 * If conversion fails, prints an error message and exits.
 *
 * @param s The string to convert.
 * @return Returns the unsigned integer.
 */
unsigned check_atou( char const *s );

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
 * Calls \c realloc(3) and checks for failure.
 * If reallocation fails, prints an error message and exits.
 *
 * @param p The pointer to reallocate.  If NULL, new memory is allocated.
 * @param size The number of bytes to allocate.
 * @return Returns a pointer to the allocated memory.
 */
void* check_realloc( void *p, size_t size );

/**
 * Calls \c strdup(3) and checks for failure.
 * If memory allocation fails, prints an error message and exits.
 *
 * @param s The null-terminated string to duplicate.
 * @return Returns a copy of \a s.
 */
char* check_strdup( char const *s );

/**
 * Copies \a ffrom to \a fto until EOF.
 *
 * @param ffrom The \c FILE to copy from.
 * @param fto The \c FILE to copy to.
 */
void fcopy( FILE *ffrom, FILE *fto );

/**
 * Gets a newline-terminated line from \a ffrom reading at most one fewer
 * characters than that given by \a size.
 *
 * @param buf A pointer to the buffer to receive the line.
 * @param size A pointer to the capacity of \a buf.  On return, it is set to
 * the number of characters read.
 * @param ffrom The \c FILE to read from.
 * @return Returns \a buf if any characters have been read or NULL on either
 * EOF or error.
 */
char* fgetsz( char *buf, size_t *size, FILE *ffrom );

/**
 * Adds a pointer to the head of the free-later-list.
 *
 * @param p The pointer to add.
 * @return Returns \a p.
 */
void* free_later( void *p );

/**
 * Frees all the memory pointed to by all the nodes in the free-later-list.
 */
void free_now( void );

/**
 * Checks whether \a s is a blank like, that is a line consisting only
 * whitespace followed by an end-of-line.
 *
 * @param s The null-terminated string to check.
 * @return Returns \c true only if \a s is a blank line.
 */
WRAP_UTIL_INLINE bool is_blank_line( char const *s ) {
  SKIP_CHARS( s, WS_STRN );
  return !*s;
}

/**
 * Checks whether \a c is an end-of-line character.
 *
 * @param c The character to check.
 * @return Returns \c true only if it is.
 */
WRAP_UTIL_INLINE bool is_eol( char c ) {
  return c == '\n' || c == '\r';
}

/**
 * Checks whether \a c is either a space or a tab only since \r or \n mean the
 * end of a line.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is a space or a tab.
 */
WRAP_UTIL_INLINE bool is_space( char c ) {
  return c == ' ' || c == '\t';
}

/**
 * Gets whether the end-of-lines are Windows' end-of-lines, i.e., \c {CR}{LF}.
 *
 * @return Returns \c true only if end-of-lines are Windows' end-of-lines.
 */
WRAP_UTIL_INLINE bool is_windows() {
  return opt_eol == EOL_WINDOWS;
}

/**
 * Gets whether the line in \a buf is a Windows line, i.e., ends with
 * \c {CR}{LF}.
 *
 * @param buf The buffer to check.
 * @param buf_len The length of \a buf.
 * @return Returns \c true only if \a buf ends with \c {CR}{LF}.
 */
WRAP_UTIL_INLINE bool is_windows_eol( char const buf[], size_t buf_len ) {
  return buf_len >= 2 && buf[ buf_len - 2 ] == '\r';
}

/**
 * Gets the end-of-line string to use.
 *
 * @return Returns said end-of-line string.
 */
WRAP_UTIL_INLINE char const* eol() {
  return (char const*)"\r\n" + !is_windows();
}

/**
 * Splits off the trailing whitespace (tws) from \a buf into \a tws.  For
 * example, if \a buf is initially <code>"# "</code>, it will become \c "#" and
 * \a tws will become <code>" "</code>.
 *
 * @param buf The null-terminated buffer to split off the trailing whitespace
 * from.
 * @param buf_len The length of \a buf.
 * @param tws The buffer to receive the trailing whitespace from \a buf, if
 * any.
 */
void split_tws( char buf[], size_t buf_len, char tws[] );

/**
 * Reverse strspn(3): spans the trailing part of \a s as long as characters
 * from \a s occur in \a set.
 *
 * @param s The null-terminated string to span.
 * @param set The null-terminated set of characters.
 * @return Returns the number of characters spanned.
 */
size_t strrspn( char const *s, char const *set );

/**
 * Converts a string to lower-case in-place.
 *
 * @param s The null-terminated string to convert.
 * @return Returns \a s.
 */
char* tolower_s( char *s );

/**
 * Checks the flag: if \c true, resets the flag to \c false.
 *
 * @param flag A pointer to the Boolean flag to be tested and, if true, reset.
 * @return Returns \c true only if \c *flag is \c true.
 */
WRAP_UTIL_INLINE bool true_reset( bool *flag ) {
  return *flag && !(*flag = false);
}

#ifndef NDEBUG
/**
 * Suspends process execution until a debugger attaches if \a env_var is set
 * and has a case-insensitive value of one of: 1, t, true, y, or yes.
 *
 * @param env_var The environment variable to check for.
 */
void wait_for_debugger_attach( char const *env_var );
#else
# define wait_for_debugger_attach( env_var ) NO_OP
#endif /* NDEBUG */

///////////////////////////////////////////////////////////////////////////////

_GL_INLINE_HEADER_END

#endif /* wrap_util_H */
/* vim:set et sw=2 ts=2: */
