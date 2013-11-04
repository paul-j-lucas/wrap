/*
**      wrap -- text reformatter
**      common.c
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
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* system */
#include <stdio.h>
#include <stdlib.h>                     /* for atoi(), malloc(), ... */
#include <string.h>

/* local */
#include "common.h"

extern char const *me;

/*****************************************************************************/

char const* base_name( char const *path_name ) {
  char const *const slash = strrchr( path_name, '/' );
  if ( slash )
    return slash[1] ? slash + 1 : slash;
  return path_name;
}

int check_atou( char const *s ) {
  if ( strspn( s, "0123456789" ) != strlen( s ) ) {
    fprintf( stderr, "%s: \"%s\": invalid integer\n", me, s );
    exit( EXIT_USAGE );
  }
  return atoi( s );
}

void* check_realloc( void *p, size_t size ) {
  void *r;
  /*
  ** Autoconf, 5.5.1:
  **
  ** realloc
  **    The C standard says a call realloc(NULL, size) is equivalent to
  **    malloc(size), but some old systems don't support this (e.g., NextStep).
  */
  if ( !size )
    ++size;
  r = p ? realloc( p, size ) : malloc( size );
  if ( !r )
    ERROR( EXIT_OUT_OF_MEMORY );
  return r;
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
