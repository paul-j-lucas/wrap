/*
**      wrap -- text reformatter
**      src/util.h
**
**      Copyright (C) 1996-2023  Paul J. Lucas
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
#include "pjl_config.h"                 /* must go first */

// standard
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdint.h>                     /* for uintptr_t */
#include <stdio.h>                      /* for FILE */
#include <stdlib.h>                     /* for exit(3) */
#include <string.h>                     /* for strspn(3) */
#include <sysexits.h>

_GL_INLINE_HEADER_BEGIN
#ifndef W_UTIL_INLINE
# define W_UTIL_INLINE _GL_INLINE
#endif /* W_UTIL_INLINE */

///////////////////////////////////////////////////////////////////////////////

/// @cond DOXYGEN_IGNORE

#define CHARIFY_0 '0'
#define CHARIFY_1 '1'
#define CHARIFY_2 '2'
#define CHARIFY_3 '3'
#define CHARIFY_4 '4'
#define CHARIFY_5 '5'
#define CHARIFY_6 '6'
#define CHARIFY_7 '7'
#define CHARIFY_8 '8'
#define CHARIFY_9 '9'
#define CHARIFY_A 'A'
#define CHARIFY_B 'B'
#define CHARIFY_C 'C'
#define CHARIFY_D 'D'
#define CHARIFY_E 'E'
#define CHARIFY_F 'F'
#define CHARIFY_G 'G'
#define CHARIFY_H 'H'
#define CHARIFY_I 'I'
#define CHARIFY_J 'J'
#define CHARIFY_K 'K'
#define CHARIFY_L 'L'
#define CHARIFY_M 'M'
#define CHARIFY_N 'N'
#define CHARIFY_O 'O'
#define CHARIFY_P 'P'
#define CHARIFY_Q 'Q'
#define CHARIFY_R 'R'
#define CHARIFY_S 'S'
#define CHARIFY_T 'T'
#define CHARIFY_U 'U'
#define CHARIFY_V 'V'
#define CHARIFY_W 'W'
#define CHARIFY_X 'X'
#define CHARIFY_Y 'Y'
#define CHARIFY_Z 'Z'
#define CHARIFY__ '_'
#define CHARIFY_a 'a'
#define CHARIFY_b 'b'
#define CHARIFY_c 'c'
#define CHARIFY_d 'd'
#define CHARIFY_e 'e'
#define CHARIFY_f 'f'
#define CHARIFY_g 'g'
#define CHARIFY_h 'h'
#define CHARIFY_i 'i'
#define CHARIFY_j 'j'
#define CHARIFY_k 'k'
#define CHARIFY_l 'l'
#define CHARIFY_m 'm'
#define CHARIFY_n 'n'
#define CHARIFY_o 'o'
#define CHARIFY_p 'p'
#define CHARIFY_q 'q'
#define CHARIFY_r 'r'
#define CHARIFY_s 's'
#define CHARIFY_t 't'
#define CHARIFY_u 'u'
#define CHARIFY_v 'v'
#define CHARIFY_w 'w'
#define CHARIFY_x 'x'
#define CHARIFY_y 'y'
#define CHARIFY_z 'z'

#define CHARIFY_IMPL(X)           CHARIFY_##X
#define STRINGIFY_IMPL(X)         #X

/// @endcond

/**
 * Gets the number of elements of the given array.
 *
 * @param A The array to get the number of elements of.
 *
 * @note \a A _must_ be a statically allocated array.
 */
#define ARRAY_SIZE(A) (     \
  sizeof(A) / sizeof(0[A])  \
  * STATIC_ASSERT_EXPR( IS_ARRAY(A), #A " must be an array" ))

#ifndef NDEBUG
/**
 * Asserts that this line of code is run at most once &mdash; useful in
 * initialization functions that must be called at most once.  For example:
 *
 *      void initialize() {
 *        ASSERT_RUN_ONCE();
 *        // ...
 *      }
 */
#define ASSERT_RUN_ONCE() BLOCK(    \
  static bool UNIQUE_NAME(called);  \
  assert( !UNIQUE_NAME(called) );   \
  UNIQUE_NAME(called) = true; )
#else
#define ASSERT_RUN_ONCE()         NO_OP
#endif /* NDEBUG */

/**
 * Embeds the given statements into a compound statement block.
 *
 * @param ... The statement(s) to embed.
 */
#define BLOCK(...)                do { __VA_ARGS__ } while (0)

/**
 * Macro that "char-ifies" its argument, e.g., <code>%CHARIFY(x)</code> becomes
 * `'x'`.
 *
 * @param X The unquoted character to charify.  It can be only in the set
 * `[0-9_A-Za-z]`.
 *
 * @sa #STRINGIFY()
 */
#define CHARIFY(X)                CHARIFY_IMPL(X)

/**
 * C version of C++'s `const_cast`.
 *
 * @param T The type to cast to.
 * @param EXPR The expression to cast.
 *
 * @note This macro can't actually implement C++'s `const_cast` because there's
 * no way to do it in C.  It serves merely as a visual cue for the type of cast
 * meant.
 *
 * @sa #POINTER_CAST()
 * @sa #STATIC_CAST()
 */
#define CONST_CAST(T,EXPR)        ((T)(EXPR))

/**
 * Calls **dup**(2), check for an error, and exits if there was one.
 *
 * @param FD The file descriptor to duplicate.
 */
#define DUP(FD)                   PERROR_EXIT_IF( dup( FD ) == -1, EX_OSERR )

/**
 * Shorthand for printing to standard error.
 *
 * @param ... The `printf()` arguments.
 *
 * @sa #FPRINTF()
 * @sa #PRINTF()
 * @sa #EPUTC()
 */
#define EPRINTF(...)              fprintf( stderr, __VA_ARGS__ )

/**
 * Shorthand for printing \a C to standard error.
 *
 * @param C The character to print.
 *
 * @sa #EPRINTF()
 * @sa #FPUTC()
 * @sa #PUTC()
 */
#define EPUTC(C)                  FPUTC( C, stderr )

/**
 * Calls **ferror**(3) and exits if there was an error on \a STREAM.
 *
 * @param STREAM The `FILE` stream to check for an error.
 *
 * @sa #PERROR_EXIT_IF()
 */
#define FERROR(STREAM)            PERROR_EXIT_IF( ferror( STREAM ), EX_IOERR )

/**
 * Shorthand for printing to standard output.
 *
 * @param STREAM The `FILE` stream to print to.
 * @param ... The `printf()` arguments.
 *
 * @sa #EPRINTF()
 * @sa #PERROR_EXIT_IF()
 */
#define FPRINTF(STREAM,...) \
	PERROR_EXIT_IF( fprintf( (STREAM), __VA_ARGS__ ) < 0, EX_IOERR )

/**
 * Calls **putc**(3), checks for an error, and exits if there was one.
 *
 * @param C The character to print.
 * @param STREAM The `FILE` stream to print to.
 *
 * @sa #EPUTC()
 * @sa #FPRINTF()
 * @sa #FPUTS()
 * @sa #PERROR_EXIT_IF()
 * @sa #PUTC()
 */
#define FPUTC(C,STREAM) \
	PERROR_EXIT_IF( putc( (C), (STREAM) ) == EOF, EX_IOERR )

/**
 * Calls **fputs**(3), checks for an error, and exits if there was one.
 *
 * @param S The C string to print.
 * @param STREAM The `FILE` stream to print to.
 *
 * @sa #FPRINTF()
 * @sa #FPUTC()
 * @sa #PERROR_EXIT_IF()
 * @sa #PUTS()
 */
#define FPUTS(S,STREAM) \
	PERROR_EXIT_IF( fputs( (S), (STREAM) ) == EOF, EX_IOERR )

/**
 * Frees the given memory.
 *
 * @param PTR The pointer to the memory to free.
 *
 * @remarks
 * This macro exists since free'ing a pointer to `const` generates a warning.
 */
#define FREE(PTR)                 free( CONST_CAST( void*, (PTR) ) )

/**
 * A special-case of fatal_error() that additionally prints the file and line
 * where an internal error occurred.
 *
 * @param FORMAT The `printf()` format to use.
 * @param ... The `printf()` arguments.
 *
 * @sa fatal_error()
 * @sa #PERROR_EXIT_IF()
 * @sa perror_exit()
 */
#define INTERNAL_ERROR(FORMAT,...) \
  fatal_error( EX_SOFTWARE, "%s:%d: internal error: " FORMAT, __FILE__, __LINE__, __VA_ARGS__ )

#ifdef __GNUC__

/**
 * Checks (at compile-time) whether \a A is an array.
 *
 * @param A The alleged array to check.
 * @return Returns 1 (true) only if \a A is an array; 0 (false) otherwise.
 *
 * @sa #IS_POINTER()
 */
#define IS_ARRAY(A)               !IS_SAME_TYPE( (A), &(A)[0] )

/**
 * Checks (at compile-time) whether \a P is a pointer.
 *
 * @param P The alleged pointer to check.
 * @return Returns 1 (true) only if \a P is a pointer; 0 (false) otherwise.
 *
 * @sa #IS_ARRAY()
 */
#define IS_POINTER(P)             IS_SAME_TYPE( (P), &(P)[0] )

/**
 * Checks (at compile-time) whether \a T1 and \a T2 are the same type.
 *
 * @param T1 The first type or expression.
 * @param T2 The second type or expression.
 * @return Returns 1 (true) only if \a T1 and \a T2 are the same type; 0
 * (false) otherwise.
 */
#if defined(HAVE___BUILTIN_TYPES_COMPATIBLE_P) && defined(HAVE___TYPEOF__)
# define IS_SAME_TYPE(T1,T2) \
    __builtin_types_compatible_p( __typeof__(T1), __typeof__(T2) )
#else
# define IS_SAME_TYPE(T1,T2)      1
#endif

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

/**
 * Convenience macro for calling check_realloc().
 *
 * @param TYPE The type to allocate.
 * @param N The number of objects of \a TYPE to allocate.  It _must_ be &gt; 0.
 * @return Returns a pointer to \a N uninitialized objects of \a TYPE.
 *
 * @sa check_realloc()
 * @sa #REALLOC()
 */
#define MALLOC(TYPE,N)            check_realloc( NULL, sizeof(TYPE) * (N) )

/// @cond DOXYGEN_IGNORE
#define NAME2_HELPER(A,B)         A##B
/// @endcond

/**
 * Concatenate \a A and \a B together to form a single token.
 *
 * @remarks This macro is needed instead of simply using `##` when either
 * argument needs to be expanded first, e.g., `__LINE__`.
 *
 * @param A The first name.
 * @param B The second name.
 */
#define NAME2(A,B)                NAME2_HELPER(A,B)

/**
 * No-operation statement.  (Useful for a `goto` target.)
 */
#define NO_OP                     ((void)0)

/**
 * If \a EXPR is `true`, prints an error message for `errno` to standard error
 * and exits with status \a STATUS.
 *
 * @param EXPR The expression.
 * @param STATUS The exit status code.
 *
 * @sa fatal_error()
 * @sa #INTERNAL_ERROR()
 * @sa perror_exit()
 */
#define PERROR_EXIT_IF( EXPR, STATUS ) \
  BLOCK( if ( unlikely( EXPR ) ) perror_exit( STATUS ); )

/**
 * Calls **pipe**(2), checks for an error, and exits if there was one.
 *
 * @param FDS An array of two integer file descriptors to create.
 *
 * @sa #PERROR_EXIT_IF()
 */
#define PIPE(FDS)                 PERROR_EXIT_IF( pipe( FDS ) == -1, EX_OSERR )

/**
 * Cast either from or to a pointer type &mdash; similar to C++'s
 * `reinterpret_cast`, but for pointers only.
 *
 * @param T The type to cast to.
 * @param EXPR The expression to cast.
 *
 * @note This macro silences a "cast to pointer from integer of different size"
 * warning.  In C++, this would be done via `reinterpret_cast`, but it's not
 * possible to implement that in C that works for both pointers and integers.
 *
 * @sa #CONST_CAST()
 * @sa #STATIC_CAST()
 */
#define POINTER_CAST(T,EXPR)      ((T)(uintptr_t)(EXPR))

/**
 * Calls #FPRINTF() with `stdout`.
 *
 * @param ... The `fprintf()` arguments.
 *
 * @sa #EPRINTF()
 * @sa #FPRINTF()
 * @sa #PUTC()
 * @sa #PUTS()
 */
#define PRINTF(...)               FPRINTF( stdout, __VA_ARGS__ )

/**
 * Calls #FPUTC() with `stdout`.
 *
 * @param C The character to print.
 *
 * @sa #EPUTC()
 * @sa #FPUTC()
 * @sa #PRINTF()
 * @sa #PUTS()
 */
#define PUTC(C)                   FPUTC( (C), stdout )

/**
 * Calls #FPUTS() with `stdout`.
 *
 * @param S The C string to print.
 *
 * @note Unlike **puts**(3), does _not_ print a newline.
 *
 * @sa #FPUTS()
 * @sa #PRINTF()
 * @sa #PUTC()
 */
#define PUTS(S)                   FPUTS( (S), stdout )

/**
 * Convenience macro for calling check_realloc().
 *
 * @param PTR The pointer to memory to reallocate.  It is set to the newly
 * reallocated memory.
 * @param TYPE The type of object to reallocate.
 * @param N The number of objects of \a TYPE to reallocate.
 *
 * @sa check_realloc()
 * @sa #MALLOC()
 */
#define REALLOC(PTR,TYPE,N) \
  (PTR) = check_realloc( (PTR), sizeof(TYPE) * (size_t)(N) )

/**
 * Advances \a S over all \a CHARS.
 *
 * @param S The string pointer to advance.
 * @param CHARS A string containing the characters to skip over.
 * @return Returns the updated \a S.
 */
#define SKIP_CHARS(S,CHARS)       ((S) += strspn( (S), (CHARS) ))

/**
 * Like C11's `_Static_assert()` except that is can be used in an expression.
 *
 * @param EXPR The expression to check.
 * @param MSG The string literal of the error message to print only if \a EXPR
 * evaluates to 0 (false).
 * @return Always returns 1.
 */
#define STATIC_ASSERT_EXPR(EXPR,MSG) \
  (!!sizeof( struct { static_assert( (EXPR), MSG ); int required; } ))

/**
 * C version of C++'s `static_cast`.
 *
 * @param T The type to cast to.
 * @param EXPR The expression to cast.
 *
 * @note This macro can't actually implement C++'s `static_cast` because
 * there's no way to do it in C.  It serves merely as a visual cue for the type
 * of cast meant.
 *
 * @sa #CONST_CAST()
 * @sa #POINTER_CAST()
 */
#define STATIC_CAST(T,EXPR)       ((T)(EXPR))

/**
 * Shorthand for calling **strerror**(3) with `errno`.
 */
#define STRERROR()                strerror( errno )

/**
 * Macro that "string-ifies" its argument, e.g., <code>%STRINGIFY(x)</code>
 * becomes `"x"`.
 *
 * @param X The unquoted string to stringify.
 *
 * @note This macro is sometimes necessary in cases where it's mixed with uses
 * of `##` by forcing re-scanning for token substitution.
 *
 * @sa #CHARIFY()
 */
#define STRINGIFY(X)              STRINGIFY_IMPL(X)

/**
 * Synthesises a name prefixed by \a PREFIX unique to the line on which it's
 * used.
 *
 * @param PREFIX The prefix of the synthesized name.
 *
 * @warning All uses for a given \a PREFIX that refer to the same name _must_
 * be on the same line.  This is not a problem within macro definitions, but
 * won't work outside of them since there's no way to refer to a previously
 * used unique name.
 */
#define UNIQUE_NAME(PREFIX)       NAME2(NAME2(PREFIX,_),__LINE__)

/**
 * Whitespace string: space and tab only.
 */
#define WS_ST                     " \t"

/**
 * Whitespace string: space, tab, and carriage return only.
 */
#define WS_STR                    WS_ST "\r"

/**
 * Whitespace string: space, tab, carriage return, and newline only.
 */
#define WS_STRN                   WS_STR "\n"

// extern variable definitions
extern char const  *me;                 ///< Program name.

////////// extern functions ///////////////////////////////////////////////////

/**
 * Extracts the base portion of a path-name.
 * Unlike **basename**(3):
 *  + Trailing `/` characters are not deleted.
 *  + \a path_name is never modified (hence can therefore be `const`).
 *  + Returns a pointer within \a path_name (hence is multi-call safe).
 *
 * @param path_name The path-name to extract the base portion of.
 * @return Returns a pointer to the last component of \a path_name.
 * If \a path_name consists entirely of '/' characters,
 * a pointer to the string "/" is returned.
 */
NODISCARD
char const* base_name( char const *path_name );

/**
 * Comparison function for **bsearch**(3) that compares a string key against an
 * element of array of constant pointer to constant char.
 *
 * @param key A pointer to the string being searched for.
 * @param str_ptr A pointer to the pointer to the string of the current element
 * to compare against.
 * @return Returns an integer less than zero, zero, or greater thatn zero if
 * the key is less than, equal to, or greater than the element, respectively.
 */
NODISCARD
int bsearch_str_strptr_cmp( void const *key, void const *str_ptr );

/**
 * Calls **atexit**(3) and checks for failure.
 *
 * @param cleanup_fn The pointer to the function to call **atexit**(3) with.
 */
void check_atexit( void (*cleanup_fn)(void) );

/**
 * Converts an ASCII string to an unsigned integer.
 * Unlike **atoi**(3), insists that all characters in \a s are digits.
 * If conversion fails, prints an error message and exits.
 *
 * @param s The string to convert.
 * @return Returns the unsigned integer.
 */
NODISCARD
unsigned check_atou( char const *s );

/**
 * Calls **dup2**(2) and checks for failure.
 *
 * @param old_fd The old file descriptor to duplicate.
 * @param new_fd The new file descriptor to duplicate to.
 */
void check_dup2( int old_fd, int new_fd );

/**
 * Calls **realloc**(3) and checks for failure.
 * If reallocation fails, prints an error message and exits.
 *
 * @param p The pointer to reallocate.  If NULL, new memory is allocated.
 * @param size The number of bytes to allocate.
 * @return Returns a pointer to the allocated memory.
 */
NODISCARD
void* check_realloc( void *p, size_t size );

/**
 * Calls **strdup**(3) and checks for failure.
 * If memory allocation fails, prints an error message and exits.
 *
 * @param s The null-terminated string to duplicate.
 * @return Returns a copy of \a s.
 */
NODISCARD
char* check_strdup( char const *s );

/**
 * Chops off the end-of-line character(s), if any.
 *
 * @param s The null-terminated string to chop.
 * @param s_len The length of \a s.
 * @return Returns the new length of \a s.
 */
NODISCARD
size_t chop_eol( char *s, size_t s_len );

/**
 * Given an "opening" character, gets it matching "closing" chcaracter.
 *
 * @param c The "opening" character.
 * @return Returns said "closing" character or the null byte if none.
 */
NODISCARD
char closing_char( char c );

/**
 * Prints an error message to standard error and exits with \a status code.
 *
 * @param status The status code to exit with.
 * @param format The `printf()` format string literal to use.
 * @param ... The `printf()` arguments.
 *
 * @sa #INTERNAL_ERROR()
 * @sa perror_exit()
 * @sa #PERROR_EXIT_IF()
 */
PJL_PRINTF_LIKE_FUNC(2)
_Noreturn void fatal_error( int status, char const *format, ... );

/**
 * Copies \a ffrom to \a fto until EOF.
 *
 * @param ffrom The FILE to copy from.
 * @param fto The FILE to copy to.
 */
void fcopy( FILE *ffrom, FILE *fto );

/**
 * Gets a newline-terminated line from \a ffrom reading at most one fewer
 * characters than that given by \a size.
 *
 * @param buf A pointer to the buffer to receive the line.
 * @param size A pointer to the capacity of \a buf.  On return, it is set to
 * the number of characters read.
 * @param ffrom The FILE to read from.
 * @return Returns \a buf if any characters have been read or NULL on either
 * EOF or error.
 */
NODISCARD
char* fgetsz( char *buf, size_t *size, FILE *ffrom );

/**
 * Adds a pointer to the head of the free-later-list.
 *
 * @param p The pointer to add.
 * @return Returns \a p.
 */
PJL_DISCARD
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
NODISCARD
unsigned get_term_columns( void );
#endif /* WITH_WIDTH_TERM */

#ifndef NDEBUG
/**
 * Checks whether \a s is an affirmative value.  An affirmative value is one of
 * 1, t, true, y, or yes, case-insensitive.
 *
 * @param s The null-terminated string to check or null.
 * @return Returns `true` only if \a s is affirmative.
 */
NODISCARD
bool is_affirmative( char const *s );
#endif /* NDEBUG */

/**
 * Checks whether \a s is any one of \a matches, case-insensitive.
 *
 * @param s The null-terminated string to check or null.
 * @param matches The null-terminated array of values to check against.
 * @return Returns `true` only if \a s is among \a matches.
 */
NODISCARD
bool is_any( char const *s, char const *const matches[const] );

/**
 * Checks whether \a s is a blank line, that is a line consisting only of
 * whitespace.
 *
 * @param s The null-terminated string to check.
 * @return Returns `true` only if \a s is a blank line.
 */
NODISCARD W_UTIL_INLINE
bool is_blank_line( char const *s ) {
  SKIP_CHARS( s, WS_STRN );
  return *s == '\0';
}

/**
 * Checks whether \a s only contains decimal digit characters.
 *
 * @param s The null-terminated string to check.
 * @return Returns `true` only if \a s contains only digits.
 */
NODISCARD W_UTIL_INLINE
bool is_digits( char const *s ) {
  return s[ strspn( s, "0123456789" ) ] == '\0';
}

/**
 * Checks whether \a c is an end-of-line character.
 *
 * @param c The character to check.
 * @return Returns `true` only if it is.
 */
NODISCARD W_UTIL_INLINE
bool is_eol( char c ) {
  return c == '\n' || c == '\r';
}

/**
 * Checks whether \a c is either a space or a tab only since `\r` or `\n` mean
 * the end of a line.
 *
 * @param c The character to check.
 * @return Returns `true` only if \a c is a space or a tab.
 */
NODISCARD W_UTIL_INLINE
bool is_space( char c ) {
  return c == ' ' || c == '\t';
}

/**
 * Gets whether the line in \a buf is a Windows line, i.e., ends with
 * `{CR}{LF}`.
 *
 * @param buf The buffer to check.
 * @param buf_len The length of \a buf.
 * @return Returns `true` only if \a buf ends with `{CR}{LF}`.
 */
NODISCARD W_UTIL_INLINE
bool is_windows_eol( char const buf[const], size_t buf_len ) {
  return buf_len >= 2 && buf[ buf_len - 2 ] == '\r';
}

/**
 * Prints an error message for `errno` to standard error and exits.
 *
 * @param status The exit status code.
 */
_Noreturn void perror_exit( int status );

/**
 * Sets the locale for the `LC_COLLATE` and `LC_CTYPE` categories to UTF-8.
 */
void setlocale_utf8( void );

/**
 * Splits off the trailing whitespace (tws) from \a buf into \a tws.  For
 * example, if \a buf is initially <code>"# "</code>, it will become `#` and
 * \a tws will become <code>" "</code>.
 *
 * @param buf The null-terminated buffer to split off the trailing whitespace
 * from.
 * @param buf_len The length of \a buf.
 * @param tws The buffer to receive the trailing whitespace from \a buf, if
 * any.
 */
void split_tws( char buf[const], size_t buf_len, char tws[const] );

/**
 * A variant of **strcpy**(3) that returns the number of characters copied.
 *
 * @param dst A pointer to receive the copy of \a src.
 * @param src The null-terminated string to copy.
 * @return Returns the number of characters copied.
 */
NODISCARD
size_t strcpy_len( char *dst, char const *src );

/**
 * Reverse **strspn**(3): spans the trailing part of \a s as long as characters
 * from \a s occur in \a set.
 *
 * @param s The null-terminated string to span.
 * @param set The null-terminated set of characters.
 * @return Returns the number of characters spanned.
 */
NODISCARD
size_t strrspn( char const *s, char const *set );

/**
 * Checks the flag: if `true`, resets the flag to `false`.
 *
 * @param flag A pointer to the Boolean flag to be tested and, if `true`, set
 * to \c false.
 * @return Returns `true` only if \a *flag is `true`.
 */
NODISCARD W_UTIL_INLINE
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
