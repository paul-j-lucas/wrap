/*
**      wrap -- text reformatter
**      util.c
**
**      Copyright (C) 2013-2015  Paul J. Lucas
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

// local
#include "common.h"
#include "util.h"

// standard
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>                     /* for atoi(), malloc(), ... */
#include <string.h>

extern char const *me;

////////// extern functions ///////////////////////////////////////////////////

char const* base_name( char const *path_name ) {
  assert( path_name );
  char const *const slash = strrchr( path_name, '/' );
  if ( slash )
    return slash[1] ? slash + 1 : slash;
  return path_name;
}

int check_atou( char const *s ) {
  assert( s );
  if ( s[ strspn( s, "0123456789" ) ] )
    PMESSAGE_EXIT( EX_USAGE, "\"%s\": invalid integer\n", s );
  return (int)strtol( s, NULL, 10 );
}

void* check_realloc( void *p, size_t size ) {
  //
  // Autoconf, 5.5.1:
  //
  // realloc
  //    The C standard says a call realloc(NULL, size) is equivalent to
  //    malloc(size), but some old systems don't support this (e.g., NextStep).
  //
  if ( !size )
    size = 1;
  void *const r = p ? realloc( p, size ) : malloc( size );
  if ( !r )
    PERROR_EXIT( EX_OSERR );
  return r;
}

void fcopy( FILE *ffrom, FILE *fto ) {
  char buf[ LINE_BUF_SIZE ];
  for ( size_t size; (size = fread( buf, 1, sizeof( buf ), ffrom )) > 0; )
    FWRITE( buf, 1, size, fto );
  CHECK_FERROR( ffrom );
}

int peekc( FILE *file ) {
  int const c = getc( file );
  if ( c == EOF )
    CHECK_FERROR( file );
  else
    UNGETC( c, file );
  return c;
}

size_t strrspn( char const *s, char const *set ) {
  assert( s );
  assert( set );

  size_t n = 0;
  for ( char const *t = s + strlen( s ); t-- > s && strchr( set, *t ); ++n )
    ;
  return n;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
