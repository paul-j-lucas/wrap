/*
**      wrap -- text reformatter
**      src/alias.c
**
**      Copyright (C) 2013-2026  Paul J. Lucas
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

/**
 * @file
 * Defines functions to manipulate an \ref alias in configuration files.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "alias.h"
#include "common.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @endcond

/**
 * @addtogroup alias-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

// local constant definitions

/// Number of aliases to allocate by default.
static size_t const ALIAS_ALLOC_DEFAULT         = 10;

/// Number of aliases to increment by.
static size_t const ALIAS_ALLOC_INCREMENT       = 10;

/// Number of alias arguments to allocate by default.
static size_t const ALIAS_ARGV_ALLOC_DEFAULT    = 10;

/// Number of alias arguments to increment by.
static size_t const ALIAS_ARGV_ALLOC_INCREMENT  = 10;

/// Characters allowable in alias names.
static char const   ALIAS_NAME_CHARS[]          = "abcdefghijklmnopqrstuvwxyz"
                                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                  "0123456789+-.:_";

// local variable definitions
static alias_t     *aliases = NULL;     ///< Global list of aliases.
static size_t       n_aliases = 0;      ///< Number of aliases in global list.

// local functions
static void   alias_cleanup( void );
static void   alias_free( alias_t* );

NODISCARD
static size_t strcpy_set( char*, size_t, char const*, char const* );

////////// local functions ////////////////////////////////////////////////////

/**
 * Allocates a new alias.
 *
 * @return Returns a new, uninitialzed alias.
 */
NODISCARD
static alias_t* alias_alloc( void ) {
  RUN_ONCE ATEXIT( &alias_cleanup );

  static size_t n_aliases_alloc = 0;    // number of aliases allocated

  if ( n_aliases_alloc == 0 ) {
    n_aliases_alloc = ALIAS_ALLOC_DEFAULT;
    aliases = MALLOC( alias_t, n_aliases_alloc );
  } else if ( n_aliases > n_aliases_alloc ) {
    n_aliases_alloc += ALIAS_ALLOC_INCREMENT;
    REALLOC( aliases, alias_t, n_aliases_alloc );
  }
  PERROR_EXIT_IF( aliases == NULL, EX_OSERR );
  return &aliases[ n_aliases++ ];
}

/**
 * Checks the most-recently-added alias against all previous aliases for a
 * duplicate name.  If a duplicate is found, prints an error message and exits.
 *
 * @param conf_file The configuration file path-name.
 * @param line_no The line-number within \a conf_file.
 */
static void alias_check_dup( char const *conf_file, unsigned line_no ) {
  assert( conf_file != NULL );
  if ( n_aliases > 1 ) {
    //
    // The number of aliases is assumed to be small, so linear search is good
    // enough.
    //
    int i = STATIC_CAST( int, n_aliases ) - 1;
    char const *const last_name = aliases[i].argv[0];
    while ( --i >= 0 ) {
      if ( strcmp( aliases[i].argv[0], last_name ) == 0 ) {
        fatal_error( EX_CONFIG,
          "%s:%u: \"%s\": duplicate alias name (first is on line %u)\n",
          conf_file, line_no, last_name, aliases[i].line_no
        );
      }
    } // while
  }
}

/**
 * Cleans-up all alias data.
 */
void alias_cleanup( void ) {
  while ( n_aliases > 0 )
    alias_free( &aliases[ --n_aliases ] );
  free( aliases );
}

/**
 * Frees all memory used by an alias.
 *
 * @param alias The alias to free.
 */
static void alias_free( alias_t *alias ) {
  assert( alias != NULL );
  while ( alias->argc > 0 )
    FREE( alias->argv[ --alias->argc ] );
  FREE( alias->argv );
}

/**
 * Imports the arguments from another alias into this one.
 *
 * @param to_alias A pointer to the alias to import to.
 * @param ps A pointer to the string to parse for the alias name to import.  It
 * is left one past the alias name.
 * @param conf_file The configuration file path-name.
 * @param line_no The line-number within \a conf_file.
 */
static void alias_import( alias_t *to_alias, char const **ps,
                          char const *conf_file, unsigned line_no ) {
  assert( to_alias != NULL );
  assert( ps != NULL );
  assert( **ps == '@' );
  assert( conf_file != NULL );

  char from_name[ 40 ];

  ++*ps;                                // skip past '@'
  *ps += strcpy_set( from_name, sizeof from_name, ALIAS_NAME_CHARS, *ps );
  alias_t const *const from_alias = alias_find( from_name );
  if ( from_alias == NULL ) {
    fatal_error( EX_CONFIG,
      "%s:%u: \"@%s\": no such alias\n",
      conf_file, line_no, from_name
    );
  }
  for ( int i = 1; i < from_alias->argc; ++i )
    to_alias->argv[ to_alias->argc++ ] = check_strdup( from_alias->argv[ i ] );
}

/**
 * Duplicates the next command-line argument stripping quotes and backslashes
 * similar to what a shell would do.
 *
 * @param ps A pointer to the argument; updated to just past the argument.
 * @return Returns a pointer to the next argument.
 */
static char* arg_dup( char const **ps ) {
  assert( ps != NULL );

  char const *s = *ps;
  char *const arg_buf = MALLOC( char, strlen( s ) + 1/*\0*/ );
  char *arg = arg_buf;
  char quote = '\0';

  for ( ; *s != '\0'; ++s ) {
    switch ( *s ) {
      case ' ':
      case '\t':
        if ( quote == '\0' )
          goto done;
        break;
      case '"':
      case '\'':
        if ( quote != '\0' ) {
          if ( *s == quote ) {
            quote = '\0';
            continue;
          }
        } else {
          quote = *s;
          continue;
        }
        break;
      case '\\':
        ++s;
        break;
    } // switch
    *arg++ = *s;
  } // for

done:
  *arg = '\0';
  *ps = s;
  return arg_buf;
}

/**
 * Performs a **strcpy**(3), but only while characters in \a src are in \a set.
 *
 * @param dest A pointer to the buffer to copy to.
 * @param dest_size The size of \a dest.
 * @param set The set of characters allowed to be copied.
 * @param src The null-terminated string to be copied.
 * @return Returns the number of characters copied.
 */
NODISCARD
static size_t strcpy_set( char *dest, size_t dest_size, char const *set,
                          char const *src ) {
  assert( dest != NULL );
  assert( set != NULL );
  assert( src != NULL );

  char const *const dest_end = dest + dest_size - 1/*null*/;
  size_t n_copied = 0;

  while ( dest < dest_end && *src != '\0' && strchr( set, *src ) != NULL ) {
    *dest++ = *src++;
    ++n_copied;
  } // while

  *dest = '\0';
  return n_copied;
}

////////// extern functions ///////////////////////////////////////////////////

alias_t const* alias_find( char const *name ) {
  assert( name != NULL );
  for ( size_t i = 0; i < n_aliases; ++i )
    if ( strcmp( aliases[i].argv[0], name ) == 0 )
      return &aliases[i];
  return NULL;
}

void alias_parse( char const *line, char const *conf_file, unsigned line_no ) {
  assert( line != NULL );
  assert( conf_file != NULL );
  assert( line_no > 0 );

  size_t n_argv_alloc = ALIAS_ARGV_ALLOC_DEFAULT;
  alias_t *const alias = alias_alloc();
  alias->argc = 1;
  alias->argv = MALLOC( char const*, n_argv_alloc );
  alias->line_no = line_no;

  // alias line: <name>[<ws>]={[<ws>]<options>}...
  //      parts:   1     2   3   4       5        

  // part 1: name
  size_t const span = strspn( line, ALIAS_NAME_CHARS );
  alias->argv[0] = strndup( line, span );
  alias_check_dup( conf_file, line_no );
  line += span;

  // part 2: whitespace
  SKIP_CHARS( line, WS_STR );
  if ( *line == '\0' )
    fatal_error( EX_CONFIG, "%s:%u: '=' expected\n", conf_file, line_no );

  // part 3: =
  if ( *line != '=' ) {
    fatal_error( EX_CONFIG,
      "%s:%u: '%c': unexpected character; '=' expected\n",
      conf_file, line_no, *line
    );
  }
  ++line;                               // skip '='

  // parts 4 & 5: whitespace, options
  for (;;) {
    SKIP_CHARS( line, WS_STR );
    if ( *line == '\0' ) {
      if ( alias->argc == 1 ) {
        fatal_error( EX_CONFIG,
          "%s:%u: option(s) expected after '='\n",
          conf_file, line_no
        );
      }
      break;
    }

    if ( STATIC_CAST( size_t, alias->argc ) == n_argv_alloc ) {
      n_argv_alloc += ALIAS_ARGV_ALLOC_INCREMENT;
      REALLOC( alias->argv, char const*, n_argv_alloc );
    }

    if ( *line == '@' )
      alias_import( alias, &line, conf_file, line_no );
    else
      alias->argv[ alias->argc++ ] = arg_dup( &line );
  } // for

  if ( n_argv_alloc > STATIC_CAST( size_t, alias->argc ) + 1 )
    REALLOC( alias->argv, char const*, alias->argc + 1 );
  alias->argv[ alias->argc ] = NULL;
}

#ifndef NDEBUG
void dump_aliases( void ) {
  for ( size_t i = 0; i < n_aliases; ++i ) {
    if ( i == 0 )
      puts( "[ALIASES]" );
    alias_t const *const alias = &aliases[i];
    printf( "%s =", alias->argv[0] );
    for ( int arg = 1; arg < alias->argc; ++arg )
      printf( " %s", alias->argv[ arg ] );
    putchar( '\n' );
  } // for
}
#endif /* NDEBUG */

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
