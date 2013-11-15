/*
**      wrapc -- comment reformatter
**      wrapc.c
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
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* system */
#include <errno.h>                      /* for errno */
#include <limits.h>                     /* for PATH_MAX */
#include <signal.h>                     /* for kill() */
#include <stdio.h>
#include <stdlib.h>                     /* for exit() */
#include <string.h>                     /* for str...() */
#include <sys/wait.h>                   /* for wait() */
#include <unistd.h>                     /* for close(), fork(), ... */

/* local */
#include "common.h"
#include "getopt.h"

#define ARG_BUF_SIZE  22                /* max wrap command-line arg size */

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
int         line_width = DEFAULT_LINE_WIDTH;
char const* me;                         /* executable name */
int         tab_spaces = DEFAULT_TAB_SPACES;

/* option definitions */
char const* opt_alias = NULL;
char const* opt_conf_file = NULL;
bool        opt_eos_delimit = false;    /* end-of-sentence delimits para's? */
char const* opt_fin_name = NULL;        /* file in name */
bool        opt_no_conf = false;        /* do not read conf file */
char const* opt_para_delimiters = NULL; /* additional para delimiter chars */
bool        opt_title_line = false;     /* 1st para line is title? */

/* local functions */
static void process_options( int argc, char const *argv[] );
static char const* str_status( int status );
static void usage();

/*****************************************************************************/

int main( int argc, char const *argv[] ) {
  char  buf[ LINE_BUF_SIZE ];
  FILE* from_wrap;                      /* file used by parent */
  char  leader[ LINE_BUF_SIZE ];        /* string segment removed/prepended */
  int   lead_width;                     /* number of leading characters */
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
  leader[ lead_width = strspn( buf, leading_chars ) ] = '\0';

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

    if ( fputs( buf + lead_width, to_wrap ) == EOF )
      ERROR( EXIT_WRITE_ERROR );
    while ( fgets( buf, LINE_BUF_SIZE, fin ) ) {
      fputs( buf + lead_width, to_wrap );
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
  ** Compute the actual width of the leader: tabs are equal to <tab_spaces>
  ** spaces minus the number of spaces we're into a tab-stop, and all others
  ** are equal to 1.  Subtract this from <line_width> to obtain the wrap width.
  **
  ** Read from pipes[0] (child 1) and write to pipes[1] (parent); exec into
  ** wrap.
  */
  if ( (pid = fork()) == -1 ) {
    kill( pid_1, SIGTERM );             /* we failed, so kill child 1 */
    ERROR( EXIT_FORK_ERROR );
  }
  if ( !pid ) {
    arg_buf arg_line_width;
    arg_buf arg_opt_alias;
    arg_buf arg_opt_conf_file;
    char    arg_opt_fin_name[ PATH_MAX ];
    arg_buf arg_opt_para_delimiters;
    arg_buf arg_tab_spaces;

    int argc = 0;
    char *argv[11];                     /* must be +1 of greatest arg below */
    char const *c;
    int spaces = 0;

    for ( c = leader; *c; ++c ) {
      if ( *c == '\t' ) {
        line_width -= tab_spaces - spaces;
        spaces = 0;
      } else {
        --line_width;
        spaces = (spaces + 1) % tab_spaces;
      }
    } /* for */

#define ARG_SPRINTF(ARG,FMT) \
  snprintf( arg_##ARG, sizeof arg_##ARG, (FMT), (ARG) )

#define ARG_DUP(FMT)        argv[ argc++ ] = strdup( FMT )
#define IF_ARG_DUP(ARG,FMT) if ( ARG ) ARG_DUP( FMT )

#define ARG_FMT(ARG,FMT) \
  do { ARG_SPRINTF( ARG, (FMT) ); argv[ argc++ ] = arg_##ARG; } while (0)

#define IF_ARG_FMT(ARG,FMT) if ( ARG ) ARG_FMT( ARG, FMT )

    /* Quoting string arguments is unnecessary since no shell is involved. */

    /*  0 */    ARG_DUP(                      PACKAGE );
    /*  1 */ IF_ARG_FMT( opt_alias          , "-a%s"  );
    /*  2 */ IF_ARG_FMT( opt_conf_file      , "-c%s"  );
    /*  3 */ IF_ARG_DUP( opt_no_conf        , "-C"    );
    /*  4 */ IF_ARG_DUP( opt_eos_delimit    , "-e"    );
    /*  5 */ IF_ARG_FMT( opt_fin_name       , "-F%s"  );
    /*  6 */ IF_ARG_FMT( opt_para_delimiters, "-p%s"  );
    /*  7 */    ARG_FMT( tab_spaces         , "-s%d"  );
    /*  8 */ IF_ARG_DUP( opt_title_line     , "-T"    );
    /*  9 */    ARG_FMT( line_width         , "-w%d"  );
    /* 10 */ argv[ argc ] = NULL;

    REDIRECT( STDIN_FILENO, 0 );
    REDIRECT( STDOUT_FILENO, 1 );
    execvp( PACKAGE, argv );
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

static void process_options( int argc, char const *argv[] ) {
  int opt;                              /* command-line option */
  char const* opt_fin = NULL;           /* file in name */
  char const* opt_fout = NULL;          /* file out name */
  char const opts[] = "a:c:Cef:F:l:o:p:s:Tvw:";

  me = base_name( argv[0] );

  opterr = 1;
  while ( (opt = pjl_getopt( argc, argv, opts )) != EOF ) {
    switch ( opt ) {
      case 'a': opt_alias           = optarg;               break;
      case 'c': opt_conf_file       = optarg;               break;
      case 'C': opt_no_conf         = true;                 break;
      case 'e': opt_eos_delimit     = true;                 break;
      case 'f': opt_fin             = optarg;         /* no break; */
      case 'F': opt_fin_name        = base_name( optarg );  break;
      case 'o': opt_fout            = optarg;               break;
      case 'p': opt_para_delimiters = optarg;               break;
      case 's': tab_spaces          = check_atou( optarg ); break;
      case 'T': opt_title_line      = true;                 break;
      case 'v': fprintf( stderr, "%s\n", PACKAGE_STRING );  exit( EXIT_OK );
      case 'l': /* deprecated: now synonym for -w */
      case 'w': line_width          = check_atou( optarg ); break;
      default : usage();
    } /* switch */
  } /* while */
  argc -= optind, argv += optind;
  if ( argc )
    usage();

  if ( opt_fin && !(fin = fopen( opt_fin, "r" )) ) {
    fprintf( stderr, "%s: \"%s\": %s\n", me, optarg, strerror( errno ) );
    exit( EXIT_READ_OPEN );
  }
  if ( opt_fout && !(fout = fopen( optarg, "w" )) ) {
    fprintf( stderr, "%s: \"%s\": %s\n", me, optarg, strerror( errno ) );
    exit( EXIT_WRITE_OPEN );
  }

  if ( !fin )
    fin = stdin;
  if ( !fout )
    fout = stdout;
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

static void usage() {
  fprintf( stderr, "usage: %s [-a alias] [-CeTv] [-w line-width]\n", me );
  fprintf( stderr, "\t[-{fF} input-file] [-o output-file] [-c conf-file]\n" );
  fprintf( stderr, "\t[-p para-delim-chars] [-s tab-spaces]\n" );
  exit( EXIT_USAGE );
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
