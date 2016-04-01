/*
**      wrap -- text reformatter
**      options.c
**
**      Copyright (C) 1996-2015  Paul J. Lucas
**
**      This program is free software; you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the License, or
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
#include "options.h"
#include "util.h"

// standard
#include <assert.h>

extern char const  *me;
opts_given_t        opts_given;

////////// extern functions ///////////////////////////////////////////////////

void check_mutually_exclusive( char const *opts1, char const *opts2 ) {
  assert( opts1 );
  assert( opts2 );

  int gave_count = 0;
  char const *opt = opts1;
  char gave_opt1 = '\0';

  for ( int i = 0; i < 2; ++i ) {
    for ( ; *opt; ++opt ) {
      if ( GAVE_OPTION( *opt ) ) {
        if ( ++gave_count > 1 ) {
          char const gave_opt2 = *opt;
          PMESSAGE_EXIT( EX_USAGE,
            "-%c and -%c are mutually exclusive\n", gave_opt1, gave_opt2
          );
        }
        gave_opt1 = *opt;
        break;
      }
    } // for
    if ( !gave_count )
      break;
    opt = opts2;
  } // for
}

eol_t parse_eol( char const *s ) {
  assert( s );
  switch ( tolower( s[0] ) ) {
    case 'i':
    case '-':
      return EOL_INPUT;
    case 'u': // unix
      return EOL_UNIX;
    case 'd': // dos
    case 'w': // windows
      return EOL_WINDOWS;
    default:
      PMESSAGE_EXIT( EX_USAGE,
        "\"%s\": invalid value for -l; must be one of [-diuw]\n", s
      );
  } // switch
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
