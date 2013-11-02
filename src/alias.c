/*
**      wrap -- text reformatter
**      alias.c
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
#include <stdlib.h>
#include <string.h>

/* local */
#include "alias.h"
#include "common.h"

#define ALIAS_ALLOC_DEFAULT         10
#define ALIAS_ALLOC_INCREMENT       10
#define ALIAS_ARGV_ALLOC_DEFAULT    10
#define ALIAS_ARGV_ALLOC_INCREMENT  10
#define ALIAS_NAME_CHARS \
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"

extern char const*  me;

static alias_t*     aliases = NULL;
static int          n_aliases = 0;
static int          n_aliases_alloc = 0;

/*****************************************************************************/

static void alias_check_dup( char const *conf_file, int line_no ) {
  if ( n_aliases > 1 ) {
    int i = n_aliases - 1;
    alias_t const *const last = &aliases[i];
    while ( --i )
      if ( strcmp( aliases[i].argv[0], last->argv[0] ) == 0 ) {
        fprintf(
          stderr,
          "%s: %s:%d: \"%s\": duplicate alias name (previous one on line %d)\n",
          me, conf_file, line_no, last->argv[0], last->line_no
        );
        exit( EXIT_CONF_ERROR );
      }
  }
}

static void alias_free( alias_t *alias ) {
  while ( alias->argc >= 0 )
    free( (void*)alias->argv[ alias->argc-- ] );
  free( alias->argv );
}

/*****************************************************************************/

void alias_cleanup() {
  while ( n_aliases )
    alias_free( &aliases[ --n_aliases ] );
  free( aliases );
}

alias_t const* alias_find( char const *name ) {
  int i;
  for ( i = 0; i < n_aliases; ++i )
    if ( strcmp( aliases[i].argv[0], name ) == 0 )
      return &aliases[i];
  return NULL;
}

void alias_parse( char const *line, char const *conf_file, int line_no ) {
  alias_t *alias;
  int n_argv_alloc = ALIAS_ARGV_ALLOC_DEFAULT;
  size_t span;

  if ( !n_aliases_alloc ) {
    n_aliases_alloc = ALIAS_ALLOC_DEFAULT;
    aliases = MALLOC( alias_t, n_aliases_alloc );
  } else if ( n_aliases > n_aliases_alloc ) {
    n_aliases_alloc += ALIAS_ALLOC_INCREMENT;
    REALLOC( aliases, alias_t, n_aliases_alloc );
  }
  if ( !aliases )
    ERROR( EXIT_OUT_OF_MEMORY );

  alias = &aliases[ n_aliases++ ];
  alias->line_no = line_no;
  alias->argc = 1;
  alias->argv = MALLOC( char const*, n_argv_alloc );
  span = strspn( line, ALIAS_NAME_CHARS );
  alias->argv[0] = strndup( line, span );
  line += span;

  alias_check_dup( conf_file, line_no );

  line += strspn( line, " \t" );
  if ( !*line ) {
    fprintf(
      stderr, "%s: %s:%d: '=' expected\n",
      me, conf_file, line_no
    );
    exit( EXIT_CONF_ERROR );
  }

  if ( *line != '=' ) {
    fprintf(
      stderr, "%s: %s:%d: '%c': unexpected character; '=' expected\n",
      me, conf_file, line_no, *line
    );
    exit( EXIT_CONF_ERROR );
  }
  ++line;                               /* skip '=' */

  while ( true ) {
    line += strspn( line, " \t" );
    if ( !*line ) {
      if ( !alias->argc ) {
        fprintf(
          stderr, "%s: %s:%d: option(s) expected after '='\n",
          me, conf_file, line_no
        );
        exit( EXIT_CONF_ERROR );
      }
      break;
    }
    if ( alias->argc == n_argv_alloc ) {
      n_argv_alloc += ALIAS_ARGV_ALLOC_INCREMENT;
      REALLOC( alias->argv, char const*, n_argv_alloc );
    }
    span = strcspn( line, " \t" );
    alias->argv[ alias->argc++ ] = strndup( line, span );
    line += span;
  } /* while */

  if ( n_argv_alloc > alias->argc + 1 )
    REALLOC( alias->argv, char const*, alias->argc + 1 );
  alias->argv[ alias->argc ] = NULL;
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
