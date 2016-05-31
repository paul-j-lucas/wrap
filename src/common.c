/*
**      wrap -- text reformatter
**      common.c
**
**      Copyright (C) 2016  Paul J. Lucas
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
#include "alias.h"
#include "common.h"
#include "markdown.h"
#include "options.h"
#include "pattern.h"
#include "util.h"

// standard
#include <assert.h>

////////// extern functions ///////////////////////////////////////////////////

void clean_up( void ) {
  alias_cleanup();
  free_now();
  markdown_cleanup();
  pattern_cleanup();
  if ( fin )
    fclose( fin );
  if ( fout )
    fclose( fout );
}

size_t buf_read( line_buf_t line, FILE *ffrom ) {
  assert( ffrom );
  size_t size = sizeof( line_buf_t );
  if ( !fgetsz( line, &size, ffrom ) )
    FERROR( ffrom );
  return size;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
