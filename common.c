/*
**      wrap -- text reformatter
**      common.c: implementation
**
**      Copyright (C) 1996-2013  Paul J. Lucas
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
**      along with this program; if not, write to the Free Software
**      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* system */
#include <stdio.h>
#include <stdlib.h>                     /* for atoi() ... */
#include <string.h>

/* local */
#include "common.h"

extern char const *me;

/****************************************************************************/

int check_atoi( char const *s ) {
  if ( strspn( s, "0123456789" ) != strlen( s ) ) {
    fprintf( stderr, "%s: \"%s\": invalid integer\n", me, s );
    exit( EXIT_USAGE );
  }
  return atoi( s );
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
