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
#include <signal.h>                     /* for kill() */
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), malloc() */
#include <string.h>                     /* for str...() */
#include <sys/wait.h>                   /* for wait() */
#include <unistd.h>                     /* for close(), fork(), ... */

/* local */
#include "common.h"

#define ARG_BUF_SIZE  22                /* max wrap command-line arg size */

/* String-ify a command-line argument. */
#define ARG_SPRINTF(ARG,FMT) \
  snprintf( arg_##ARG, sizeof arg_##ARG, (FMT), (ARG) )

/* Close both ends of pipe P. */
#define CLOSE(P) \
  do { close( pipes[P][0] ); close( pipes[P][1] ); } while (0)

/* Redirect file-descriptor FD to/from pipe P. */
#define REDIRECT(FD,P) \
  do { close( FD ); dup( pipes[P][FD] ); CLOSE( P ); } while (0)

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

/* option definitions */
bool        opt_eos_delimit = false;    /* end-of-sentence delimits para's? */
char const* opt_para_delimiters = NULL; /* additional para delimiter chars */

/* local functions */
static void process_options( int argc, char *argv[] );
static char const* str_status( int status );

/*****************************************************************************/

int main( int argc, char *argv[] ) {
  char  buf[ LINE_BUF_SIZE ];
  FILE* from_wrap;                      /* file used by parent */
  char  leader[ LINE_BUF_SIZE ];        /* string segment removed/prepended */
  int   lead_length;                    /* number of leading characters */
  pid_t pid, pid_1;                     /* child process-IDs */
  /*
  ** Two pipes: pipes[0] goes between child 1 and child 2 (wrap)
  **            pipes[1] goes between child 2 (wrap) and parent
  */
  int   pipes[2][2];
  int   wait_status;                    /* child process wait status */

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
  if ( (pid_1 = fork()) == -1 )
    ERROR( EXIT_FORK_ERROR );
  if ( !pid_1 ) {
    FILE *to_wrap;
    CLOSE( 1 );
    if ( !(to_wrap = fdopen( pipes[0][1], "w" )) ) {
      fprintf(
        stderr, "%s: child can't open pipe for writing: %s\n",
        me, strerror( errno )
      );
      exit( EXIT_WRITE_OPEN );
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
  }

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
  if ( (pid = fork()) == -1 ) {
    kill( pid_1, SIGTERM );
    ERROR( EXIT_FORK_ERROR );
  }
  if ( !pid ) {
    arg_buf arg_line_length;
    arg_buf arg_opt_para_delimiters;
    arg_buf arg_tab_spaces;
    int argc = 0;
    char *argv[6];                      /* must be +1 of greatest arg below */
    char const *c;
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

    argv[ argc++ ] = strdup( "wrap" );  /* 0 */
    if ( opt_eos_delimit )              /* 1: -e */
      argv[ argc++ ] = strdup( "-e" );
    argv[ argc++ ] = arg_line_length;   /* 2: -l */
    if ( opt_para_delimiters ) {        /* 3: -p */
      ARG_SPRINTF( opt_para_delimiters, "-p%s" );
      argv[ argc++ ] = arg_opt_para_delimiters;
    }
    argv[ argc++ ] = arg_tab_spaces;    /* 4: -s */
    argv[ argc ] = (char*)0;            /* 5 */

    REDIRECT( STDIN_FILENO, 0 );
    REDIRECT( STDOUT_FILENO, 1 );
    execvp( "wrap", argv );
    ERROR( EXIT_EXEC_ERROR );
  }

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
    exit( EXIT_READ_OPEN );
  }

  while ( fgets( buf, LINE_BUF_SIZE, from_wrap ) ) {
    fprintf( fout, "%s%s", leader, buf );
    if ( ferror( fout ) )
      ERROR( EXIT_WRITE_ERROR );
  }
  if ( ferror( from_wrap ) )
    ERROR( EXIT_READ_ERROR );

  /*
  ** Wait for child processes.
  */
  while ( (pid = wait( &wait_status )) > 0 ) {
    if ( WIFEXITED( wait_status ) ) {
      int exit_status = WEXITSTATUS( wait_status );
      if ( exit_status != 0 ) {
        fprintf(
          stderr,
          "%s: child process exited with status %d: %s\n",
          me, exit_status, str_status( exit_status )
        );
        exit( exit_status );
      }
    } else if ( WIFSIGNALED( wait_status ) ) {
      int signal = WTERMSIG( wait_status );
      fprintf(
        stderr,
        "%s: child process terminated with signal %d: %s\n",
        me, signal, strsignal( signal )
      );
      exit( EXIT_CHILD_SIGNAL );
    }
  } /* while */

  exit( EXIT_OK );
}

/*****************************************************************************/

static void process_options( int argc, char *argv[] ) {
  extern char *optarg;
  extern int optind, opterr;
  int opt;                              /* command-line option */
  char const opts[] = "ef:l:o:p:s:v";

  me = strrchr( argv[0], '/' );         /* determine base name... */
  me = me ? me + 1 : argv[0];           /* ...of executable */

  opterr = 1;
  while ( (opt = getopt( argc, argv, opts )) != EOF )
    switch ( opt ) {
      case 'e': opt_eos_delimit = true;             break;
      case 'f':
        if ( !(fin = fopen( optarg, "r" )) )
          ERROR( EXIT_READ_OPEN );
        break;
      case 'l': line_length = check_atoi( optarg ); break;
      case 'o':
        if ( !(fout = fopen( optarg, "w" )) )
          ERROR( EXIT_WRITE_OPEN );
        break;
      case 'p': opt_para_delimiters = optarg;       break;
      case 's': tab_spaces  = check_atoi( optarg ); break;
      case 'v': goto version;
      case '?': goto usage;
    } /* switch */
  argc -= optind, argv += optind;
  if ( argc )
    goto usage;

  if ( !fin )
    fin = stdin;
  if ( !fout )
    fout = stdout;

  return;

usage:
  fprintf( stderr, "usage: %s [-ev] [-l line-length] [-p para-delim-chars] [-s tab-spaces]\n", me );
  fprintf( stderr, "\t[-f input-file] [-o output-file]\n" );
  exit( EXIT_USAGE );

version:
  fprintf( stderr, "%s %s\n", me, WRAP_VERSION );
  exit( EXIT_OK );
}

static char const* str_status( int status ) {
  switch ( status ) {
    case EXIT_OK          : return "OK";
    case EXIT_USAGE       : return "usage error";
    case EXIT_READ_OPEN   : return "error opening file for reading";
    case EXIT_READ_ERROR  : return "read error";
    case EXIT_WRITE_OPEN  : return "error opening file for writing";
    case EXIT_WRITE_ERROR : return "write error";
    case EXIT_FORK_ERROR  : return "fork() failed";
    case EXIT_EXEC_ERROR  : return "exec() failed";
    case EXIT_CHILD_SIGNAL: return "child process terminated by signal";
    case EXIT_PIPE_ERROR  : return "pipe() failed";
    default               : return "unknown status";
  }
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
