/*
**      wrap -- text reformatter
**      wregex.c
**
**      Copyright (C) 2017  Paul J. Lucas
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
#include "wregex.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <sysexits.h>

///////////////////////////////////////////////////////////////////////////////

// local constant definitions
static int const    WRAP_REGEX_COMPILE_FLAGS = REG_EXTENDED;

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a c is a word character, that is an alphanumeric or \c _
 * character.  Note that this function is locale sensitive.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is a word character.
 */
static inline bool is_word_char( char c ) {
  return isalnum( c ) || c == '_';
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Checks whether the character at \a pos is the beginning of a word.  This
 * function exists because POSIX regular expressions don't support \c \\b
 * (match a word boundary).
 *
 * @param s The string to check within.
 * @param pos The position of the character to check.
 * @return Returns \c true only if the character at \a pos is at the beginning
 * of a word.
 */
static bool is_begin_word_boundary( char const *s, size_t pos ) {
  if ( pos == 0 )
    return true;
  char const *const begin = s + pos;
  return  (!is_word_char( begin[0] ) || !is_word_char( begin[-1] )) ||
          (isspace( begin[-1] ) && !isspace( begin[0] ));
}

/**
 * Prints a regular expression error to standard error and exits.
 *
 * @param re The regex_t involved in the error.
 * @param err_code The error code.
 */
static void regex_error( regex_t *re, int err_code ) {
  assert( re );
  char err_buf[ 128 ];
  regerror( err_code, re, err_buf, sizeof err_buf );
  PMESSAGE_EXIT( EX_SOFTWARE,
    "regular expression error (%d): %s\n", err_code, err_buf
  );
}

////////// extern functions ///////////////////////////////////////////////////

void regex_free( regex_t *re ) {
  assert( re );
  regfree( re );
}

void regex_init( regex_t *re, char const *pattern ) {
  assert( re );
  assert( pattern );

  int const err_code = regcomp( re, pattern, WRAP_REGEX_COMPILE_FLAGS );
  if ( err_code != 0 )
    regex_error( re, err_code );
}

bool regex_match( regex_t *re, char const *s, size_t offset, size_t range[2] ) {
  assert( re );
  assert( s );

  char const *const so = s + offset;
  regmatch_t match[ re->re_nsub + 1 ];

  int const err_code = regexec( re, so, re->re_nsub + 1, match, /*eflags=*/0 );

  if ( err_code == REG_NOMATCH )
    return false;
  if ( err_code < 0 )
    regex_error( re, err_code );
  if ( !is_begin_word_boundary( so, match[0].rm_so ) )
    return false;

  range[0] = (size_t)match[0].rm_so + offset;
  range[1] = (size_t)match[0].rm_eo + offset;
  return true;
}


///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
