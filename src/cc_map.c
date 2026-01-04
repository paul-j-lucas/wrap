/*
**      wrapc -- comment reformatter
**      src/cc_map.c
**
**      Copyright (C) 2017-2026  Paul J. Lucas
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
 * Defines functions to manipulate a comment delimiter character map.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "cc_map.h"
#include "options.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <string.h>                     /* for str...() */

/// @endcond

/**
 * @addtogroup @cc-map-group
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Comment delimiter character set indexed by character.
 */
typedef bool cc_set_t[128];

// extern variable definitions
cc_map_t            cc_map;             ///< Comment delimeter character map.

////////// inline functions ///////////////////////////////////////////////////

/**
 * Initializes the comment character map.
 *
 * @sa cc_map_free()
 */
static inline void cc_map_init( void ) {
  MEM_ZERO( &cc_map );
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Frees the memory used by the comment delimiter character map.
 *
 * @sa cc_map_init()
 */
static void cc_map_free( void ) {
  for ( size_t i = 0; i < ARRAY_SIZE( cc_map ); ++i )
    FREE( cc_map[i] );
}

/**
 * Adds a comment delimiter character, and its corresponding closing comment
 * delimiter character (if any), to the given set.
 *
 * @param cc_set The set to add to.
 * @param c The character to add.
 * @return Returns the number of distinct comment delimiter characters added.
 */
NODISCARD
static unsigned cc_set_add( cc_set_t cc_set, char c ) {
  unsigned added = 0;
  if ( !cc_set[ STATIC_CAST( unsigned char, c ) ] ) {
    cc_set[ STATIC_CAST( unsigned char, c ) ] = true;
    ++added;
    char const closing = closing_char( c );
    if ( closing != '\0' && !cc_set[ (unsigned char)closing ] ) {
      cc_set[ STATIC_CAST( unsigned char, closing ) ] = true;
      ++added;
    }
  }
  return added;
}

////////// extern functions ///////////////////////////////////////////////////

char const* cc_map_compile( char const *in_cc ) {
  assert( in_cc != NULL );

  RUN_ONCE ATEXIT( &cc_map_free );

  cc_set_t cc_set = { false };
  unsigned distinct_cc = 0;             // distinct comment characters

  cc_map_free();
  cc_map_init();

  for ( char const *cc = in_cc; *cc != '\0'; ++cc ) {
    if ( isspace( *cc ) || *cc == ',' )
      continue;
    if ( !ispunct( *cc ) ) {
      fatal_error( EX_USAGE,
        "\"%s\": invalid value for %s;\n\tmust only be either:"
        " punctuation or whitespace characters\n",
        in_cc, opt_format( COPT(COMMENT_CHARS) )
      );
    }

    bool const is_double_cc = ispunct( cc[1] ) && cc[1] != ',';
    if ( is_double_cc && ispunct( cc[2] ) && cc[2] != ',' ) {
      fatal_error( EX_USAGE,
        "\"%s\": invalid value for %s: \"%c%c%c\":"
        " more than two consecutive comment characters\n",
        in_cc, opt_format( COPT(COMMENT_CHARS) ), cc[0], cc[1], cc[2]
      );
    }
    char const cc1 = is_double_cc ? cc[1] : CC_SINGLE_CHAR;

    unsigned char const *const ucc = STATIC_CAST( unsigned char*, cc );
    char *cc_map_entry = cc_map[ ucc[0] ];
    if ( cc_map_entry == NULL ) {
      cc_map_entry = MALLOC( char, 1 + 1/*\0*/ );
      cc_map_entry[0] = cc1;
      cc_map_entry[1] = '\0';
    }
    else if ( strchr( cc_map_entry, cc1 ) == NULL ) {
      //
      // Add the second comment delimiter character only if it's not already
      // there.
      //
      size_t const len = strlen( cc_map_entry );
      REALLOC( cc_map_entry, char, len + 1 + 1/*\0*/ );
      cc_map_entry[ len ] = cc1;
      cc_map_entry[ len + 1 ] = '\0';
    }
    cc_map[ ucc[0] ] = cc_map_entry;

    distinct_cc += cc_set_add( cc_set, cc[0] );
    if ( is_double_cc ) {
      distinct_cc += cc_set_add( cc_set, cc[1] );
      ++cc;                             // skip past second comment character
    }
  } // for

  if ( distinct_cc == 0 ) {
    fatal_error( EX_USAGE,
      "value for %s must not be only whitespace or commas\n",
      opt_format( COPT(COMMENT_CHARS) )
    );
  }

  char *const out_cc = free_later( MALLOC( char, distinct_cc + 1/*\0*/ ) );
  char *s = out_cc;
  for ( size_t i = 0; i < ARRAY_SIZE( cc_set ); ++i ) {
    if ( cc_set[i] )
      *s++ = STATIC_CAST( char, i );
  } // for
  *s = '\0';

#ifndef NDEBUG
  if ( is_affirmative( getenv( "WRAP_DUMP_CC_MAP" ) ) ) {
    for ( size_t i = 0; i < ARRAY_SIZE( cc_map ); ++i ) {
      if ( cc_map[i] )
        EPRINTF( "%c \"%s\"\n", STATIC_CAST( char, i ), cc_map[i] );
    } // for
    EPRINTF( "\n%u distinct = \"%s\"\n", distinct_cc, out_cc );
  }
#endif /* NDEBUG */

  return out_cc;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

extern inline bool cc_is_single( char const* );
extern inline char const* cc_map_get( char );

/* vim:set et sw=2 ts=2: */
