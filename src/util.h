/*
**      wrap -- text reformatter
**      util.h
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

#ifndef wrap_util_H
#define wrap_util_H

/**
 * @file
 * Contains utility constants, macros, and functions.
 */

// local
#include "config.h"

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

#define ARRAY_SIZE(A)             (sizeof(A) / sizeof(A[0]))
#define BLOCK(...)                do { __VA_ARGS__ } while (0)
#define CONST_CAST(T,EXPR)        ((T)(EXPR))
#define FREE(PTR)                 free( CONST_CAST( void*, (PTR) ) )
#define NO_OP                     ((void)0)
#define PRINT_ERR(...)            fprintf( stderr, __VA_ARGS__ )
#define SKIP_CHARS(S,CHARS)       ((S) += strspn( (S), (CHARS) ))
#define STRERROR                  strerror( errno )
#define WS_ST                     " \t"       /* Space Tab */
#define WS_STR                    WS_ST "\r"  /* Space Tab Return */
#define WS_STRN                   WS_STR "\n" /* Space Tab Return Newline */

#ifdef __GNUC__

/**
 * Specifies that \a EXPR is \e very likely (as in 99.99% of the time) to be
 * non-zero (true) allowing the compiler to better order code blocks for
 * magrinally better performance.
 *
 * @see http://lwn.net/Articles/255364/
 * @hideinitializer
 */
#define likely(EXPR)              __builtin_expect( !!(EXPR), 1 )

/**
 * Specifies that \a EXPR is \e very unlikely (as in .01% of the time) to be
 * non-zero (true) allowing the compiler to better order code blocks for
 * magrinally better performance.
 *
 * @see http://lwn.net/Articles/255364/
 * @hideinitializer
 */
#define unlikely(EXPR)            __builtin_expect( !!(EXPR), 0 )

#else
# define likely(EXPR)             (EXPR)
# define unlikely(EXPR)           (EXPR)
#endif /* __GNUC__ */

#define MALLOC(TYPE,N) \
  (TYPE*)check_realloc( NULL, sizeof(TYPE) * (N) )

#define PMESSAGE_EXIT(STATUS,FORMAT,...) \
  BLOCK( PRINT_ERR( "%s: " FORMAT, me, __VA_ARGS__ ); exit( STATUS ); )

#define REALLOC(PTR,TYPE,N) \
  (PTR) = (TYPE*)check_realloc( (PTR), sizeof(TYPE) * (N) )

#define W_DUP(FD) BLOCK( \
	if ( unlikely( dup( FD ) == -1 ) ) perror_exit( EX_OSERR ); )

#define W_FERROR(STREAM) BLOCK( \
	if ( unlikely( ferror( STREAM ) ) ) perror_exit( EX_IOERR ); )

#define W_FPRINTF(STREAM,...) BLOCK( \
	if ( unlikely( fprintf( (STREAM), __VA_ARGS__ ) < 0 ) ) perror_exit( EX_IOERR ); )

#define W_FPUTC(C,STREAM) BLOCK( \
	if ( unlikely( putc( (C), (STREAM) ) == EOF ) ) perror_exit( EX_IOERR ); )

#define W_FPUTS(S,STREAM) BLOCK( \
	if ( unlikely( fputs( (S), (STREAM) ) == EOF ) ) perror_exit( EX_IOERR ); )

#define W_FWRITE(BUF,SIZE,NITEMS,STREAM) BLOCK( \
	if ( unlikely( fwrite( (BUF), (SIZE), (NITEMS), (STREAM) ) < (NITEMS) ) ) perror_exit( EX_IOERR ); )

#define W_PIPE(FDS) BLOCK( \
	if ( unlikely( pipe( FDS ) == -1 ) ) perror_exit( EX_OSERR ); )

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
 * Chops off the end-of-line character(s), if any.
 *
 * @param s The null-terminated string to chop.
 * @param s_len The length of \a s.
 * @return Returns the new length of \a s.
 */
size_t chop_eol( char *s, size_t s_len );

/**
 * Given an "opening" character, gets it matching "closing" chcaracter.
 *
 * @param c The "opening" character.
 * @return Returns said "closing" character or the null byte if none.
 */
char closing_char( char c );

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

#ifdef WITH_WIDTH_TERM
/**
 * Gets the number of columns of the terminal.
 *
 * @return Returns said number of columns or 0 if it can not be determined.
 */
unsigned get_term_columns( void );
#endif /* WITH_WIDTH_TERM */

#ifndef NDEBUG
/**
 * Checks whether \a s is an affirmative value.  An affirmative value is one of
 * 1, t, true, y, or yes, case-insensitive.
 *
 * @param s The null-terminated string to check or null.
 * @return Returns \c true only if \a s is affirmative.
 */
bool is_affirmative( char const *s );
#endif /* NDEBUG */

/**
 * Checks whether \a s is any one of \a matches, case-insensitive.
 *
 * @param s The null-terminated string to check or null.
 * @param matches The null-terminated array of values to check against.
 * @return Returns \c true only if \a s is among \a matches.
 */
bool is_any( char const *s, char const *const matches[] );

/**
 * Checks whether \a s is a blank line, that is a line consisting only of
 * whitespace.
 *
 * @param s The null-terminated string to check.
 * @return Returns \c true only if \a s is a blank line.
 */
WRAP_UTIL_INLINE bool is_blank_line( char const *s ) {
  SKIP_CHARS( s, WS_STRN );
  return *s == '\0';
}

/**
 * Checks whether \a s only contains decimal digit characters.
 *
 * @param s The null-terminated string to check.
 * @return Returns \c true only if \a s contains only digits.
 */
WRAP_UTIL_INLINE bool is_digits( char const *s ) {
  return s[ strspn( s, "0123456789" ) ] == '\0';
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
 * Prints an error message for \c errno to standard error and exits.
 *
 * @param status The exit status code.
 */
void perror_exit( int status );

/**
 * Sets the locale for the \c LC_COLLATE and \c LC_CTYPE categories to UTF-8.
 */
void setlocale_utf8( void );

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
 * A variant of strcpy(3) that returns the number of characters copied.
 *
 * @param dst A pointer to receive the copy of \a src.
 * @param src The null-terminated string to copy.
 * @return Returns the number of characters copied.
 */
size_t strcpy_len( char *dst, char const *src );

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
 * Checks the flag: if \c true, resets the flag to \c false.
 *
 * @param flag A pointer to the Boolean flag to be tested and, if \c true, set
 * to \c false.
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
