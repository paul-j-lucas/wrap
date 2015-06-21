/*
**      pjl_getopt() -- get option character from command line argument list
**      getopt.c
**
**      Copyright (C) 1992-2014  Paul J. Lucas
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
#include "common.h"
#include "getopt.h"

// standard
#include <assert.h>
#include <stdio.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

int pjl_getopt( int argc, char const *argv[], char const *opts ) {
  assert( opts );

  char const *cp;
  static int sp = 1;

  if ( sp == 1 ) {
    if ( optind >= argc || argv[ optind ][0] != '-' || !argv[ optind ][1] )
      return EOF;
    if ( !strcmp( argv[ optind ], "--" ) ) {
      ++optind;
      return EOF;
    }
  }
  optopt = argv[ optind ][ sp ];
  if ( optopt == ':' || !(cp = strchr( opts, optopt )) ) {
    if ( opterr )
      PRINT_ERR( "%s: '%c': illegal option\n", *argv, optopt );
    sp = 1;
    return '?';
  }
  if ( *++cp == ':' ) {
    if ( argv[ optind ][ sp+1 ] )
      optarg = (char*)&argv[ optind++ ][ sp+1 ];
    else if ( ++optind >= argc ) {
      if ( opterr )
        PRINT_ERR( "%s: '%c': option requires an argument\n", *argv, optopt );
      sp = 1;
      return '?';
    } else
      optarg = (char*)argv[ optind++ ];
    sp = 1;
  } else if ( *cp == '?' ) {
    //
    // Added the functionality of having optional arguments by JRP on 7/16/92.
    //
    if ( argv[ optind ][ sp+1 ] )
      optarg = (char*)&argv[ optind++ ][ sp+1 ];
    else if ( ++optind >= argc || *argv[ optind ] == '-' )
      optarg = NULL;
    else
      optarg = (char*)argv[ optind++ ];
    sp = 1;
  } else {
    if ( !argv[ optind ][ ++sp ] ) {
      ++optind;
      sp = 1;
    }
    optarg = NULL;
  }
  return optopt;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
