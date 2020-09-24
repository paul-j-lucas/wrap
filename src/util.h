/*
**      wrap -- text reformatter
**      src/util.h
**
**      Copyright (C) 1996-2019  Paul J. Lucas
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
#include "wrap.h"                       /* must go first */

// standard
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdint.h>                     /* for uintptr_t */
#include <stdio.h>                      /* for FILE */
#include <stdlib.h>                     /* for exit(3) */
#include <stdnoreturn.h>
#include <string.h>                     /* for strspn(3) */
#include <sysexits.h>

_GL_INLINE_HEADER_BEGIN
#ifndef W_UTIL_INLINE
# define W_UTIL_INLINE _GL_INLINE
#endif /* W_UTIL_INLINE */

///////////////////////////////////////////////////////////////////////////////

/** Gets the number of elements of the given array. */
#define ARRAY_SIZE(A)             (sizeof(A) / sizeof(A[0]))

/** Embeds the given statements into a compount statement block. */
#define BLOCK(...)                do { __VA_ARGS__ } while (0)

/** Explicit C version of C++'s `const_cast`. */
#define CONST_CAST(T,EXPR)        ((T)(EXPR))

/** Frees the given memory. */
#define FREE(PTR)                 free( CONST_CAST( void*, (PTR) ) )

/** Frees the newly allocated C string later. */
#define FREE_STRBUF_LATER(SIZE)   FREE_STR_LATER( MALLOC( char, (SIZE) ) )

/** Frees the given C string later. */
#define FREE_STR_LATER(PTR)       REINTERPRET_CAST( char*, free_later( PTR ) )

/** Frees the duplicated C string later. */
#define FREE_STRDUP_LATER(PTR)    FREE_STR_LATER( check_strdup( PTR ) )

/**
 * Evaluates \a EXPR: if it returns `true`, calls perror_exit() with \a ERR.
 *
 * @param EXPR The expression to evaluate.
 * @param ERR The exit status code to use.
 */
#define IF_EXIT(EXPR,ERR) \
  BLOCK( if ( unlikely( EXPR ) ) perror_exit( ERR ); )

/** No-operation statement.  (Useful for a `goto` target.) */
#define NO_OP                     ((void)0)

/** Shorthand for printing to standard error. */
#define PRINT_ERR(...)            fprintf( stderr, __VA_ARGS__ )

/** Explicit C version of C++'s `reinterpret_cast`. */
#define REINTERPRET_CAST(T,EXPR)  ((T)(uintptr_t)(EXPR))

/** Advances \a S over all \a CHARS. */
#define SKIP_CHARS(S,CHARS)       ((S) += strspn( (S), (CHARS) ))

/** Explicit C version of C++'s `static_cast`. */
#define STATIC_CAST(T,EXPR)       ((T)(EXPR))

/** Shorthand for calling **strerror**(3). */
#define STRERROR                  strerror( errno )

#define WS_ST                     " \t"       /**< Space Tab. */
#define WS_STR                    WS_ST "\r"  /**< Space Tab Return. */
#define WS_STRN                   WS_STR "\n" /**< Space Tab Return Newline. */

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
  STATIC_CAST(TYPE*, check_realloc( NULL, sizeof(TYPE) * (N) ))

#define PMESSAGE_EXIT(STATUS,FORMAT,...) \
  BLOCK( PRINT_ERR( "%s: " FORMAT, me, __VA_ARGS__ ); exit( STATUS ); )

#define REALLOC(PTR,TYPE,N) \
  (PTR) = STATIC_CAST(TYPE*, check_realloc( (PTR), sizeof(TYPE) * (size_t)(N) ))

#define W_DUP(FD)                 IF_EXIT( dup( FD ) == -1, EX_OSERR )

#define W_FERROR(STREAM)          IF_EXIT( ferror( STREAM ), EX_IOERR )

#define W_FPRINTF(STREAM,...) \
	IF_EXIT( fprintf( (STREAM), __VA_ARGS__ ) < 0, EX_IOERR )

#define W_FPUTC(C,STREAM) \
	IF_EXIT( putc( (C), (STREAM) ) == EOF, EX_IOERR )

#define W_FPUTS(S,STREAM) \
	IF_EXIT( fputs( (S), (STREAM) ) == EOF, EX_IOERR )

#define W_FWRITE(BUF,SIZE,NITEMS,STREAM) \
	IF_EXIT( fwrite( (BUF), (SIZE), (NITEMS), (STREAM) ) < (NITEMS), EX_IOERR )

#define W_PIPE(FDS)               IF_EXIT( pipe( FDS ) == -1, EX_OSERR )

// extern variable definitions
extern char const  *me;                 ///< Program name.

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
W_WARN_UNUSED_RESULT
char const* base_name( char const *path_name );

/**
 * Performs a binary search looking for \a key.
 *
 * @param key The key to search for.
 * @param elt_base A pointer to the base address of the elements to search.
 * The elements must be sorted ascendingly according to \a elt_cmp.
 * @param elt_count The number of elements.
 * @param elt_size The size in bytes of each element.
 * @param elt_cmp A pointer to a comparison function that must return an
 * integer less than zero, zero, or greater thatn zero if the key is less than,
 * equal to, or greater than a particular element, respectively.
 * @return Returns a pointer to the element matching \a key or null if not
 * found.
 *
 * @note \a key need not have the same type as the elements in the array, e.g.,
 * \a key could be a string and \a elt_cmp could compare the string with a
 * struct's field.
 */
W_WARN_UNUSED_RESULT
void const* bin_search( void const *key, void const *elt_base,
                        size_t elt_count, size_t elt_size,
                        int (*elt_cmp)( void const *key, void const *elt ) );

/**
 * Comparison function for bin_search that compares a string key against an
 * element of array of constant pointer to constant char.
 *
 * @param key A pointer to the string being searched for.
 * @param str_ptr A pointer to the pointer to the string of the current element
 * to compare against.
 * @return Returns an integer less than zero, zero, or greater thatn zero if
 * the key is less than, equal to, or greater than the element, respectively.
 */
W_WARN_UNUSED_RESULT
int bin_search_str_strptr_cmp( void const *key, void const *str_ptr );

/**
 * Converts an ASCII string to an unsigned integer.
 * Unlike \c atoi(3), insists that all characters in \a s are digits.
 * If conversion fails, prints an error message and exits.
 *
 * @param s The string to convert.
 * @return Returns the unsigned integer.
 */
W_WARN_UNUSED_RESULT
unsigned check_atou( char const *s );

/**
 * Calls \c realloc(3) and checks for failure.
 * If reallocation fails, prints an error message and exits.
 *
 * @param p The pointer to reallocate.  If NULL, new memory is allocated.
 * @param size The number of bytes to allocate.
 * @return Returns a pointer to the allocated memory.
 */
W_WARN_UNUSED_RESULT
void* check_realloc( void *p, size_t size );

/**
 * Calls \c strdup(3) and checks for failure.
 * If memory allocation fails, prints an error message and exits.
 *
 * @param s The null-terminated string to duplicate.
 * @return Returns a copy of \a s.
 */
W_WARN_UNUSED_RESULT
char* check_strdup( char const *s );

/**
 * Chops off the end-of-line character(s), if any.
 *
 * @param s The null-terminated string to chop.
 * @param s_len The length of \a s.
 * @return Returns the new length of \a s.
 */
W_WARN_UNUSED_RESULT
size_t chop_eol( char *s, size_t s_len );

/**
 * Given an "opening" character, gets it matching "closing" chcaracter.
 *
 * @param c The "opening" character.
 * @return Returns said "closing" character or the null byte if none.
 */
W_WARN_UNUSED_RESULT
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
W_WARN_UNUSED_RESULT
char* fgetsz( char *buf, size_t *size, FILE *ffrom );

/**
 * Adds a pointer to the head of the free-later-list.
 *
 * @param p The pointer to add.
 * @return Returns \a p.
 */
W_NOWARN_UNUSED_RESULT
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
W_WARN_UNUSED_RESULT
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
W_WARN_UNUSED_RESULT
bool is_affirmative( char const *s );
#endif /* NDEBUG */

/**
 * Checks whether \a s is any one of \a matches, case-insensitive.
 *
 * @param s The null-terminated string to check or null.
 * @param matches The null-terminated array of values to check against.
 * @return Returns \c true only if \a s is among \a matches.
 */
W_WARN_UNUSED_RESULT
bool is_any( char const *s, char const *const matches[] );

/**
 * Checks whether \a s is a blank line, that is a line consisting only of
 * whitespace.
 *
 * @param s The null-terminated string to check.
 * @return Returns \c true only if \a s is a blank line.
 */
W_WARN_UNUSED_RESULT W_UTIL_INLINE
bool is_blank_line( char const *s ) {
  SKIP_CHARS( s, WS_STRN );
  return *s == '\0';
}

/**
 * Checks whether \a s only contains decimal digit characters.
 *
 * @param s The null-terminated string to check.
 * @return Returns \c true only if \a s contains only digits.
 */
W_WARN_UNUSED_RESULT W_UTIL_INLINE
bool is_digits( char const *s ) {
  return s[ strspn( s, "0123456789" ) ] == '\0';
}

/**
 * Checks whether \a c is an end-of-line character.
 *
 * @param c The character to check.
 * @return Returns \c true only if it is.
 */
W_WARN_UNUSED_RESULT W_UTIL_INLINE
bool is_eol( char c ) {
  return c == '\n' || c == '\r';
}

/**
 * Checks whether \a c is either a space or a tab only since \r or \n mean the
 * end of a line.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is a space or a tab.
 */
W_WARN_UNUSED_RESULT W_UTIL_INLINE
bool is_space( char c ) {
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
W_WARN_UNUSED_RESULT W_UTIL_INLINE
bool is_windows_eol( char const buf[], size_t buf_len ) {
  return buf_len >= 2 && buf[ buf_len - 2 ] == '\r';
}

/**
 * Prints an error message for \c errno to standard error and exits.
 *
 * @param status The exit status code.
 */
noreturn void perror_exit( int status );

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
W_WARN_UNUSED_RESULT
size_t strcpy_len( char *dst, char const *src );

/**
 * Reverse strspn(3): spans the trailing part of \a s as long as characters
 * from \a s occur in \a set.
 *
 * @param s The null-terminated string to span.
 * @param set The null-terminated set of characters.
 * @return Returns the number of characters spanned.
 */
W_WARN_UNUSED_RESULT
size_t strrspn( char const *s, char const *set );

/**
 * Checks the flag: if \c true, resets the flag to \c false.
 *
 * @param flag A pointer to the Boolean flag to be tested and, if \c true, set
 * to \c false.
 * @return Returns \c true only if \c *flag is \c true.
 */
W_WARN_UNUSED_RESULT W_UTIL_INLINE
bool true_reset( bool *flag ) {
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
