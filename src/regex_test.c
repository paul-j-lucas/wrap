/*
**      wrap -- text reformatter
**      src/regex_test.c
**
**      Copyright (C) 2017-2023  Paul J. Lucas
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
#include "util.h"
#include "wregex.h"

// standard
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

///////////////////////////////////////////////////////////////////////////////

// local constant definitions
static char const TEST_SEP = ' ';

// extern variable definitions
char const       *me;                   ///< Program name.

////////// local functions ////////////////////////////////////////////////////

_Noreturn
static void usage( void ) {
  EPRINTF( "usage: %s test\n", me );
  exit( EX_USAGE );
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *argv[] ) {
  me = base_name( argv[0] );
  if ( --argc != 1 )
    usage();

  char const *const test_path = argv[1];
  FILE *const fin = fopen( test_path, "r" );
  if ( fin == NULL )
    fatal_error( EX_NOINPUT, "\"%s\": %s\n", test_path, STRERROR() );

  setlocale_utf8();

  wregex_t re;
  int const regex_err_code = regex_compile( &re, WRAP_RE );
  if ( regex_err_code != 0 ) {
    fatal_error( EX_SOFTWARE,
      "internal regular expression error (%d): %s\n",
      regex_err_code, regex_error( &re, regex_err_code )
    );
  }

  char line_buf[ 128 ];
  unsigned line_no = 0;
  unsigned mismatches = 0;

  while ( fgets( line_buf, sizeof line_buf, fin ) != NULL ) {
    ++line_no;
    if ( line_buf[0] == '#' || is_blank_line( line_buf ) )
      continue;

    char *const sep = strchr( line_buf, TEST_SEP );
    if ( sep == NULL ) {
      fatal_error( EX_DATAERR,
        "%s:%u: missing separator '%c'\n",
        test_path, line_no, TEST_SEP
      );
    }

    *sep = '\0';
    char const *const expected = line_buf;
    size_t const expected_len = STATIC_CAST( size_t, sep - expected );
    char const *const subject = sep + 1;

    size_t match_range[2];
    bool const matched = regex_match( &re, subject, 0, match_range );

    if ( !matched ) {
      if ( expected_len > 0 ) {
        EPRINTF(
          "%s:%u: <%s> wasn't matched when it should have been\n",
          test_path, line_no, expected
        );
        ++mismatches;
      }
      continue;
    }

    char const *const match_begin = subject + match_range[0];
    size_t const match_len = match_range[1] - match_range[0];
    char match[ 128 ];
    strncpy( match, match_begin, match_len );
    match[ match_len ] = '\0';

    if ( expected_len == 0 ) {
      EPRINTF(
        "%s:%u: <%s> matched when it shouldn't have\n",
        test_path, line_no, match
      );
      ++mismatches;
      continue;
    }

    if ( match_len != expected_len || strcmp( match, expected ) != 0 ) {
      EPRINTF(
        "%s:%u: match <%s> does not equal expected <%s>\n",
        test_path, line_no, match, expected
      );
      ++mismatches;
    }
  } // while

  regex_free( &re );

  FERROR( fin );
  PJL_IGNORE_RV( fclose( fin ) );

  printf( "%u mismatches\n", mismatches );
  exit( mismatches > 0 ? EX_SOFTWARE : EX_OK );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
