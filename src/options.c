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
  struct eol_map {
    char const *map_name;
    eol_t       map_eol;
  };
  typedef struct eol_map eol_map_t;

  static eol_map_t const eol_map[] = {
    { "-",        EOL_INPUT   },
    { "crlf",     EOL_WINDOWS },
    { "d",        EOL_WINDOWS },
    { "dos",      EOL_WINDOWS },
    { "i",        EOL_INPUT   },
    { "input",    EOL_INPUT   },
    { "lf",       EOL_UNIX    },
    { "u",        EOL_UNIX    },
    { "unix",     EOL_UNIX    },
    { "w",        EOL_WINDOWS },
    { "windows",  EOL_WINDOWS },
    { NULL,       EOL_INPUT   }
  };

  assert( s );
  char const *const s_lc = tolower_s( (char*)free_later( check_strdup( s ) ) );
  size_t names_buf_size = 1;            // for trailing null

  for ( eol_map_t const *m = eol_map; m->map_name; ++m ) {
    if ( strcmp( s_lc, m->map_name ) == 0 )
      return m->map_eol;
      // sum sizes of names in case we need to construct an error message
      names_buf_size += strlen( m->map_name ) + 2 /* ", " */;
  } // for

  // name not found: construct valid name list for an error message
  char *const names_buf = (char*)free_later( MALLOC( char, names_buf_size ) );
  char *pnames = names_buf;
  for ( eol_map_t const *m = eol_map; m->map_name; ++m ) {
    if ( pnames > names_buf ) {
      strcpy( pnames, ", " );
      pnames += 2;
    }
    strcpy( pnames, m->map_name );
    pnames += strlen( m->map_name );
  } // for
  PMESSAGE_EXIT( EX_USAGE,
    "\"%s\": invalid value for -%c; must be one of:\n\t%s\n",
    s, 'l', names_buf
  );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
