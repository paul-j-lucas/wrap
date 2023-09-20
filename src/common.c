/*
**      wrap -- text reformatter
**      src/common.c
**
**      Copyright (C) 2016-2023  Paul J. Lucas
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
#define W_COMMON_INLINE _GL_EXTERN_INLINE
#include "common.h"
#include "options.h"
#include "util.h"

// standard
#include <assert.h>

////////// extern functions ///////////////////////////////////////////////////

size_t check_readline( line_buf_t line, FILE *ffrom ) {
  assert( ffrom != NULL );
  size_t size = sizeof( line_buf_t );
  if ( fgetsz( line, &size, ffrom ) == NULL )
    FERROR( ffrom );
  return size;
}

void common_cleanup( void ) {
  free_now();
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
