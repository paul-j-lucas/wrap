/*
**      wrap -- text reformatter
**      src/wregex.c
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
#include "wregex.h"
#include "unicode.h"
#include "util.h"

// standard
#include <assert.h>
#include <stdbool.h>
#include <sysexits.h>
#include <wctype.h>

///////////////////////////////////////////////////////////////////////////////

// local constant definitions
static int const    WRAP_REGEX_COMPILE_FLAGS = REG_EXTENDED;

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a cp is a word character, that is an alphanumeric or \c _
 * character.  Note that this function is locale sensitive.
 *
 * @param cp The code-point to check.
 * @return Returns \c true only if \a cp is a word code-point.
 */
NODISCARD
static inline bool cp_is_word_char( char32_t cp ) {
  return iswalnum( (wint_t)cp ) || cp == '_';
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Checks whether the character at \a curr is the beginning of a word.  This
 * function exists because POSIX regular expressions don't support \c \\b
 * (match a word boundary).
 *
 * @param s The UTF-8 encoded string to check within.
 * @param curr A pointer to the first byte of the UTF-8 encoded character to
 * check.
 * @return Returns \c true only if the character at \a curr is at the beginning
 * of a word.
 */
NODISCARD
static bool is_begin_word_boundary( char const *s, char const *curr ) {
  assert( s != NULL );
  assert( curr != NULL );
  assert( curr >= s );

  if ( curr == s )
    return true;
  char const *const prev = utf8_rsync( s, curr - 1 );
  if ( prev == NULL )
    return true;

  char32_t const cp_curr = utf8_decode( curr );
  char32_t const cp_prev = utf8_decode( prev );

  return (cp_is_word_char( cp_curr ) ^ cp_is_word_char( cp_prev ))
      || (cp_is_space( cp_curr ) ^ cp_is_space( cp_prev ));
}

////////// extern functions ///////////////////////////////////////////////////

int regex_compile( wregex_t *re, char const *pattern ) {
  assert( re != NULL );
  assert( pattern != NULL );
  return regcomp( re, pattern, WRAP_REGEX_COMPILE_FLAGS );
}

char const* regex_error( wregex_t *re, int err_code ) {
  assert( re != NULL );
  static char err_buf[ 128 ];
  PJL_IGNORE_RV( regerror( err_code, re, err_buf, sizeof err_buf ) );
  return err_buf;
}

void regex_free( wregex_t *re ) {
  assert( re != NULL );
  regfree( re );
}

bool regex_match( wregex_t *re, char const *s, size_t offset, size_t *range ) {
  assert( re != NULL );
  assert( s != NULL );

  char const *const so = s + offset;
  regmatch_t match[ re->re_nsub + 1 ];

  int const err_code = regexec( re, so, re->re_nsub + 1, match, /*eflags=*/0 );

  if ( err_code == REG_NOMATCH )
    return false;
  if ( err_code < 0 )
    FATAL_ERR( EX_SOFTWARE,
      "regular expression error (%d): %s\n",
      err_code, regex_error( re, err_code )
    );
  if ( !is_begin_word_boundary( so, so + match[0].rm_so ) )
    return false;

  if ( range != NULL ) {
    range[0] = (size_t)match[0].rm_so + offset;
    range[1] = (size_t)match[0].rm_eo + offset;
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
