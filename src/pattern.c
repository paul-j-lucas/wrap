/*
**      wrap -- text reformatter
**      pattern.c
**
**      Copyright (C) 2013-2014  Paul J. Lucas
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
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* local */
#include "common.h"
#include "pattern.h"

#define PATTERN_ALLOC_DEFAULT       10
#define PATTERN_ALLOC_INCREMENT     10

extern char const*  me;
static int          n_patterns = 0;     /* number of patterns */
static pattern_t*   patterns = NULL;    /* global list of patterns */

/*****************************************************************************/

/**
 * Frees all memory used by a pattern.
 *
 * @paran pattern The pattern to free.
 */
static void pattern_free( pattern_t *pattern ) {
  free( (void*)pattern->pattern );
}

/*****************************************************************************/

void pattern_cleanup() {
  while ( n_patterns )
    pattern_free( &patterns[ --n_patterns ] );
  free( patterns );
}

alias_t const* pattern_find( char const *file_name ) {
  int i;
  for ( i = 0; i < n_patterns; ++i )
    if ( fnmatch( patterns[i].pattern, file_name, 0 ) == 0 )
      return patterns[i].alias;
  return NULL;
}

void pattern_parse( char const *line, char const *conf_file, int line_no ) {
  alias_t const *alias;
  static int n_patterns_alloc = 0;      /* number of patterns allocated */
  pattern_t *pattern;                   /* current pattern being parsed into */
  size_t span;

  if ( !n_patterns_alloc ) {
    n_patterns_alloc = PATTERN_ALLOC_DEFAULT;
    patterns = MALLOC( pattern_t, n_patterns_alloc );
  } else if ( n_patterns > n_patterns_alloc ) {
    n_patterns_alloc += PATTERN_ALLOC_INCREMENT;
    REALLOC( patterns, pattern_t, n_patterns_alloc );
  }
  if ( !patterns )
    PERROR_EXIT( OUT_OF_MEMORY );

  pattern = &patterns[ n_patterns++ ];

  /* pattern line: <pattern>[<ws>]=[<ws>]<alias> */
  /*        parts:     1      2   3  4      5    */

  /* part 1: pattern */
  span = strcspn( line, " \t=" );
  pattern->pattern = strndup( line, span );
  line += span;

  /* part 2: whitespace */
  line += strspn( line, " \t" );
  if ( !*line )
    PMESSAGE_EXIT( CONF_ERROR, "%s:%d: '=' expected\n", conf_file, line_no );

  /* part 3: = */
  if ( *line != '=' )
    PMESSAGE_EXIT( CONF_ERROR,
      "%s:%d: '%c': unexpected character; '=' expected\n",
      conf_file, line_no, *line
    );
  ++line;                               /* skip '=' */

  /* part 4: whitespace */
  line += strspn( line, " \t" );
  if ( !*line )
    PMESSAGE_EXIT( CONF_ERROR,
      "%s:%d: alias name expected after '='\n",
      conf_file, line_no
    );

  /* part 5: alias */
  if ( !(alias = alias_find( line )) )
    PMESSAGE_EXIT( CONF_ERROR,
      "%s:%d: \"%s\": no such alias\n", conf_file, line_no, line
    );
  pattern->alias = alias;
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
