/*
**      wrap -- text reformatter
**      wrap.c: implementation
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

/*
** Options:
**
**      -l n  Set <line_length> to n characters; default is 80.
**      -s n  Set <tab_spaces> to n; default is 8.
**
**      -t n  Put n leading tabs before each line.
**      -S n  Put n leading spaces before each line after any leading tabs.
**
**      -m n  Mirror tabs;   same as: -tn -l<line_length>-n*2*<tab_spaces>
**      -M n  Mirror spaces; same as: -Sn -l<line_length>-n*2
**
**      -i n  Indent n more tabs for the first line of a paragraph.
**      -I n  Indent n more spaces for the first line of a paragraph after any
**            tabs.
**
**      -h n  Hang-indent n more tabs for all but the first line of a
**            paragraph.
**      -H n  Hang-indent n more spaces for all but the first line of a
**            paragraph after any tabs.
**
**      -b    Treat a line beginning with white-space as a paragraph delimiter.
**      -e    Treat white-space after an end-of-sentence character as a
**            paragraph delimiter.
**      -f    Specify file to read from.
**      -n    Do not treat 2 or more consecutive newlines as paragraph
**            delimiters.
**      -N    Treat every newline as a paragraph delimiter.  (This option
**            subsumes -bhHiIn.)
**      -o    Specify file to write to.
**
**      -v    Print version to stderr and exit.
*/

/* system */
#include <ctype.h>
#include <limits.h>                     /* for INT_MAX */
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), ... */
#include <string.h>
#include <unistd.h>                     /* for getopt(3), STDIN_FILENO, ... */

/* local */
#include "common.h"

/* global variable definitions */
char        buf[ LINE_BUF_SIZE ];
FILE*       fin = NULL;                 /* file in */
FILE*       fout = NULL;                /* file out */
int         line_length = DEFAULT_LINE_LENGTH;
char const* me;                         /* executable name */
int         newlines_delimit = 2;       /* # newlines that delimit a para */
int         tab_spaces = DEFAULT_TAB_SPACES;

/* option definitions */
bool        opt_eos_delimit = false;    /* end-of-sentence delimits para's? */
int         opt_hang_spaces = 0;        /* hanging-indent spaces */
int         opt_hang_tabs = 0;          /* hanging-indent tabs */
int         opt_indt_spaces = 0;        /* indent spaces */
int         opt_indt_tabs = 0;          /* indent tabs */
int         opt_lead_spaces = 0;        /* leading spaces */
int         opt_lead_tabs = 0;          /* leading tabs */
bool        opt_lead_ws_delimit = false;/* leading whitespace delimit para's? */

/* local functions */
static void print_line( int up_to );
static void process_options( int argc, char *argv[] );

/****************************************************************************/

int main( int argc, char *argv[] ) {
  int c;                                /* current character */
  int buf_count = 0;                    /* number of characters in buffer */
  int buf_length = 0;                   /* actual length of buffer */
  int wrap_pos = 0;                     /* position at which we can wrap */
  int i, from, to;                      /* work variables */
  /*
  ** Fold all white-space (perhaps multiple characters) to a single space with
  ** the following exceptions:
  **
  ** 1. Force 2 spaces after the end of a sentence that ends a line; sentences
  **    are ended by an "end-of-sentence" character, that is, a period,
  **    question-mark, or an exclamation-point, optionally followed by a
  **    single-quote, double-quote, or a closing parenthesis or bracket.
  **
  ** 2. Allow 2 spaces after the end of a sentence that does not end a line.
  **    This distinction is made so as not to put 2 spaces after a period that
  **    is an abbreviation and not the end of a sentence; periods at the end of
  **    a line will hopefully not be abbreviations.
  **
  ** 3. If neither the -n or -N option is specified, 2 consecutive newline
  **    characters delimit a paragraph and are not folded; more than 2 are
  **    folded.  If -n is specified, newlines receive no special treatment;
  **    if -N is specified, every newline delimits a paragraph.
  **
  ** 4. If the -b option is specified, a line beginning with white-space
  **    delimits a paragraph.
  */

  /*
  ** True only if the previous character was an end-of-sentence character.
  */
  bool was_eos_char     = false;

  /*
  ** Number of consecutive newlines.
  */
  int  consec_newlines  = 0;

  /*
  ** Set to 1 when a newline is encountered; decremented otherwise.  Used and
  ** valid only if opt_lead_ws_delimit is true.
  */
  int  newline_counter  = 0;

  /*
  ** Number of spaces to output before the next non-white-space character: set
  ** to 1 when we encounter a white-space character and 2 when was_eos_char is
  ** true so as to put 2 characters after the end of a sentence.
  */
  int  put_spaces       = 0;

  /*
  ** Flag to signal when we should do indenting: set initially to indent the
  ** first line of a paragraph and after newlines_delimit or more consecutive
  ** newlines for subsequent paragraphs.
  */
  bool indent             = true;

  process_options( argc, argv );

  while ( (c = getc( fin )) != EOF ) {

    /*************************************************************************
     *  HANDLE NEWLINE(s)
     *************************************************************************/

    if ( c == '\n' ) {
      newline_counter = 1;
      if ( ++consec_newlines >= newlines_delimit ) {
        /*
        ** At least newlines_delimit consecutive newlines: print text, if any;
        ** also set up for indenting.
        */
delimit_paragraph:
        if ( buf_count ) {
          print_line( buf_count );
          buf_count = buf_length = 0;
        }
        if ( consec_newlines == 2 || 
            (consec_newlines > 2 && newlines_delimit == 1) ) {
          putc( c, fout );
          if ( ferror( fout ) )
            ERROR( EXIT_WRITE_ERROR );
        }
        was_eos_char = false;
        put_spaces = 0;
        indent = true;
        continue;
      }
      if ( was_eos_char ) {
        /*
        ** We are joining a line after the end of a sentence: force 2 spaces.
        */
        put_spaces = 2;
        continue;
      }
    } else {
      consec_newlines = 0;
      --newline_counter;
    }

    /*************************************************************************
     *  HANDLE WHITE-SPACE
     *************************************************************************/

    if ( isspace( c ) ) {
      /*
      ** If newline_counter == 0, the previous character was a newline;
      ** therefore, this white-space character is at the beginning of a line.
      */
      if ( opt_lead_ws_delimit && newline_counter == 0 )
        goto delimit_paragraph;
      /*
      ** If end-of-sentence characters delimit paragraphs and the previous
      ** character was an end-of-sentence character, delimit the paragraph.
      */
      if ( opt_eos_delimit && was_eos_char )
        goto delimit_paragraph;
      /*
      ** Only if we are not at the beginning of a line, remember to insert 1
      ** space later; allow 2 after the end of a sentence.
      */
      if ( buf_count && put_spaces < 1 + was_eos_char )
        ++put_spaces;
      continue;
    }

    /*************************************************************************
     *  DISCARD CONTROL CHARACTERS
     *************************************************************************/

    if ( iscntrl( c ) )
      continue;

    /*************************************************************************
     *  HANDLE EOS CHARACTERS
     *************************************************************************/

    /*
    ** Treat a quote or a closing parenthesis or bracket as an end-of-sentence
    ** character if it was preceded by a regular end-of-sentence character.
    */
    if ( !(was_eos_char && strchr( "'\")]", c )) )
      was_eos_char = (strchr( ".?!", c ) != NULL);

    /*************************************************************************
     *  INSERT SPACES
     *************************************************************************/

    if ( put_spaces ) {
      if ( buf_count ) {
        /*
        ** Mark position at a space to perform a wrap if necessary.
        */
        wrap_pos = buf_count;
        buf_length += put_spaces;
        do {
          buf[ buf_count++ ] = ' ';
        } while ( --put_spaces );
      } else {
        /*
        ** Never put spaces at the beginning of a line.
        */
        put_spaces = 0;
      }
    }

    /*************************************************************************
     *  PERFORM FIRST-LINE-OF-PARAGRAPH INDENTATION
     *************************************************************************/

    if ( indent ) {
      for ( i = 0; i < opt_indt_tabs; ++i )
        buf[ buf_count++ ] = '\t';
      for ( i = 0; i < opt_indt_spaces; ++i )
        buf[ buf_count++ ] = ' ';
      buf_length += opt_indt_tabs * tab_spaces + opt_indt_spaces;
      indent = false;
    }

    /*************************************************************************
     *  INSERT NON-SPACE CHARACTER
     *************************************************************************/

    buf[ buf_count++ ] = c, ++buf_length;
    if ( buf_length < line_length ) {
      /*
      ** We haven't exceeded the line length yet.
      */
      continue;
    }

    /*************************************************************************
     *  EXCEEDED LINE LENGTH; PRINT LINE OUT
     *************************************************************************/

    print_line( wrap_pos );

    /*
    ** Prepare to slide the partial word to the left where we can pick up from
    ** where we left off the next time around.
    */
    to = 0;

    /*
    ** Perform hang indentation first.
    */
    for ( i = 0; i < opt_hang_tabs; ++i )
      buf[ to++ ] = '\t';
    for ( i = 0; i < opt_hang_spaces; ++i )
      buf[ to++ ] = ' ';
    buf_length = opt_hang_tabs * tab_spaces + opt_hang_spaces;

    /*
    ** Now slide the partial word to the left.
    */
    for ( from = wrap_pos + 1; from < buf_count; ++from ) {
      buf[ to ] = buf[ from ];
      if ( !isspace( buf[ to ] ) )
        ++to, ++buf_length;
    }
    buf_count = to;
    wrap_pos = 0;

    /*************************************************************************/
  } /* while */
  if ( ferror( fin ) )
    ERROR( EXIT_READ_ERROR );

  if ( buf_count )                      /* print left-over text */
    print_line( buf_count );
  exit( EXIT_OK );
}

/*****************************************************************************/

static void print_line( int up_to ) {
  int i;
  for ( i = 0; i < opt_lead_tabs; ++i )
    putc( '\t', fout );
  for ( i = 0; i < opt_lead_spaces; ++i )
    putc( ' ', fout );
  buf[ up_to ] = '\0';
  fprintf( fout, "%s\n", buf );
  if ( ferror( fout ) )
    ERROR( EXIT_WRITE_ERROR );
}

static void process_options( int argc, char *argv[] ) {
  int opt_mirror_tabs = 0, opt_mirror_spaces = 0;
  extern char *optarg;
  extern int optind, opterr;
  int opt;                              /* command-line option */

  me = strrchr( argv[0], '/' );         /* determine base name... */
  me = me ? me + 1 : argv[0];           /* ...of executable */

  opterr = 1;
  while ( (opt = getopt( argc, argv, "bef:h:H:i:I:l:m:M:nNo:s:S:t:v" )) != EOF )
    switch ( opt ) {
      case 'b': opt_lead_ws_delimit = true;                 break;
      case 'e': opt_eos_delimit     = true;                 break;
      case 'f':
        if ( !(fin = fopen( optarg, "r" )) )
          ERROR( EXIT_READ_OPEN );
        break;
      case 'h': opt_hang_tabs       = check_atoi( optarg ); break;
      case 'H': opt_hang_spaces     = check_atoi( optarg ); break;
      case 'i': opt_indt_tabs       = check_atoi( optarg ); break;
      case 'I': opt_indt_spaces     = check_atoi( optarg ); break;
      case 'l': line_length         = check_atoi( optarg ); break;
      case 'm': opt_mirror_tabs     = check_atoi( optarg ); break;
      case 'M': opt_mirror_spaces   = check_atoi( optarg ); break;
      case 'n': newlines_delimit    = INT_MAX;              break;
      case 'N': newlines_delimit    = 1;                    break;
      case 'o':
        if ( !(fout = fopen( optarg, "w" )) )
          ERROR( EXIT_WRITE_OPEN );
        break;
      case 's': tab_spaces          = check_atoi( optarg ); break;
      case 'S': opt_lead_spaces     = check_atoi( optarg ); break;
      case 't': opt_lead_tabs       = check_atoi( optarg ); break;
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

  line_length -=
    2 * (opt_mirror_tabs * tab_spaces + opt_mirror_spaces) +
    opt_lead_tabs * tab_spaces + opt_lead_spaces;

  opt_lead_tabs   += opt_mirror_tabs;
  opt_lead_spaces += opt_mirror_spaces;

  return;

usage:
  fprintf( stderr, "usage: %s [-benNv] [-l line-length] [-s tab-spaces]\n", me );
  fprintf( stderr, "\t[-f input-file]   [-o output-file]\n" );
  fprintf( stderr, "\t[-t leading-tabs] [-S leading-spaces]\n" );
  fprintf( stderr, "\t[-m mirror-tabs]  [-M mirror-spaces]\n" );
  fprintf( stderr, "\t[-i indent-tabs]  [-I indent-spaces]\n" );
  fprintf( stderr, "\t[-h hanging-tabs] [-H hanging-spaces]\n" );
  exit( EXIT_USAGE );

version:
  fprintf( stderr, "%s %s\n", me, WRAP_VERSION );
  exit( EXIT_OK );
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
