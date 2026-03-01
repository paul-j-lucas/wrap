/*
**      wrap -- text reformatter
**      src/util.c
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
 * Defines utility data structures, variables, and functions.
 */

// local
#include "pjl_config.h"                 /* IWYU pragma: keep */
#include "wrap_term.h"
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <fcntl.h>                      /* for open(2) */
#include <stdio.h>
#include <stdlib.h>                     /* for malloc(), ... */
#include <sysexits.h>
#include <term.h>                       /* for setupterm(3) */
#include <unistd.h>                     /* for close(2) */

#if HAVE_CURSES_H
#  define _BOOL /* nothing */           /* prevent bool clash on AIX/Solaris */
#  include <curses.h>
#  undef _BOOL
#elif HAVE_NCURSES_H
#  include <ncurses.h>
#endif

/// @endcond

/**
 * @addtogroup terminal-group
 * @{
 */

////////// extern functions ///////////////////////////////////////////////////

unsigned term_get_columns( void ) {
  static unsigned const UNSET = STATIC_CAST( unsigned, -1 );
  static unsigned cols = UNSET;

  if ( cols == UNSET ) {
    cols = 0;

    int         cterm_fd = -1;
    char        reason_buf[ 128 ];
    char const *reason = NULL;

    char const *const term = getenv( "TERM" );
    if ( unlikely( term == NULL ) ) {
      reason = "TERM environment variable not set";
      goto error;
    }

    char const *const cterm_path = ctermid( NULL );
    if ( unlikely( cterm_path == NULL || *cterm_path == '\0' ) ) {
      reason = "ctermid(3) failed to get controlling terminal";
      goto error;
    }

    if ( unlikely( (cterm_fd = open( cterm_path, O_RDWR )) == -1 ) ) {
      reason = STRERROR();
      goto error;
    }

    int sut_err;
    if ( setupterm( CONST_CAST( char*, term ), cterm_fd, &sut_err ) == ERR ) {
      reason = reason_buf;
      switch ( sut_err ) {
        case -1:
          reason = "terminfo database not found";
          break;
        case 0:
          snprintf(
            reason_buf, sizeof reason_buf,
            "TERM=%s not found in database or too generic", term
          );
          break;
        case 1:
          reason = "terminal is harcopy";
          break;
        default:
          snprintf(
            reason_buf, sizeof reason_buf,
            "setupterm(3) returned error code %d", sut_err
          );
      } // switch
      goto error;
    }

    int const ti_cols = tigetnum( CONST_CAST( char*, "cols" ) );
    if ( unlikely( ti_cols < 0 ) ) {
      snprintf(
        reason_buf, sizeof reason_buf,
        "tigetnum(\"cols\") returned error code %d", ti_cols
      );
      goto error;
    }

    cols = STATIC_CAST( unsigned, ti_cols );

error:
    if ( likely( cterm_fd != -1 ) )
      close( cterm_fd );
    if ( unlikely( reason != NULL ) ) {
      fatal_error( EX_UNAVAILABLE,
        "failed to determine number of columns in terminal: %s\n",
        reason
      );
    }
  }

  return cols;
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
