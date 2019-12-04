/*
**      wrap -- text reformatter
**      src/pattern.c
**
**      Copyright (C) 2013-2019  Paul J. Lucas
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

// local
#include "wrap.h"                       /* must go first */
#include "pattern.h"
#include "common.h"
#include "util.h"

// standard
#include <assert.h>
#include <fnmatch.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

// local constant definitions
static size_t const PATTERN_ALLOC_DEFAULT   = 10;
static size_t const PATTERN_ALLOC_INCREMENT = 10;

// local variable definitions
static size_t       n_patterns = 0;     // number of patterns
static pattern_t   *patterns = NULL;    // global list of patterns

////////// local functions ////////////////////////////////////////////////////

/**
 * Allocates a new pattern.
 *
 * @return Returns a new, uninitialzed pattern.
 */
static pattern_t* pattern_alloc( void ) {
  static size_t n_patterns_alloc = 0;   // number of patterns allocated

  if ( n_patterns_alloc == 0 ) {
    n_patterns_alloc = PATTERN_ALLOC_DEFAULT;
    patterns = MALLOC( pattern_t, n_patterns_alloc );
  } else if ( n_patterns > n_patterns_alloc ) {
    n_patterns_alloc += PATTERN_ALLOC_INCREMENT;
    REALLOC( patterns, pattern_t, n_patterns_alloc );
  }
  IF_EXIT( patterns == NULL, EX_OSERR );
  return &patterns[ n_patterns++ ];
}

/**
 * Frees all memory used by a pattern.
 *
 * @param pattern The pattern to free.
 */
static void pattern_free( pattern_t *pattern ) {
  assert( pattern != NULL );
  FREE( pattern->pattern );
}

////////// extern functions ///////////////////////////////////////////////////

#ifndef NDEBUG
void dump_patterns( void ) {
  for ( size_t i = 0; i < n_patterns; ++i ) {
    if ( i == 0 )
      printf( "[PATTERNS]\n" );
    pattern_t const *const pattern = &patterns[i];
    printf( "%s = %s\n", pattern->pattern, pattern->alias->argv[0] );
  } // for
}
#endif /* NDEBUG */

void pattern_cleanup( void ) {
  while ( n_patterns > 0 )
    pattern_free( &patterns[ --n_patterns ] );
  FREE( patterns );
}

alias_t const* pattern_find( char const *file_name ) {
  assert( file_name != NULL );
  for ( size_t i = 0; i < n_patterns; ++i )
    if ( fnmatch( patterns[i].pattern, file_name, 0 ) == 0 )
      return patterns[i].alias;
  return NULL;
}

void pattern_parse( char const *line, char const *conf_file,
                    unsigned line_no ) {
  assert( line != NULL );
  assert( conf_file != NULL );
  assert( line_no > 0 );

  pattern_t *const pattern = pattern_alloc();

  // pattern line: <pattern>[<ws>]=[<ws>]<alias>
  //        parts:     1      2   3  4      5   

  // part 1: pattern
  size_t const span = strcspn( line, " \t=" );
  pattern->pattern = strndup( line, span );
  line += span;

  // part 2: whitespace
  SKIP_CHARS( line, WS_STR );
  if ( *line == '\0' )
    PMESSAGE_EXIT( EX_CONFIG, "%s:%u: '=' expected\n", conf_file, line_no );

  // part 3: =
  if ( *line != '=' )
    PMESSAGE_EXIT( EX_CONFIG,
      "%s:%u: '%c': unexpected character; '=' expected\n",
      conf_file, line_no, *line
    );
  ++line;                               // skip '='

  // part 4: whitespace
  SKIP_CHARS( line, WS_STR );
  if ( *line == '\0' )
    PMESSAGE_EXIT( EX_CONFIG,
      "%s:%u: alias name expected after '='\n",
      conf_file, line_no
    );

  // part 5: alias
  alias_t const *const alias = alias_find( line );
  if ( alias == NULL )
    PMESSAGE_EXIT( EX_CONFIG,
      "%s:%u: \"%s\": no such alias\n", conf_file, line_no, line
    );
  pattern->alias = alias;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
