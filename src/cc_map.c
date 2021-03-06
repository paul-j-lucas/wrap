/*
**      wrapc -- comment reformatter
**      src/cc_map.c
**
**      Copyright (C) 2017  Paul J. Lucas
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
#include "pjl_config.h"                 /* must go first */
/// @cond DOXYGEN_IGNORE
#define W_CC_MAP_INLINE _GL_EXTERN_INLINE
/// @endcond
#include "cc_map.h"
#include "options.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <string.h>                     /* for str...() */

///////////////////////////////////////////////////////////////////////////////

/**
 * Comment delimiter character set indexed by character.
 */
typedef bool cc_set_t[128];

// extern variable definitions
cc_map_t            cc_map;

////////// inline functions ///////////////////////////////////////////////////

/**
 * Initializes the comment character map.
 */
static inline void cc_map_init( void ) {
  memset( cc_map, 0, sizeof cc_map );
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Adds a comment delimiter character, and its corresponding closing comment
 * delimiter character (if any), to the given set.
 *
 * @param cc_set The set to add to.
 * @param c The character to add.
 * @return Returns the number of distinct comment delimiter characters added.
 */
PJL_WARN_UNUSED_RESULT
static unsigned cc_set_add( cc_set_t cc_set, char c ) {
  unsigned added = 0;
  if ( !cc_set[ (unsigned char)c ] ) {
    cc_set[ (unsigned char)c ] = true;
    ++added;
    char const closing = closing_char( c );
    if ( closing != '\0' && !cc_set[ (unsigned char)closing ] ) {
      cc_set[ (unsigned char)closing ] = true;
      ++added;
    }
  }
  return added;
}

////////// extern functions ///////////////////////////////////////////////////

char const* cc_map_compile( char const *in_cc ) {
  assert( in_cc != NULL );

  cc_set_t cc_set = { false };
  unsigned distinct_cc = 0;             // distinct comment characters
  char opt_buf[ OPT_BUF_SIZE ];         // to format options in error messages

  cc_map_free();
  cc_map_init();

  for ( char const *cc = in_cc; *cc != '\0'; ++cc ) {
    if ( isspace( *cc ) || *cc == ',' )
      continue;
    if ( !ispunct( *cc ) )
      PMESSAGE_EXIT( EX_USAGE,
        "\"%s\": invalid value for %s;\n\tmust only be either: %s\n",
        in_cc, format_opt( 'D', opt_buf, sizeof opt_buf ),
        "punctuation or whitespace characters"
      );

    bool const is_double_cc = ispunct( cc[1] ) && cc[1] != ',';
    if ( is_double_cc && ispunct( cc[2] ) && cc[2] != ',' )
      PMESSAGE_EXIT( EX_USAGE,
        "\"%s\": invalid value for %s: \"%c%c%c\": %s\n",
        in_cc, format_opt( 'D', opt_buf, sizeof opt_buf ), cc[0], cc[1], cc[2],
        "more than two consecutive comment characters"
      );
    char const cc1 = is_double_cc ? cc[1] : CC_SINGLE_CHAR;

    unsigned char const *const ucc = (unsigned char*)cc;
    char *cc_map_entry = cc_map[ ucc[0] ];
    if ( cc_map_entry == NULL ) {
      cc_map_entry = MALLOC( char, 1 + 1/*null*/ );
      cc_map_entry[0] = cc1;
      cc_map_entry[1] = '\0';
    }
    else if ( strchr( cc_map_entry, cc1 ) == NULL ) {
      //
      // Add the second comment delimiter character only if it's not already
      // there.
      //
      size_t const len = strlen( cc_map_entry );
      REALLOC( cc_map_entry, char, len + 1 + 1/*null*/ );
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

  if ( distinct_cc == 0 )
    PMESSAGE_EXIT( EX_USAGE,
      "value for %s must not be only whitespace or commas\n",
      format_opt( 'D', opt_buf, sizeof opt_buf )
    );

  char *const out_cc = FREE_STRBUF_LATER( distinct_cc + 1/*null*/ );
  char *s = out_cc;
  for ( size_t i = 0; i < ARRAY_SIZE( cc_set ); ++i ) {
    if ( cc_set[i] )
      *s++ = (char)i;
  } // for
  *s = '\0';

#ifndef NDEBUG
  if ( is_affirmative( getenv( "WRAP_DUMP_CC_MAP" ) ) ) {
    for ( size_t i = 0; i < ARRAY_SIZE( cc_map ); ++i ) {
      if ( cc_map[i] )
        PRINT_ERR( "%c \"%s\"\n", (char)i, cc_map[i] );
    } // for
    PRINT_ERR( "\n%u distinct = \"%s\"\n", distinct_cc, out_cc );
  }
#endif /* NDEBUG */

  return out_cc;
}

void cc_map_free( void ) {
  for ( size_t i = 0; i < ARRAY_SIZE( cc_map ); ++i )
    FREE( cc_map[i] );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
