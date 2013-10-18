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
#include <errno.h>                      /* for errno */
#include <stdlib.h>                     /* for exit(), malloc() */
#include <stdio.h>
#include <string.h>                     /* for str...() */
#include <unistd.h>                     /* for close(), fork(), ... */

/* local */
#include "common.h"

#define ARG_BUF_SIZE  22                /* max wrap command-line arg size */

#define ARG_SPRINTF(ARG,FMT) \
  snprintf( arg_##ARG, sizeof arg_##ARG, (FMT), (ARG) )

#define CLOSE(P)        { close( pipes[P][0] ); close( pipes[P][1] ); }
#define REDIRECT(FD,P)  { close( FD ); dup( pipes[P][FD] ); CLOSE( P ); }

typedef char arg_buf[ ARG_BUF_SIZE ];   /* wrap command-line argument buffer */

/*
** The default leading characters are:
**  space and tab
**  '!': HTML & XML comments
**  '#': CMake, Make, Perl, Python, Ruby, and Shell comments
**  '/' & '*': C, C++, and Java comments
**  '%': PostScript comments
**  ':': XQuery comments
**  ';': Assember & Lisp comments
**  '>': Forward mail indicator
*/
char const* leading_chars = "\t !#%*/:;>";

FILE*       fin = NULL;                 /* file in */
FILE*       fout = NULL;                /* file out */
int         line_length = DEFAULT_LINE_LENGTH;
char const* me;                         /* executable name */
int         tab_spaces = DEFAULT_TAB_SPACES;

/* local functions */
static void process_options( int argc, char *argv[] );

/*****************************************************************************/

int main( int argc, char *argv[] ) {
  char  buf[ LINE_BUF_SIZE ];
  FILE* from_wrap;                      /* file used by parent */
  char  leader[ LINE_BUF_SIZE ];        /* string segment removed/prepended */
  int   lead_length;                    /* number of leading characters */
  pid_t pid;                            /* child process-id */
  /*
  ** Two pipes: pipes[0] goes between child 1 and child 2 (wrap)
  **            pipes[1] goes between child 2 (wrap) and parent
  */
  int   pipes[2][2];

  process_options( argc, argv );

  /*
  ** Read the first line of input and obtain a string of leading characters to
  ** be removed from all lines.
  */
  if ( !fgets( buf, LINE_BUF_SIZE, fin ) ) {
    if ( ferror( fin ) )
      ERROR( EXIT_READ_ERROR );
    exit( EXIT_OK );
  }
  strcpy( leader, buf );
  leader[ lead_length = strspn( buf, leading_chars ) ] = '\0';

  /* open pipes */
  if ( pipe( pipes[0] ) == -1 || pipe( pipes[1] ) == -1 )
    ERROR( EXIT_PIPE_ERROR );

  /***************************************************************************/
  /*
  ** Child 1
  ** 
  ** Close both ends of pipes[1] since it doesn't use it; read from our
  ** original stdin and write to pipes[0] (child 2, wrap).
  **
  ** Print the first and all subsequent lines with the leading characters
  ** removed from the beginning of each line.
  */
  if ( (pid = fork()) == 0 ) {
    FILE *to_wrap;
    CLOSE( 1 );
    if ( !( to_wrap = fdopen( pipes[0][1], "w" ) ) ) {
      fprintf(
        stderr, "%s: child can't open pipe for writing: %s\n",
        me, strerror( errno )
      );
      exit( EXIT_OPEN_WRITE );
    }

    if ( fputs( buf + lead_length, to_wrap ) == EOF )
      ERROR( EXIT_WRITE_ERROR );
    while ( fgets( buf, LINE_BUF_SIZE, fin ) ) {
      fputs( buf + lead_length, to_wrap );
      if ( ferror( to_wrap ) )
        ERROR( EXIT_WRITE_ERROR );
    }
    if ( ferror( fin ) )
      ERROR( EXIT_READ_ERROR );
    exit( EXIT_OK );
  } else if ( pid == -1 )
    ERROR( EXIT_FORK_ERROR );

  /***************************************************************************/
  /*
  ** Child 2
  **
  ** Compute the actual length of the leader: tabs are equal to <tab_spaces>
  ** spaces minus the number of spaces we're into a tab-stop, and all others
  ** are equal to 1.  Subtract this from <line_length> to obtain the wrap
  ** length.
  **
  ** Read from pipes[0] (child 1) and write to pipes[1] (parent); exec into
  ** wrap.
  */
  if ( (pid = fork()) == 0 ) {
    arg_buf arg_line_length;
    arg_buf arg_tab_spaces;
    char *c;
    int spaces = 0;

    for ( c = leader; *c; ++c )
      if ( *c == '\t' ) {
        line_length -= tab_spaces - spaces;
        spaces = 0;
      } else {
        --line_length;
        spaces = (spaces + 1) % tab_spaces;
      }

    ARG_SPRINTF( line_length, "-l%d" );
    ARG_SPRINTF( tab_spaces , "-s%d" );

    REDIRECT( STDIN_FILENO, 0 );
    REDIRECT( STDOUT_FILENO, 1 );
    execlp(
      "wrap", "wrap", "-l", arg_line_length, "-s", arg_tab_spaces, (char*)0
    );
    ERROR( EXIT_EXEC_ERROR );
  } else if ( pid == -1 )
    ERROR( EXIT_FORK_ERROR );

  /***************************************************************************/
  /*
  ** Parent
  **
  ** Close both ends of pipes[0] since it doesn't use it; close the write end
  ** of pipes[1].
  **
  ** Read from pipes[1] (child 2, wrap) and prepend the leading text to each
  ** line.
  */
  CLOSE( 0 );
  close( pipes[1][1] );

  if ( !(from_wrap = fdopen( pipes[1][0], "r" )) ) {
    fprintf(
      stderr, "%s: parent can't open pipe for reading: %s\n",
      me, strerror( errno )
    );
    exit( EXIT_READ_ERROR );
  }

  while ( fgets( buf, LINE_BUF_SIZE, from_wrap ) ) {
    fprintf( fout, "%s%s", leader, buf );
    if ( ferror( fout ) )
      ERROR( EXIT_WRITE_ERROR );
  }
  if ( ferror( from_wrap ) )
    ERROR( EXIT_READ_ERROR );
  exit( EXIT_OK );
}

/*****************************************************************************/

static void process_options( int argc, char *argv[] ) {
  extern char *optarg;
  extern int optind, opterr;
  int opt;                              /* command-line option */

  me = strrchr( argv[0], '/' );         /* determine base name... */
  me = me ? me + 1 : argv[0];           /* ...of executable */

  opterr = 1;
  while ( (opt = getopt( argc, argv, "f:l:o:s:v" )) != EOF )
    switch ( opt ) {
      case 'f':
        if ( !(fin = fopen( optarg, "r" )) )
          ERROR( EXIT_OPEN_READ );
        break;
      case 'l': line_length = atoi( optarg ); break;
      case 'o':
        if ( !(fout = fopen( optarg, "w" )) )
          ERROR( EXIT_OPEN_WRITE );
        break;
      case 's': tab_spaces  = atoi( optarg ); break;
      case 'v': goto version;
      case '?': goto usage;
    }
  argc -= optind, argv += optind;
  if ( argc )
    goto usage;

  if ( !fin )
    fin = fdopen( STDIN_FILENO, "r");
  if ( !fout )
    fout = fdopen( STDOUT_FILENO, "w" );

  return;

usage:
  fprintf( stderr, "usage: %s [-l line-length] [-s tab-spaces]\n", me );
  fprintf( stderr, "\t[-f input-file]   [-o output-file]\n" );
  exit( EXIT_USAGE );

version:
  fprintf( stderr, "%s %s\n", me, WRAP_VERSION );
  exit( EXIT_OK );
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
