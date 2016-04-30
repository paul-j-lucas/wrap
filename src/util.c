/*
**      wrap -- text reformatter
**      util.c
**
**      Copyright (C) 2013-2016  Paul J. Lucas
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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

/**
 * A node for a singly linked list of pointers to memory to be freed via
 * \c atexit().
 */
struct free_node {
  void *ptr;
  struct free_node *next;
};
typedef struct free_node free_node_t;

// local variable definitions
static free_node_t *free_head;          // linked list of stuff to free

// inline functions
extern bool         is_blank_line( char const* );
extern bool         is_eol( char );

////////// extern functions ///////////////////////////////////////////////////

char const* base_name( char const *path_name ) {
  assert( path_name );
  char const *const slash = strrchr( path_name, '/' );
  if ( slash )
    return slash[1] ? slash + 1 : slash;
  return path_name;
}

unsigned check_atou( char const *s ) {
  assert( s );
  if ( s[ strspn( s, "0123456789" ) ] )
    PMESSAGE_EXIT( EX_USAGE, "\"%s\": invalid integer\n", s );
  return (unsigned)strtoul( s, NULL, 10 );
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

char* check_strdup( char const *s ) {
  assert( s );
  char *const dup = strdup( s );
  if ( !dup )
    PERROR_EXIT( EX_OSERR );
  return dup;
}

void fcopy( FILE *ffrom, FILE *fto ) {
  char buf[ LINE_BUF_SIZE ];
  for ( size_t size; (size = fread( buf, 1, sizeof buf, ffrom )) > 0; )
    FWRITE( buf, 1, size, fto );
  FERROR( ffrom );
}

char* fgetsz( char *buf, size_t *size, FILE *ffrom ) {
  assert( buf );
  assert( size );
  //
  // Based on the implementation given in:
  //
  //  + Brian W. Kernighan and Dennis M. Ritchie: "The C Programming Language,"
  //    2nd ed., section 7.7 "Line Input and Output," Prentice Hall, 1988, p.
  //    134.
  //
  int c;
  char *s = buf;

  for ( size_t n = *size; n > 0 && (c = getc( ffrom )) != EOF; --n ) {
    if ( (*s++ = c) == '\n' )
        break;
  } // for

  *s = '\0';
  *size = s - buf;

  return c == EOF && !*size ? NULL : buf;
}

void* free_later( void *p ) {
  assert( p );
  free_node_t *const new_node = MALLOC( free_node_t, 1 );
  new_node->ptr = p;
  new_node->next = free_head;
  free_head = new_node;
  return p;
}

void free_now() {
  for ( free_node_t *p = free_head; p; ) {
    free_node_t *const next = p->next;
    free( p->ptr );
    free( p );
    p = next;
  } // for
  free_head = NULL;
}

size_t strrspn( char const *s, char const *set ) {
  assert( s );
  assert( set );

  size_t n = 0;
  for ( char const *t = s + strlen( s ); t-- > s && strchr( set, *t ); ++n )
    /* empty */;
  return n;
}

char* tolower_s( char *s ) {
  assert( s );
  for ( char *t = s; *t; ++t )
    *t = tolower( *t );
  return s;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
