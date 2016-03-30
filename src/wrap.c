/*
**      wrap -- text reformatter
**      wrap.c
**
**      Copyright (C) 1996-2016  Paul J. Lucas
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
#include "config.h"
#include "alias.h"
#include "common.h"
#include "options.h"
#include "pattern.h"
#include "read_conf.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>                     /* for INT_MAX */
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), ... */
#include <string.h>
#include <unistd.h>

// extern variable definitions
FILE               *fin;                // file in
FILE               *fout;               // file out
char const         *me;                 // executable name

// local constant definitions
static char const   UTF8_BOM[] = "\xEF\xBB\xBF";
static size_t const UTF8_BOM_LEN = sizeof( UTF8_BOM ) - 1; // terminating null
static char const   UTF8_LEN_TABLE[] = {
  /*      0 1 2 3 4 5 6 7 8 9 A B C D E F */
  /* 0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 1 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 2 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 3 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 4 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 5 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 6 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 7 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  /* 8 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  // continuation bytes
  /* 9 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  //        |
  /* A */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  //        |
  /* B */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  //        |
  /* C */ 0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  // C0 & C1 are overlong ASCII
  /* D */ 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  /* E */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
  /* F */ 4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0
};

// option local variable definitions
static char const  *opt_alias;
static char const  *opt_conf_file;
static bool         opt_eos_delimit;    // end-of-sentence delimits para's?
static bool         opt_data_link_esc;  // respond to in-band control
static char const  *opt_fin;            // file in path
static char const  *opt_fin_name;       // file in name (only)
static char const  *opt_fout = NULL;    // file out path
static size_t       opt_hang_spaces;    // hanging-indent spaces
static size_t       opt_hang_tabs;      // hanging-indent tabs
static size_t       opt_indt_spaces;    // indent spaces
static size_t       opt_indt_tabs;      // indent tabs
static bool         opt_lead_dot_ignore;// ignore lines starting with '.'?
static char const  *opt_lead_para_delims;
static size_t       opt_lead_spaces;    // leading spaces
static size_t       opt_lead_tabs;      // leading tabs
static bool         opt_lead_ws_delimit;// leading whitespace delimit para's?
static size_t       opt_line_width = LINE_WIDTH_DEFAULT;
static size_t       opt_mirror_spaces;
static size_t       opt_mirror_tabs;
static size_t       opt_newlines_delimit = NEWLINES_DELIMIT_DEFAULT;
static bool         opt_no_conf;        // do not read conf file
static char const  *opt_para_delims;    // additional para delimiter chars
static bool         opt_prototype;      // first line whitespace is prototype?
static size_t       opt_tab_spaces = TAB_SPACES_DEFAULT;
static bool         opt_title_line;     // 1st para line is title?

// other local variable definitions
static char         line_buf[ LINE_BUF_SIZE ];
static size_t       line_len;           // number of characters in buffer
static size_t       line_width;         // actual width of buffer
static char         proto_buf[ LINE_BUF_SIZE ];
static size_t       proto_len;
static size_t       proto_width;

// local functions
static bool copy_line( FILE*, FILE*, char*, size_t );
static void clean_up( void );
static void hang_indent( void );
static void init( int, char const*[] );
static void print_lead_chars( void );
static void print_line( size_t, bool );
static void process_options( int, char const*[], char const*, unsigned );
static void usage( void );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Gets the number of bytes comprising the UTF-8 encoding of a Unicode
 * codepoint.
 *
 * @param c The first byte of the UTF-8 encoded codepoint.
 * @return Returns 1-6, or 0 if \a c is invalid.
 */
static inline size_t utf8_len( char c ) {
  return (size_t)UTF8_LEN_TABLE[ (unsigned char)c ];
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *argv[] ) {
  init( argc, argv );

  //
  // Number of consecutive newlines.
  //
  unsigned consec_newlines = 0;

  //
  // True only when we should do hang-indenting after a title line.
  //
  bool do_hang_indent = false;

  //
  // True only when opt_lead_dot_ignore = true, the current character is a '.',
  // and the previous character was a '\n' (i.e., the line starts with a '.').
  //
  bool do_ignore_lead_dot = false;

  //
  // True only when we should do indenting: set initially to indent the first
  // line of a paragraph and after opt_newlines_delimit or more consecutive
  // newlines for subsequent paragraphs.
  //
  bool do_indent = true;

  //
  // True only when opt_lead_para_delims was set, the current character is
  // among them, and the previous character was a '\n' (i.e., the line starts
  // with a one of opt_lead_para_delims).
  //
  bool do_lead_para_delim = false;

  //
  // True only when we're handling a "long line" (a sequence of non-whitespace
  // characters longer than opt_line_width) that can't be wrapped.
  //
  bool is_long_line = false;

  //
  // True when the next line to be read will be a title line.  The first
  // non-all-whitespace line of the first paragraph is a title line only if
  // opt_title_line (-T) is true.
  //
  bool next_line_is_title = opt_title_line;

  //
  // Number of spaces to output before the next non-whitespace character: set
  // to 1 when we encounter a whitespace character and 2 when was_eos_char is
  // true so as to put 2 characters after the end of a sentence.
  //
  unsigned put_spaces = 0;

  //
  // True only if the previous character was an end-of-sentence character.
  //
  bool was_eos_char = false;

  //
  // True only if the previous character was a paragraph-delimiter character.
  //
  bool was_para_delim_char = false;

  /////////////////////////////////////////////////////////////////////////////

  size_t wrap_pos = 0;                  // position at which we can wrap

  for ( int c, prev_c = '\0'; (c = getc( fin )) != EOF; prev_c = c ) {

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE NEWLINE(s)
    ///////////////////////////////////////////////////////////////////////////

    if ( c == '\n' ) {
      opt_prototype = false;            // the prototype is complete

      if ( ++consec_newlines >= opt_newlines_delimit ) {
        //
        // At least opt_newlines_delimit consecutive newlines: set that the
        // next line is a title line and delimit the paragraph.
        //
        next_line_is_title = opt_title_line;
        goto delimit_paragraph;
      }
      if ( opt_eos_delimit && was_eos_char ) {
        //
        // End-of-sentence characters delimit paragraphs and the previous
        // character was an end-of-sentence character: delimit the paragraph.
        //
        goto delimit_paragraph;
      }
      if ( next_line_is_title && line_len ) {
        //
        // The first line of the next paragraph is title line and the buffer
        // isn't empty (there is a title): print the title.
        //
        print_lead_chars();
        print_line( line_len, true );
        do_hang_indent = true;
        next_line_is_title = false;
        continue;
      }
      if ( was_eos_char ) {
        //
        // We are joining a line after the end of a sentence: force 2 spaces.
        //
        put_spaces = 2;
        continue;
      }
    } else {
      consec_newlines = 0;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE WHITESPACE
    ///////////////////////////////////////////////////////////////////////////

    if ( isspace( c ) ) {
      if ( opt_prototype ) {
        if ( proto_len < sizeof( proto_buf ) - 1 ) {
          static size_t tab_pos;
          proto_buf[ proto_len++ ] = c;
          proto_width += c == '\t' ?
            (opt_tab_spaces - tab_pos % opt_tab_spaces) : 1;
          ++tab_pos;
          opt_line_width = LINE_WIDTH_DEFAULT - proto_width;
        }
        continue;
      }
      if ( is_long_line ) {
        //
        // We've been handling a "long line" and finally got a whitespace
        // character at which we can finally wrap: delimit the paragraph.
        //
        goto delimit_paragraph;
      }
      if ( opt_lead_ws_delimit && prev_c == '\n' ) {
        //
        // Leading whitespace characters delimit paragraphs and the previous
        // character was a newline which means this whitespace character is at
        // the beginning of a line: delimit the paragraph.
        //
        goto delimit_paragraph;
      }
      if ( opt_eos_delimit && was_eos_char ) {
        //
        // End-of-sentence characters delimit paragraphs and the previous
        // character was an end-of-sentence character: delimit the paragraph.
        //
        goto delimit_paragraph;
      }
      if ( was_para_delim_char ) {
        //
        // The previous character was a paragraph-delimiter character (set only
        // if opt_para_delims was set): delimit the paragraph.
        //
        goto delimit_paragraph;
      }
      if ( line_len && put_spaces < 1u + was_eos_char ) {
        //
        // We are not at the beginning of a line: remember to insert 1 space
        // later and allow 2 after the end of a sentence.
        //
        ++put_spaces;
      }
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE WRAP/WRAPC INTERPROCESS MESSAGE
    ///////////////////////////////////////////////////////////////////////////

    if ( opt_data_link_esc && c == ASCII_DLE ) {
      switch ( c = getc( fin ) ) {
        case ASCII_ETB:
          //
          // We've been told by wrapc (child 1) that we've reached the end of
          // the comment: dump any remaining buffer, propagate the interprocess
          // message to the other wrapc process (parent), and pass text through
          // verbatim.
          //
          print_line( line_len, true );
          FPRINTF( fout, "%c%c", ASCII_DLE, ASCII_ETB );
          fcopy( fin, fout );
          exit( EX_OK );
        case EOF:
          goto done;
      } // switch
    }

    ///////////////////////////////////////////////////////////////////////////
    //  DISCARD CONTROL CHARACTERS
    ///////////////////////////////////////////////////////////////////////////

    if ( iscntrl( c ) )
      continue;

    ///////////////////////////////////////////////////////////////////////////
    //  NON-WHITESPACE CHARACTERS
    ///////////////////////////////////////////////////////////////////////////

    opt_prototype = false;              // the prototype is complete

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE LEADING-PARAGRAPH-DELIMITERS, LEADING-DOT, END-OF-SENTENCE, AND
    //  PARAGRAPH-DELIMITERS
    ///////////////////////////////////////////////////////////////////////////

    if ( prev_c == '\n' ) {
      if ( opt_lead_para_delims && strchr( opt_lead_para_delims, c ) ) {
        do_lead_para_delim = true;
        goto delimit_paragraph;
      }
      if ( opt_lead_dot_ignore && c == '.' ) {
        do_ignore_lead_dot = true;
        goto delimit_paragraph;
      }
    }

    //
    // Treat a quote or a closing parenthesis or bracket as an end-of-sentence
    // character if it was preceded by a regular end-of-sentence character.
    //
    if ( !(was_eos_char && strchr( "'\")]", c )) )
      was_eos_char = (strchr( ".?!", c ) != NULL);

    if ( opt_para_delims )
      was_para_delim_char = (strchr( opt_para_delims, c ) != NULL);

    ///////////////////////////////////////////////////////////////////////////
    //  INSERT SPACES
    ///////////////////////////////////////////////////////////////////////////

    if ( put_spaces ) {
      if ( line_len ) {
        //
        // Mark position at a space to perform a wrap if necessary.
        //
        wrap_pos = line_len;
        line_width += put_spaces;
        do {
          line_buf[ line_len++ ] = ' ';
        } while ( --put_spaces );
      } else {
        //
        // Never put spaces at the beginning of a line.
        //
        put_spaces = 0;
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    //  PERFORM INDENTATION
    ///////////////////////////////////////////////////////////////////////////

insert:
    if ( do_indent ) {
      line_len = 0;
      for ( size_t i = 0; i < opt_indt_tabs; ++i )
        line_buf[ line_len++ ] = '\t';
      for ( size_t i = 0; i < opt_indt_spaces; ++i )
        line_buf[ line_len++ ] = ' ';
      line_width = opt_indt_tabs * opt_tab_spaces + opt_indt_spaces;
      do_indent = false;
    }

    if ( do_hang_indent ) {
      hang_indent();
      do_hang_indent = false;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  INSERT NON-SPACE CHARACTER
    ///////////////////////////////////////////////////////////////////////////

    size_t len = utf8_len( c );       // bytes comprising UTF-8 character
    if ( !len )                       // unexpected UTF-8 continuation byte
      continue;

    size_t tmp_line_len = line_len;   // save count in case invalid UTF-8
    line_buf[ tmp_line_len++ ] = c;

    //
    // If we've just read the start byte of a multi-byte UTF-8 character, read
    // the remaining bytes comprising the character.  The entire muti-byte
    // UTF-8 character is always treated as having a width of 1.
    //
    for ( size_t i = len; i > 1; --i ) {
      if ( (c = getc( fin )) == EOF )
        goto done;                      // premature end of UTF-8 character
      line_buf[ tmp_line_len++ ] = c;
      if ( utf8_len( c ) )              // unexpected UTF-8 start byte
        goto continue_outer_loop;       // skip entire UTF-8 character
    } // for
    line_len = tmp_line_len;            // UTF-8 is valid

    if ( len == UTF8_BOM_LEN ) {
      size_t const utf8_start_pos = line_len - UTF8_BOM_LEN;
      if ( strncmp( line_buf + utf8_start_pos, UTF8_BOM, UTF8_BOM_LEN ) == 0 )
        continue;                       // discard UTF-8 BOM
    }

    if ( ++line_width < opt_line_width ) {
      //
      // We haven't exceeded the line width yet.
      //
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  EXCEEDED LINE WIDTH; PRINT LINE OUT
    ///////////////////////////////////////////////////////////////////////////

    if ( !wrap_pos ) {
      //
      // We've exceeded the line width, but haven't encountered a whitespace
      // character at which to wrap; therefore, we've got a "long line."
      //
      if ( !is_long_line )
        print_lead_chars();
      print_line( line_len, false );
      is_long_line = true;
      continue;
    }

    is_long_line = false;
    print_lead_chars();
    tmp_line_len = line_len;
    print_line( wrap_pos, true );
    hang_indent();

    //
    // Slide the partial word to the left where we can pick up from where we
    // left off the next time around.
    //
    for ( size_t from = wrap_pos + 1; from < tmp_line_len; ++from ) {
      c = line_buf[ from ];
      if ( !isspace( c ) ) {
        line_buf[ line_len++ ] = c;
        ++line_width;
        for ( len = utf8_len( c ); len > 1; --len )
          line_buf[ line_len++ ] = line_buf[ ++from ];
      }
    } // for

    wrap_pos = 0;

continue_outer_loop:
    continue;

    ///////////////////////////////////////////////////////////////////////////

delimit_paragraph:
    if ( line_len ) {
      //
      // Print what's in the buffer before delimiting the paragraph.  If we've
      // been handling a "long line," it's now finally ended; otherwise, print
      // the leading characters.
      //
      if ( is_long_line )
        is_long_line = false;
      else
        print_lead_chars();
      print_line( line_len, true );
    } else if ( is_long_line )
      FPUTC( '\n', fout );              // delimit the "long line"

    put_spaces = 0;
    was_eos_char = was_para_delim_char = false;

    if ( do_lead_para_delim ) {
      do_lead_para_delim = false;
      //
      // The line starts with a leading paragraph delimiter and
      // opt_lead_dot_ignore is true: write the delimiter now that we've
      // delimited the paragraph.
      //
      goto insert;
    }
    else if ( do_ignore_lead_dot ) {
      do_ignore_lead_dot = false;
      //
      // The line starts with a leading dot and opt_lead_dot_ignore is true:
      // read/write the line as-is.
      //
      FPUTC( c, fout );
      if ( !copy_line( fin, fout, line_buf, sizeof( line_buf ) ) )
        goto done;
    }
    else {
      if ( consec_newlines == 2 ||
          (consec_newlines > 2 && opt_newlines_delimit == 1) ) {
        FPUTC( c, fout );
      }
      do_indent = true;
    }
  } // for

done:
  CHECK_FERROR( fin );
  if ( line_len ) {                     // print left-over text
    if ( !is_long_line )
      print_lead_chars();
    print_line( line_len, true );
  }
  exit( EX_OK );
}

////////// local functions ////////////////////////////////////////////////////

static void clean_up( void ) {
  alias_cleanup();
  pattern_cleanup();
}

static bool copy_line( FILE *fin, FILE *fout, char *buf, size_t buf_size ) {
  assert( fin );
  assert( fout );
  assert( buf );

  char *const last = buf + buf_size - 1;
  do {
    *last = '\0';
    if ( !fgets( buf, buf_size, fin ) ) {
      CHECK_FERROR( fin );
      return false;
    }
    FPUTS( buf, fout );
  } while ( *last && *last != '\n' );
  return true;
}

static void hang_indent( void ) {
  for ( size_t i = 0; i < opt_hang_tabs; ++i )
    line_buf[ line_len++ ] = '\t';
  for ( size_t i = 0; i < opt_hang_spaces; ++i )
    line_buf[ line_len++ ] = ' ';
  line_width += opt_hang_tabs * opt_tab_spaces + opt_hang_spaces;
}

static void init( int argc, char const *argv[] ) {
  me = base_name( argv[0] );
  atexit( clean_up );

  char const opts[] = "a:b:c:CdDef:F:h:H:i:I:m:M:nNo:p:Ps:S:t:Tvw:W";
  process_options( argc, argv, opts, 0 );
  argc -= optind, argv += optind;
  if ( argc )
    usage();

  if ( !opt_no_conf && (opt_alias || opt_fin_name ) ) {
    alias_t const *alias = NULL;
    opt_conf_file = read_conf( opt_conf_file );
    if ( opt_alias ) {
      if ( !(alias = alias_find( opt_alias )) )
        PMESSAGE_EXIT( EX_USAGE,
          "\"%s\": no such alias in %s\n",
          opt_alias, opt_conf_file
        );
    }
    else if ( opt_fin_name )
      alias = pattern_find( opt_fin_name );
    if ( alias )
      process_options( alias->argc, alias->argv, opts, alias->line_no );
  }

  if ( opt_fin && !(fin = fopen( opt_fin, "r" )) )
    PMESSAGE_EXIT( EX_NOINPUT, "\"%s\": %s\n", optarg, ERROR_STR );
  if ( opt_fout && !(fout = fopen( opt_fout, "w" )) )
    PMESSAGE_EXIT( EX_CANTCREAT, "\"%s\": %s\n", optarg, ERROR_STR );

  if ( !fin )
    fin = stdin;
  if ( !fout )
    fout = stdout;

  int const line_width = (int)opt_line_width -
    (int)(2 * (opt_mirror_tabs * opt_tab_spaces + opt_mirror_spaces) +
          opt_lead_tabs * opt_tab_spaces + opt_lead_spaces);

  if ( line_width < LINE_WIDTH_MINIMUM )
    PMESSAGE_EXIT( EX_USAGE,
      "line-width (%d) is too small (<%d)\n",
      line_width, LINE_WIDTH_MINIMUM
    );
  opt_line_width = line_width;

  opt_lead_tabs   += opt_mirror_tabs;
  opt_lead_spaces += opt_mirror_spaces;
}

static void print_lead_chars( void ) {
  if ( proto_buf[0] ) {
    FPUTS( proto_buf, fout );
  } else {
    for ( size_t i = 0; i < opt_lead_tabs; ++i )
      FPUTC( '\t', fout );
    for ( size_t i = 0; i < opt_lead_spaces; ++i )
      FPUTC( ' ', fout );
  }
}

static void print_line( size_t len, bool newline ) {
  line_buf[ len ] = '\0';
  if ( len )
    FPRINTF( fout, newline ? "%s\n" : "%s", line_buf );
  line_len = line_width = 0;
}

static void process_options( int argc, char const *argv[], char const *opts,
                             unsigned line_no ) {
  assert( opts );

  optind = opterr = 1;
  bool print_version = false;
  CLEAR_OPTIONS();

  for ( int opt; (opt = getopt( argc, (char**)argv, opts )) != EOF; ) {
    SET_OPTION( opt );
    if ( line_no && strchr( "acCDfFov", opt ) )
      PMESSAGE_EXIT( EX_CONFIG,
        "%s:%u: '%c': option not allowed in configuration file\n",
        opt_conf_file, line_no, opt
      );
    switch ( opt ) {
      case 'a': opt_alias             = optarg;                     break;
      case 'c': opt_conf_file         = optarg;                     break;
      case 'C': opt_no_conf           = true;                       break;
      case 'd': opt_lead_dot_ignore   = true;                       break;
      case 'b': opt_lead_para_delims  = optarg;                     break;
      case 'D': opt_data_link_esc     = true;                       break;
      case 'e': opt_eos_delimit       = true;                       break;
      case 'f': opt_fin               = optarg;               // no break;
      case 'F': opt_fin_name          = base_name( optarg );        break;
      case 'h': opt_hang_tabs         = check_atou( optarg );       break;
      case 'H': opt_hang_spaces       = check_atou( optarg );       break;
      case 'i': opt_indt_tabs         = check_atou( optarg );       break;
      case 'I': opt_indt_spaces       = check_atou( optarg );       break;
      case 'm': opt_mirror_tabs       = check_atou( optarg );       break;
      case 'M': opt_mirror_spaces     = check_atou( optarg );       break;
      case 'n': opt_newlines_delimit  = INT_MAX;                    break;
      case 'N': opt_newlines_delimit  = 1;                          break;
      case 'o': opt_fout              = optarg;                     break;
      case 'p': opt_para_delims       = optarg;                     break;
      case 'P': opt_prototype         = true;                       break;
      case 's': opt_tab_spaces        = check_atou( optarg );       break;
      case 'S': opt_lead_spaces       = check_atou( optarg );       break;
      case 't': opt_lead_tabs         = check_atou( optarg );       break;
      case 'T': opt_title_line        = true;                       break;
      case 'v': print_version         = true;                       break;
      case 'w': opt_line_width        = check_atou( optarg );       break;
      case 'W': opt_lead_ws_delimit   = true;                       break;
      default : usage();
    } // switch
  } // for

  check_mutually_exclusive( "f", "F" );
  check_mutually_exclusive( "n", "N" );
  check_mutually_exclusive( "P", "hHiImMStW" );
  check_mutually_exclusive( "v", "abcCdDefFhHiIlmMnNopsStTwW" );

  if ( print_version ) {
    PRINT_ERR( "%s\n", PACKAGE_STRING );
    exit( EX_OK );
  }
}

static void usage( void ) {
  PRINT_ERR(
"usage: %s [options]\n"
"       %s -v\n"
"\n"
"options:\n"
"  -a alias   Use alias from configuration file.\n"
"  -b chars   Specify block leading delimiter characters.\n"
"  -c file    Specify the configuration file [default: ~/%s].\n"
"  -C         Suppress reading configuration file.\n"
"  -d         Does not alter lines that begin with '.' (dot).\n"
"  -e         Treat whitespace after end-of-sentence as new paragraph.\n"
"  -f file    Read from this file [default: stdin].\n"
"  -F string  Specify filename for stdin.\n"
"  -h number  Hang-indent tabs for all but first line of each paragraph.\n"
"  -H number  Hang-indent spaces for all but first line of each paragraph.\n"
"  -i number  Indent tabs for first line of each paragraph.\n"
"  -I number  Indent spaces for first line of each paragraph.\n"
"  -n         Does not treat newlines as paragraph delimeters.\n"
"  -N         Treat every newline as a paragraph delimiter.\n"
"  -o file    Write to this file [default: stdout].\n"
"  -p chars   Specify additional paragraph delimiter characters.\n"
"  -P         Treat leading whitespace on first line as prototype.\n"
"  -s number  Specify tab-spaces equivalence [default: %d].\n"
"  -S number  Prepend leading spaces after leading tabs to each line.\n"
"  -t number  Prepend leading tabs to each line.\n"
"  -T         Treat paragraph first line as title.\n"
"  -v         Print version an exit.\n"
"  -w number  Specify line width [default: %d]\n"
"  -W         Treat line beginning with whitespace as paragraph delimiter.\n"
    , me, me, CONF_FILE_NAME, TAB_SPACES_DEFAULT, LINE_WIDTH_DEFAULT
  );
  exit( EX_USAGE );
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
