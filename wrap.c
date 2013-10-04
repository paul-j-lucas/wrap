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
**      -n    Do not treat 2 or more consecutive newlines as paragraph
**            delimiters.
**      -N    Treat every newline as a paragraph delimiter.  (This option
**            subsumes -bhHiIn.)
**
**      -v    Print version to stderr and exit.
*/

/* system */
#include <ctype.h>
#include <limits.h>                     /* for INT_MAX */
#include <stdio.h>
#include <stdlib.h>                     /* for atoi(), exit(), ... */
#include <string.h>
#include <unistd.h>                     /* for getopt(3) */

/* local */
#include "common.h"

/* global variable definitions */
char  buf[ 1024 ];                      /* hopefully, no one will exceed this */
bool  lead_white_delimit = false;       /* leading whitespace delimit para's? */
int   line_length = 80;                 /* wrap text to this line length */
int   lead_spaces = 0, lead_tabs = 0;   /* leading spaces and/or tabs */
int   indt_spaces = 0, indt_tabs = 0;   /* indent spaces and/or tabs */
int   hang_spaces = 0, hang_tabs = 0;   /* hanging indent spaces and/or tabs */
int   newlines_delimit = 2;             /* newlines that delimit a paragraph */
int   tab_spaces = 8;                   /* number of spaces a tab equals */

/* local functions */
void  print_line( int up_to );
void  process_options( int argc, char *argv[] );

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
  ** valid only if lead_white_delimit is true.
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
  ** first line of a paragraph and after 'newlines_delimit' or more consecutive
  ** newlines for subsequent paragraphs.
  */
  bool indent             = true;

  process_options( argc, argv );

  while ( (c = getchar()) != EOF ) {

    /*************************************************************************
     *  HANDLE NEWLINE(s)
     *************************************************************************/

    if ( c == '\n' ) {
      newline_counter = 1;
      if ( ++consec_newlines >= newlines_delimit ) {
        /*
        ** At least 'newlines_delimit' consecutive newlines: print text, if
        ** any; also set up for indenting.
        */
delimit_paragraph:
        if ( buf_count ) {
          print_line( buf_count );
          buf_count = buf_length = 0;
        }
        if ( consec_newlines == 2 || 
            (consec_newlines > 2 && newlines_delimit == 1) )
          putchar( c );
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
      if ( lead_white_delimit && newline_counter == 0 )
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
    if ( !( was_eos_char && strchr( "'\")]", c ) ) )
      was_eos_char = ( strchr( ".?!", c ) != 0 );

    /*************************************************************************
     *  INSERT SPACES
     *************************************************************************/

    if ( put_spaces ) {
      if ( buf_count ) {
        /*
        ** Mark position at a space to perform a wrap if necessary.
        */
        wrap_pos = buf_count;
        do {
          buf[ buf_count++ ] = ' ';
          ++buf_length;
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
      for ( i = 0; i < indt_tabs; ++i )
        buf[ buf_count++ ] = '\t';
      for ( i = 0; i < indt_spaces; ++i )
        buf[ buf_count++ ] = ' ';
      buf_length += indt_tabs * tab_spaces + indt_spaces;
      indent = false;
    }

    /*************************************************************************
     *  INSERT NON-SPACE CHARACTER
     *************************************************************************/

    buf[ buf_count++ ] = c;
    if ( ++buf_length < line_length ) {
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
    ** Prepare to slide the partial word to the left we can pick up from where
    ** we left off the next time around.
    */
    to = 0;

    /*
    ** Perform hang indentation first.
    */
    for ( i = 0; i < hang_tabs; ++i )
      buf[ to++ ] = '\t';
    for ( i = 0; i < hang_spaces; ++i )
      buf[ to++ ] = ' ';
    buf_length = hang_tabs * tab_spaces + hang_spaces;

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

  if ( buf_count )                      /* print left-over text */
    print_line( buf_count );
  exit( 0 );
}

/*****************************************************************************/

void print_line( int up_to ) {
  int i;
  for ( i = 0; i < lead_tabs; ++i )
    putchar( '\t' );
  for ( i = 0; i < lead_spaces; ++i )
    putchar( ' ' );
  buf[ up_to ] = '\0';
  printf( "%s\n", buf );
}

void process_options( int argc, char *argv[] ) {
  char const *me;                       /* executable name */
  int mirror_tabs = 0, mirror_spaces = 0;
  extern char *optarg;
  extern int optind, opterr;
  int opt;                              /* command-line option */

  me = strrchr( argv[0], '/' );         /* determine base name... */
  me = me ? me + 1 : argv[0];           /* ...of executable */

  opterr = 1;
  while ( (opt = getopt( argc, argv, "bh:H:i:I:l:m:M:nNs:S:t:v" )) != EOF )
    switch ( opt ) {
      case 'b': lead_white_delimit = true;           break;
      case 'h': hang_tabs          = atoi( optarg ); break;
      case 'H': hang_spaces        = atoi( optarg ); break;
      case 'i': indt_tabs          = atoi( optarg ); break;
      case 'I': indt_spaces        = atoi( optarg ); break;
      case 'l': line_length        = atoi( optarg ); break;
      case 'm': mirror_tabs        = atoi( optarg ); break;
      case 'M': mirror_spaces      = atoi( optarg ); break;
      case 'n': newlines_delimit   = INT_MAX;        break;
      case 'N': newlines_delimit   = 1;              break;
      case 's': tab_spaces         = atoi( optarg ); break;
      case 'S': lead_spaces        = atoi( optarg ); break;
      case 't': lead_tabs          = atoi( optarg ); break;
      case 'v': goto version;
      case '?': goto usage;
    }
  argc -= optind, argv += optind;
  if ( argc )
    goto usage;

  line_length -=
    2 * (mirror_tabs * tab_spaces + mirror_spaces) +
    lead_tabs * tab_spaces + lead_spaces;

  lead_tabs   += mirror_tabs;
  lead_spaces += mirror_spaces;

  return;

usage:
  fprintf( stderr, "usage: %s [-bnNv] [-l line-length] [-s tab-spaces]\n", me );
  fprintf( stderr, "\t[-t leading-tabs] [-S leading-spaces]\n" );
  fprintf( stderr, "\t[-m mirror-tabs]  [-M mirror-spaces]\n" );
  fprintf( stderr, "\t[-i indent-tabs]  [-I indent-spaces]\n" );
  fprintf( stderr, "\t[-h hanging-tabs] [-H hanging-spaces]\n" );
  exit( 1 );

version:
  fprintf( stderr, "%s %s\n", me, WRAP_VERSION );
  exit( 0 );
}

/*****************************************************************************/

/* vim:set et sw=2 ts=2: */
