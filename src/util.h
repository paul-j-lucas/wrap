/*
**      wrap -- text reformatter
**      util.h
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

#ifndef wrap_util_H
#define wrap_util_H

// local
#include "config.h"

// standard
#include <stddef.h>                     /* for size_t */
#include <stdio.h>                      /* for FILE */

///////////////////////////////////////////////////////////////////////////////

// define a "bool" type
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#   ifdef __cplusplus
typedef bool _Bool;
#   else
#     define _Bool signed char
#   endif /* __cplusplus */
# endif /* HAVE__BOOL */
# define bool   _Bool
# define false  0
# define true   1
# define __bool_true_false_are_defined 1
#endif /* HAVE_STDBOOL_H */

#define BLOCK(...)          do { __VA_ARGS__ } while (0)
#define ERROR_STR           strerror( errno )
#define PERROR_EXIT(STATUS) BLOCK( perror( me ); exit( EXIT_##STATUS ); )
#define PRINT_ERR(...)      fprintf( stderr, __VA_ARGS__ )

#define PMESSAGE_EXIT(STATUS,FORMAT,...) \
  BLOCK( PRINT_ERR( "%s: " FORMAT, me, __VA_ARGS__ ); exit( EXIT_##STATUS ); )

#define CHECK_FERROR(STREAM) \
  BLOCK( if ( ferror( STREAM ) ) PERROR_EXIT( READ_ERROR ); )

#define FPRINTF(STREAM,...) \
  BLOCK( if ( fprintf( (STREAM), __VA_ARGS__ ) < 0 ) PERROR_EXIT( WRITE_ERROR ); )

#define FPUTC(C,STREAM) \
  BLOCK( if ( putc( (C), (STREAM) ) == EOF ) PERROR_EXIT( WRITE_ERROR ); )

#define FPUTS(S,STREAM) \
  BLOCK( if ( fputs( (S), (STREAM) ) == EOF ) PERROR_EXIT( WRITE_ERROR ); )

#define FWRITE(BUF,SIZE,NITEMS,STREAM) \
  BLOCK( if ( fwrite( (BUF), (SIZE), (NITEMS), (STREAM) ) < (NITEMS) ) PERROR_EXIT( WRITE_ERROR ); )

#define MALLOC(TYPE,N) \
  (TYPE*)check_realloc( NULL, sizeof(TYPE) * (N) )

#define PIPE(P) \
  BLOCK( if ( pipe( P ) == -1 ) PERROR_EXIT( PIPE_ERROR ); )

#define REALLOC(PTR,TYPE,N) \
  (PTR) = (TYPE*)check_realloc( (PTR), sizeof(TYPE) * (N) )

#define UNGETC(C,STREAM) \
  BLOCK( if ( ungetc( (C), (STREAM) ) == EOF ) PERROR_EXIT( READ_ERROR ); )

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
int check_atou( char const *s );

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
 * Copies \a ffrom to \a fto until EOF.
 *
 * @param ffrom The \c FILE to copy from.
 * @param fto The \c FILE to copy to.
 */
void fcopy( FILE *ffrom, FILE *fto );

/**
 * Peeks at the next character on the given file stream, but does not advance
 * the \c FILE pointer.
 *
 * @param file The file to peek from.
 * @return Returns the next character, if any, or \c EOF if none.
 */
int peekc( FILE *file );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_util_H */
/* vim:set et sw=2 ts=2: */
