/*
**      wrap -- text reformatter
**      common.h
**
**      Copyright (C) 1996-2013  Paul J. Lucas
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
**      along with this program; if not, write to the Free Software
**      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef wrap_common_H
#define wrap_common_H

/*****************************************************************************/

#define DEFAULT_LINE_LENGTH 80          /* wrap text to this line length */
#define DEFAULT_TAB_SPACES  8           /* number of spaces a tab equals */
#define LINE_BUF_SIZE       1024        /* hopefully, no one will exceed this */
#define WRAP_NAME           "wrap"      /* for exec(2) in wrapc */
#define WRAP_VERSION        "3.0"

/* exit(3) status codes */
#define EXIT_OK             0
#define EXIT_USAGE          1
#define EXIT_CONF_ERROR     2
#define EXIT_OUT_OF_MEMORY  3
#define EXIT_READ_OPEN      10
#define EXIT_READ_ERROR     11
#define EXIT_WRITE_OPEN     12
#define EXIT_WRITE_ERROR    13
#define EXIT_FORK_ERROR     20
#define EXIT_EXEC_ERROR     21
#define EXIT_CHILD_SIGNAL   30
#define EXIT_PIPE_ERROR     40

#ifndef __cplusplus
# ifdef bool
#   undef bool
# endif
# ifdef true
#   undef true
# endif
# ifdef false
#   undef false
# endif
# define bool   int
# define true   1
# define false  0
#endif /* __cplusplus */

#define ERROR(E)            do { perror( me ); exit( E ); } while (0)

#define MALLOC(TYPE,N) \
  (TYPE*)check_realloc( NULL, sizeof(TYPE) * (N) )

#define REALLOC(PTR,TYPE,N) \
  (PTR) = (TYPE*)check_realloc( (PTR), sizeof(TYPE) * (N) )

/**
 * Extracts the base portion of a path-name.
 *
 * @param path_name The path-name to extract the base portion of.
 * @return Returns a pointer to the last component of \a path_name.
 * If \a path_name consists entirely of '/' characters,
 * a pointer to the string "/" is returned.
 */
char const* base_name( char const *path_name );

/**
 * Converts an ASCII string to an unsigned integer.
 * Unlike atoi(3), this function insists that all characters in \a s
 * are digits.
 * If conversion fails, prints an error message and exits.
 *
 * @param s The string to convert.
 * @return Returns the unsigned integer.
 */
int check_atou( char const *s );

/**
 * Calls realloc(3) and checks for failure.
 * If reallocation fails, prints an error message and exits.
 *
 * @param p The pointer to reallocate.  If NULL, new memory is allocated.
 * @param size The number of bytes to allocate.
 * @return Returns a pointer to the allocated memory.
 */
void* check_realloc( void *p, size_t size );

/*****************************************************************************/

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
