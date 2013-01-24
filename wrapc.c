/*
**      wrapc -- comment reformatter
**      wrapc.c: implementation
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
#include <stdlib.h>                     /* for exit(), malloc() */
#include <stdio.h>
#include <string.h>                     /* for str...() */
#include <unistd.h>                     /* for close(), fork(), ... */

/* local */
#include "c_compat.h"
#include "version.h"

#define BUF_SIZE    1024                /* hopefully, no one will exceed this */
#define TAB_EQUAL   8                   /* tabs are equal to 8 spaces */

#define ERROR( e )      { perror( argv[0] ); exit( e ); }
#define CLOSE( p )      { close( pipes[p][0] ); close( pipes[p][1] ); }
#define REDIRECT( s,p ) { close( s ); dup( pipes[p][s] ); CLOSE( p ); }

/*
** The default leading characters are: spaces and tabs, '!' (HTML and XML
** comments), '#' (Shell, Make, and Perl comments), '/' and '*' (C, C++, and
** Java comments), '%' (PostScript comments), ':' (XQuery comments), ';'
** (assember and Lisp comments), and '>' (mail forward indicator).
*/
char        *leading_chars = "\t !#%*/:;>";

char const  *me;                        /* executable name */
int         text_length = 80;           /* wrap text to this length */

void process_options PJL_PROTO(( int argc, char *argv[] ));

/*************************/
  int
main PJL_ARG_LIST(( argc, argv ))
  PJL_ARG_DEF( int argc )
  PJL_ARG_END( char *argv[] )
{
  char buf[ BUF_SIZE ];
  /*
  ** Two pipes: pipes[0] goes between child 1 and child 2
  **            pipes[1] goes between child 2 and parent
  */
  int pipes[2][2];
  pid_t pid;                            /* child process-id */
  FILE *from_wrap;                      /* file used by parent */

  /*
  ** Read the first line of input and obtain a string of leading characters to
  ** be removed from all lines.
  */
  char leader[ BUF_SIZE ];              /* string segment removed/prepended */
  int lead_length;                      /* number of leading characters */

  process_options( argc, argv );

  if ( !fgets( buf, BUF_SIZE, stdin ) )
    exit( 0 );
  strcpy( leader, buf );
  leader[ lead_length = strspn( buf, leading_chars ) ] = '\0';

  /* open pipes */
  if ( pipe( pipes[0] ) == -1 || pipe( pipes[1] ) == -1 )
    ERROR( 1 );

  /***************************************************************************/
  /*
  ** Child 1
  ** 
  ** Close both ends of pipes[1] since it doesn't use it; read from our
  ** original stdin and write to pipes[0].
  **
  ** Print first and all subsequent lines with the leading characters removed
  ** from the beginning of each line.
  */
  if ( ( pid = fork() ) == 0 ) {
    FILE *to_wrap;
    CLOSE( 1 );
    if ( !( to_wrap = fdopen( pipes[0][1], "w" ) ) ) {
      fprintf( stderr, "%s: child can't write to pipe\n", me);
      exit( -11 );
    }

    fputs( buf + lead_length, to_wrap );
    while ( fgets( buf, BUF_SIZE, stdin ) )
      fputs( buf + lead_length, to_wrap );
    exit( 0 );
  } else if ( pid == -1 )
    ERROR( -10 );

  /***************************************************************************/
  /*
  ** Child 2
  **
  ** Compute the actual length of the leader: tabs are equal to 8 spaces minus
  ** the number of spaces we're into a tab-stop, and all other are equal to 1.
  ** Subtract this from 80 to obtain the wrap length.
  **
  ** Read from pipes[0] and write to pipes[1]; exec into wrap.
  */
  if ( ( pid = fork() ) == 0 ) {
    char *c;
    int spaces = 0;
    char text_length_arg[4];

    for ( c = leader; *c; ++c )
      if ( *c == '\t' ) {
        text_length -= TAB_EQUAL - spaces;
        spaces = 0;
      } else {
        --text_length;
        spaces = (spaces + 1) % TAB_EQUAL;
      }

    sprintf( text_length_arg, "%d", text_length );

    REDIRECT( 0, 0 );
    REDIRECT( 1, 1 );
    execlp( "wrap", "wrap", "-l", text_length_arg, (char*)0 );
    ERROR( -21 );
  } else if ( pid == -1 )
    ERROR( -20 );

  /***************************************************************************/
  /*
  ** Parent
  **
  ** Close both ends of pipes[0] since it doesn't use it; close the write end
  ** of pipes[1].
  **
  ** Read from pipes[1] (the output from wrap) and prepend the leading text to
  ** each line.
  */
  CLOSE( 0 );
  close( pipes[1][1] );

  from_wrap = fdopen( pipes[1][0], "r" );
  if ( !from_wrap ) {
    fprintf( stderr, "%s: parent can't read from pipe\n", me );
    exit( 1 );
  }

  while ( fgets( buf, BUF_SIZE, from_wrap ) )
    printf( "%s%s", leader, buf );
  exit( 0 );
}

/*************************/
  void
process_options PJL_ARG_LIST(( argc, argv ))
  PJL_ARG_DEF( int argc )
  PJL_ARG_END( char *argv[] )
{
  extern char *optarg;
  extern int optind, opterr;
  int opt;                              /* command-line option */

  me = strrchr( argv[0], '/' );         /* determine base name... */
  me = me ? me + 1 : argv[0];           /* ...of executable */

  opterr = 1;
  while ( ( opt = getopt( argc, (char**)argv, "l:v" ) ) != EOF )
    switch ( opt ) {
      case 'l':
        text_length = atoi( optarg );
        break;
      case 'v':
        fprintf( stderr, "%s %s\n", me, WRAP_VERSION );
        exit( 0 );
      case '?':
        goto usage;
    }
  argc -= optind, argv += optind;
  if ( !argc )
    return;

usage:
  fprintf( stderr, "usage: %s [-l text-length]\n", me );
  exit( 1 );
}
/* vim:set et sw=2 ts=2: */
