/*
**      wrapc -- comment reformatter
**      src/align.c
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
#include "wrap.h"                       /* must go first */
#include "cc_map.h"
#include "common.h"
#include "options.h"
#include "util.h"

// standard
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <string.h>                     /* for str...() */

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets whether \a s starts an end-of-line comment.
 *
 * @param s The null-terminated string to check.
 * @return Returns \c true only if \a s starts an end-of-line comment.
 */
static bool is_eol_comment( char const *s ) {
  char const *const cc = cc_map_get( *s );
  if ( cc == NULL )
    return false;

  char closing = closing_char( *s );

  if ( cc_is_single( cc ) ) {
    //
    // Single-character comment delimiter, e.g., '#' (Python) or '{' (Pascal).
    //
    if ( closing == '\0' ) {
      //
      // A single-character comment delimiter that has no closing character,
      // e.g., '#', invariably is a comment to the end of the line.
      //
      return true;
    }

    //
    // We're dealing with a case like '{' ... '}' (Pascal): we have to attempt
    // to find the closing comment delimiter character.
    //
    for (;;) {
      if ( *s == '\0' )
        return false;
      if ( *s++ == closing ) {
        //
        // We found the closing comment delimiter character: now check to see
        // if there's non-whitespace characters after it, e.g.:
        //
        //      { comment } something else?
        //
        // If so, then this comment isn't an end-of-line comment.
        //
        return is_blank_line( s );
      }
    } // for
  }

  if ( s[1] == s[0] ) {
    //
    // A two-character comment delimiter where both characters are the same,
    // e.g., "//", invariably is a comment to the end of the line.
    //
    return true;
  }

  char const d1 = *s;                   // save first delimiter character
  if ( !(*++s != '\0' && strchr( cc, *s ) != NULL) ) {
    //
    // If the next character isn't the second character in a two-character
    // comment delimiter, then it's not a comment.
    //
    return false;
  }

  if ( closing == '\0' ) {
    //
    // If the first character in a two-character comment delimiter doesn't
    // have a closing character, then it's its own closing character, e.g.,
    // "/*" ... "*/".
    //
    closing = d1;
  }

  //
  // Attempt to find the closing comment delimiter characters.
  //
  char const d2 = *s;                   // save second delimiter character
  for ( char c_prev = '\0'; *++s != '\0'; c_prev = *s ) {
    if ( c_prev == d2 && *s == closing ) {
      //
      // We found the closing comment delimiter characters: now it's just like
      // the single-character comment delimiter case above.
      //
      return is_blank_line( ++s );
    }
  } // for

  return false;
}

////////// extern functions ///////////////////////////////////////////////////

void align_eol_comments( char input_buf[] ) {
  do {
    size_t      col = 0;
    bool        is_backslash = false;   // got a backslash?
    bool        is_word = false;        // got a word character?
    ssize_t     last_nonws_col = -1;    // last non-whitespace column
    ssize_t     last_nonws_len = -1;    // length to non-whitespace character
    char        last_ws = ' ';          // last whitespace encountered
    line_buf_t  output_buf;
    size_t      output_len = 0;
    char        quote = '\0';           // between quotes?
    unsigned    token_count = 0;

    for ( char const *s = input_buf; *s != '\0' && !is_eol( *s ); ++s ) {
      bool const was_backslash = true_reset( &is_backslash );
      bool const was_word = true_reset( &is_word );

      switch ( *s ) {
        case '"':
        case '\'':
          if ( quote == '\0' ) {
            quote = *s;
          }
          else if ( !was_backslash && *s == quote ) {
            quote = '\0';
            ++token_count;
          }
          break;
        case '\\':
          is_backslash = true;
          break;

        default:
          if ( quote != '\0' )          // do nothing else while between quotes
            break;

          if ( is_eol_comment( s ) ) {
            //
            // Align comment only if:
            //
            //  1. It is the last thing on the line -- so a comment within a
            //     line is not aligned, e.g.:
            //
            //          char cc_buf[ 3 + 1/*null*/ ];
            //
            //  2. There is more than one "token" on the line before the
            //     comment -- so comments like:
            //
            //            } // for
            //          #endif /* NDEBUG */
            //
            //     are not aligned.  A "token" is one of:
            //
            //        * A "word": an optional '#' followed by one or more
            //          alpha-numeric characters.
            //        * A single punctuation character.
            //        * A single- or-double-quoted string.
            //
            // An end-of-line comment that does not meet these criteria is
            // passed through verbatim (except that the line-ending is replaced
            // by whatever the chosen line-ending is).
            //
            if ( token_count > 1 ) {
              if ( opt_align_char == '\0' ) {
                //
                // The user hasn't specified an alignment character: use
                // whatever the first character is after the last non-
                // whitespace character before the comment.  If that isn't a
                // whitespace character, use whatever the last whitespace
                // character we encountered was.
                //
                char const c = input_buf[ last_nonws_len + 1 ];
                opt_align_char = isspace( c ) ? c : last_ws;
              }

              //
              // Reset the column and length to those of the last non-
              // whitespace character before the comment.  We want to replace
              // all the whitespace between there and the comment with
              // opt_align_char.
              //
              col = last_nonws_col;
              output_len = last_nonws_len;

              //
              // While we're less than the alignment column, insert whitespace.
              //
              while ( col < opt_align_column - 1 ) {
                size_t width = char_width( opt_align_char, col );
                if ( col + width > opt_align_column ) {
                  //
                  // If width > 1 (as it could be when using tabs) and the new
                  // column > the alignment column, fall back to using spaces.
                  //
                  width = 1;
                  opt_align_char = ' ';
                }
                col += width;
                output_buf[ output_len++ ] = opt_align_char;
              } // while
            }

            //
            // Copy the comment without the end-of-line so we can replace it by
            // whatever the chosen line-ending is.
            //
            output_len += strcpy_len( output_buf + output_len, s );
            output_len = chop_eol( output_buf, output_len );
            goto print_line;
          }

          if ( ispunct( *s ) ) {
            if ( s[0] == '#' && isalnum( s[1] ) && !was_word ) {
              //
              // Special case: allow '#' to start words so C/C++ preprocessor
              // directives, e.g., #endif, are considered single tokens.
              //
              is_word = true;
            }
            ++token_count;
          }
          else if ( isalnum( *s ) ) {
            if ( !was_word )
              ++token_count;
            is_word = true;
          }
      } // switch

      col += char_width( *s, col );
      output_buf[ output_len++ ] = *s;

      if ( quote == '\0' ) {
        //
        // Keep track of the last whitespace character and non-whitespace
        // column & position.
        //
        if ( is_space( *s ) ) {
          last_ws = *s;
        } else {
          last_nonws_col = col;
          last_nonws_len = s - input_buf + 1;
        }
      }
    } // for

print_line:
    output_buf[ output_len ] = '\0';
    W_FPRINTF( fout, "%s%s", output_buf, eol() );
  } while ( check_readline( input_buf, fin ) );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
