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
#include "markdown.h"
#include "options.h"
#include "pattern.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), ... */
#include <string.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////

#define WIPC_DEFERF(BUF,SIZE,CODE,FORMAT,...) \
  snprintf( (BUF), (SIZE), ("%c" FORMAT), (CODE), __VA_ARGS__ )

/**
 * Line indentation type.
 */
enum indentation {
  INDENT_NONE,
  INDENT_LINE,
  INDENT_HANG
};
typedef enum indentation indent_t;

// extern variable definitions
char const         *me;                 // executable name

// local constant definitions
static char const   UTF8_BOM[] = "\xEF\xBB\xBF";
static size_t const UTF8_BOM_LEN = sizeof UTF8_BOM - 1; // terminating null
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

// local variable definitions
static unsigned     consec_newlines;    // number of consecutive newlines
static line_buf_t   in_buf;
static line_buf_t   ipc_buf;            // deferred IPC message
static size_t       ipc_line_width;     // deferred IPC line width
static size_t       line_width;         // max width of line
static line_buf_t   out_buf;
static size_t       out_len;            // number of characters in buffer
static size_t       out_width;          // actual width of buffer
static line_buf_t   proto_buf;
static line_buf_t   proto_tws;          // prototype trailing whitespace, if any
static size_t       proto_len;
static size_t       proto_width;

// local functions
static int          buf_getc( char const** );
static void         init( int, char const*[] );
static void         print_lead_chars( void );
static void         print_line( size_t, bool );
static void         put_tabs_spaces( size_t, size_t );
static size_t       read_line( line_buf_t );
static void         usage( void );
static void         wipc_send( FILE*, char* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Prints an end-of-line and sends any pending IPC message to wrapc.
 */
static inline void print_eol( void ) {
  FPUTS( eol(), fout );
  wipc_send( fout, ipc_buf );
}

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
  wait_for_debugger_attach( "WRAP_DEBUG" );
  init( argc, argv );

  //
  // True only when opt_lead_dot_ignore = true, the current character is a '.',
  // and the previous character was a '\n' (i.e., the line starts with a '.').
  //
  bool do_ignore_lead_dot = false;

  //
  // True only when opt_lead_para_delims was set, the current character is
  // among them, and the previous character was a '\n' (i.e., the line starts
  // with a one of opt_lead_para_delims).
  //
  bool do_lead_para_delim = false;

  //
  // The type of indent to do, if any.  Set initially to INDENT_LINE to
  // indent the first line of the first paragraph.
  //
  indent_t indent = INDENT_LINE;

  //
  // True only when we're handling a "long line" (a sequence of non-whitespace
  // characters longer than line_width) that can't be wrapped.
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
  // True only if the previous character was a newline.
  //
  bool was_newline = false;

  //
  // True only if the previous character was a paragraph-delimiter character.
  //
  bool was_para_delim_char = false;

  /////////////////////////////////////////////////////////////////////////////

  char const *pc = in_buf;              // pointer to current character
  size_t wrap_pos = 0;                  // position at which we can wrap

  for ( int c; (c = buf_getc( &pc )) != EOF; was_newline = (c == '\n') ) {

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE NEWLINE(s)
    ///////////////////////////////////////////////////////////////////////////

    if ( c == '\r' ) {
      //
      // The code is simpler if we always strip \r and add it back later (if
      // opt_eol is EOL_WINDOWS).
      //
      continue;
    }

    if ( c == '\n' ) {
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
      if ( next_line_is_title && out_len ) {
        //
        // The first line of the next paragraph is title line and the buffer
        // isn't empty (there is a title): print the title.
        //
        print_lead_chars();
        print_line( out_len, true );
        indent = INDENT_HANG;
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
      if ( is_long_line ) {
        //
        // We've been handling a "long line" and finally got a whitespace
        // character at which we can finally wrap: delimit the paragraph.
        //
        goto delimit_paragraph;
      }
      if ( opt_lead_ws_delimit && was_newline ) {
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
      if ( out_len && put_spaces < 1u + was_eos_char ) {
        //
        // We are not at the beginning of a line: remember to insert 1 space
        // later and allow 2 after the end of a sentence.
        //
        ++put_spaces;
      }
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  DISCARD CONTROL CHARACTERS
    ///////////////////////////////////////////////////////////////////////////

    if ( iscntrl( c ) )
      continue;

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE LEADING-PARAGRAPH-DELIMITERS, LEADING-DOT, END-OF-SENTENCE, AND
    //  PARAGRAPH-DELIMITERS
    ///////////////////////////////////////////////////////////////////////////

    if ( was_newline ) {
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
      if ( out_len ) {
        //
        // Mark position at a space to perform a wrap if necessary.
        //
        wrap_pos = out_len;
        out_width += put_spaces;
        do {
          out_buf[ out_len++ ] = ' ';
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
    switch ( indent ) {
      case INDENT_HANG:
        put_tabs_spaces( opt_hang_tabs, opt_hang_spaces );
        break;
      case INDENT_LINE:
        put_tabs_spaces( opt_indt_tabs, opt_indt_spaces );
        break;
      case INDENT_NONE:
        /* suppress warning */;
    } // switch
    indent = INDENT_NONE;

    ///////////////////////////////////////////////////////////////////////////
    //  INSERT NON-SPACE CHARACTER
    ///////////////////////////////////////////////////////////////////////////

    size_t len = utf8_len( c );         // bytes comprising UTF-8 character
    if ( !len )                         // unexpected UTF-8 continuation byte
      continue;

    size_t tmp_out_len = out_len;       // save count in case invalid UTF-8
    out_buf[ tmp_out_len++ ] = c;

    //
    // If we've just read the start byte of a multi-byte UTF-8 character, read
    // the remaining bytes comprising the character.  The entire muti-byte
    // UTF-8 character is always treated as having a width of 1.
    //
    for ( size_t i = len; i > 1; --i ) {
      if ( (c = buf_getc( &pc )) == EOF )
        goto done;                      // premature end of UTF-8 character
      out_buf[ tmp_out_len++ ] = c;
      if ( utf8_len( c ) )              // unexpected UTF-8 start byte
        goto continue_outer_loop;       // skip entire UTF-8 character
    } // for
    out_len = tmp_out_len;              // UTF-8 is valid

    if ( len == UTF8_BOM_LEN ) {
      size_t const utf8_start_pos = out_len - UTF8_BOM_LEN;
      if ( strncmp( out_buf + utf8_start_pos, UTF8_BOM, UTF8_BOM_LEN ) == 0 )
        continue;                       // discard UTF-8 BOM
    }

    if ( ++out_width < line_width ) {
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
      print_line( out_len, false );
      is_long_line = true;
      continue;
    }

    is_long_line = false;
    tmp_out_len = out_len;
    print_lead_chars();
    print_line( wrap_pos, true );
    put_tabs_spaces( opt_hang_tabs, opt_hang_spaces );

    //
    // Slide the partial word to the left where we can pick up from where we
    // left off the next time around.
    //
    for ( size_t from = wrap_pos + 1; from < tmp_out_len; ++from ) {
      c = out_buf[ from ];
      if ( !isspace( c ) ) {
        out_buf[ out_len++ ] = c;
        ++out_width;
        for ( len = utf8_len( c ); len > 1; --len )
          out_buf[ out_len++ ] = out_buf[ ++from ];
      }
    } // for

    wrap_pos = 0;

continue_outer_loop:
    continue;

    ///////////////////////////////////////////////////////////////////////////

delimit_paragraph:
    if ( out_len ) {
      //
      // Print what's in the buffer before delimiting the paragraph.  If we've
      // been handling a "long line," it's now finally ended; otherwise, print
      // the leading characters.
      //
      if ( is_long_line )
        is_long_line = false;
      else
        print_lead_chars();
      print_line( out_len, true );
    } else if ( is_long_line )
      print_eol();                      // delimit the "long line"

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
      FPUTS( in_buf, fout );
      (void)read_line( in_buf );
      pc = in_buf;
    }
    else {
      if ( consec_newlines == 2 ||
          (consec_newlines > 2 && opt_newlines_delimit == 1) ) {
        print_lead_chars();
        print_eol();
      }
      indent = opt_markdown ? INDENT_NONE : INDENT_LINE;
    }
  } // for

done:
  FERROR( fin );
  if ( out_len ) {                      // print left-over text
    if ( !is_long_line )
      print_lead_chars();
    print_line( out_len, true );
  }
  exit( EX_OK );
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the next character from the input.
 *
 * @param ps A pointer to the pointer to character to advance.
 * @return Returns said character or EOF.
 */
static int buf_getc( char const **ps ) {
  while ( !**ps ) {
read_line:
    if ( !read_line( in_buf ) )
      return EOF;
    *ps = in_buf;
    //
    // When wrapping Markdown, we have to strip leading whitespace from lines
    // since it interferes with indenting.
    //
    if ( !opt_markdown || *SKIP_CHARS( *ps, WS_STR ) )
      break;
  } // while

  int c = *(*ps)++;

  if ( opt_data_link_esc && c == WIPC_HELLO ) {
    switch ( c = *(*ps)++ ) {
      case WIPC_END_WRAP:
        //
        // We've been told by wrapc (child 1) that we've reached the end of
        // the comment: dump any remaining buffer, propagate the interprocess
        // message to the other wrapc process (parent), and pass text through
        // verbatim.
        //
        print_lead_chars();
        print_line( out_len, true );
        WIPC_SENDF( fout, WIPC_END_WRAP, "%s", *ps );
        fcopy( fin, fout );
        exit( EX_OK );

      case WIPC_NEW_LEADER: {
        //
        // We've been told by wrapc (child 1) that the comment characters
        // and/or leading whitespace has changed: we have to echo it back to
        // the other wrapc process (parent).
        //
        // If an output line has already been started, we have to defer the IPC
        // until just after the line is sent; otherwise, we must send it
        // immediately.
        //
        char *sep;
        size_t const new_line_width = strtoul( *ps, &sep, 10 );
        if ( out_len ) {
          WIPC_DEFERF(
            ipc_buf, sizeof ipc_buf, WIPC_NEW_LEADER, "%zu" WIPC_SEP "%s",
            new_line_width, sep + 1
          );
          ipc_line_width = new_line_width;
        } else {
          WIPC_SENDF(
            fout, WIPC_NEW_LEADER, "%zu" WIPC_SEP "%s",
            new_line_width, sep + 1
          );
          line_width = opt_line_width = new_line_width;
        }
        goto read_line;
      }

      case '\0':
        return EOF;
    } // switch
  }

  return c;
}

/**
 * Sets-up clean-up, parses command-line options, reads the conf. file, sets-up
 * I/O, and probes the input for end-of-line type.
 *
 * @param argc The number of command-line arguments from main().
 * @param argv The command-line arguments from main().
 */
static void init( int argc, char const *argv[] ) {
  atexit( clean_up );
  init_options( argc, argv, usage );

  int const out_width = (int)opt_line_width -
    (int)(2 * (opt_mirror_tabs * opt_tab_spaces + opt_mirror_spaces) +
          opt_lead_tabs * opt_tab_spaces + opt_lead_spaces);

  if ( out_width < LINE_WIDTH_MINIMUM )
    PMESSAGE_EXIT( EX_USAGE,
      "line-width (%d) is too small (<%d)\n",
      out_width, LINE_WIDTH_MINIMUM
    );
  opt_line_width = line_width = out_width;

  opt_lead_tabs   += opt_mirror_tabs;
  opt_lead_spaces += opt_mirror_spaces;

  if ( opt_markdown )
    opt_tab_spaces =  TAB_SPACES_MARKDOWN;

  size_t const bytes_read = read_line( in_buf );
  if ( !bytes_read )
    exit( EX_OK );

  //
  // Copy the prototype and calculate its width.
  //
  if ( opt_lead_string || opt_prototype ) {
    size_t pos = 0;
    for ( char const *s = opt_lead_string ? opt_lead_string : in_buf; *s;
          ++s, ++pos ) {
      if ( opt_prototype && !is_space( *s ) )
        break;
      if ( proto_len == sizeof proto_buf - 1 )
        break;
      proto_buf[ proto_len++ ] = *s;
      proto_width += *s == '\t' ? (opt_tab_spaces - pos % opt_tab_spaces) : 1;
    } // for
    line_width = opt_line_width - proto_width;
    if ( opt_lead_string ) {
      //
      // Split off the trailing whitespace (tws) from the prototype so that if
      // we read a line that's empty, we won't emit trailing whitespace when we
      // prepend the prototype. For example, given:
      //
      //      # foo
      //      #
      //      # bar
      //
      // and a prototype of "# ", if we didn't split off trailing whitespace,
      // then when we wrapped the text above, the second line would become "# "
      // containing a trailing whitespace.
      //
      split_tws( proto_buf, proto_len, proto_tws );
    }
  }

  if ( opt_eol == EOL_INPUT ) {
    //
    // We're supposed to use the same end-of-lines as the input, but we can't
    // just wait until we read a \r as part of the normal character-at-a-time
    // input stream to know it's using Windows end-of-lines because if the
    // first line is a long line, we'll need to wrap it (by emitting a newline)
    // before we get to the end of the line and read the \r.
    //
    // Therefore, we have to read only the first line in its entirety and peek
    // ahead to see if it ends with \r\n.
    //
    opt_eol = bytes_read >= 2 && in_buf[ bytes_read - 2 ] == '\r' ?
      EOL_WINDOWS : EOL_UNIX;
  }
}

/**
 * Adjusts wrap's indent, hang-indent, and line-width for each Markdown line.
 *
 * @param line The line to parse.
 * @return Returns \c true if we're to proceed or \c false if another line
 * should be read.
 */
static bool markdown_adjust( line_buf_t line ) {
  static md_line_t  prev_line_type;
  static md_seq_t   prev_seq_num = MD_SEQ_NUM_INIT;

  md_state_t const *const md = markdown_parse( line );
  MD_DEBUG(
    "T=%c N=%2u D=%u L=%u H=%u|%s",
    (char)md->line_type, md->seq_num, md->depth,
    md->indent_left, md->indent_hang, line
  );

  if ( prev_line_type != md->line_type ) {
    switch ( prev_line_type ) {
      case MD_CODE:
      case MD_HEADER_ATX:
      case MD_HR:
      case MD_HTML_ABBR:
      case MD_HTML_BLOCK:
      case MD_LINK_LABEL:
      case MD_TABLE:
        consec_newlines = 0;
        if ( is_blank_line( line ) ) {
          //
          // Prevent blank lines immediately after these Markdown line types
          // from being swallowed by wrap by just printing them directly.
          //
          FPUTS( line, fout );
        }
        break;
      default:
        /* suppress warning */;
    } // switch

    switch ( md->line_type ) {
      case MD_FOOTNOTE_DEF:
        if ( !md->fn_def_text ) {
          //
          // For a footnote definition line that does not have text on the same
          // line:
          //
          //      [^1]:
          //          Like this.
          //
          // print the marker line as-is "behind wrap's back" so it won't be
          // wrapped.
          //
          FPUTS( line, fout );
          line[0] = '\0';
        }
        break;
      default:
        /* suppress warning */;
    } // switch
    prev_line_type = md->line_type;
  }

  switch ( md->line_type ) {
    case MD_CODE:
    case MD_HEADER_ATX:
    case MD_HEADER_LINE:
    case MD_HR:
    case MD_HTML_ABBR:
    case MD_HTML_BLOCK:
    case MD_LINK_LABEL:
    case MD_TABLE:
      //
      // Flush out_buf and print the Markdown line as-is "behind wrap's back"
      // because these line types are never wrapped.
      //
      print_lead_chars();
      print_line( out_len, true );
      FPUTS( line, fout );
      return false;

    case MD_DL:
    case MD_FOOTNOTE_DEF:
    case MD_OL:
    case MD_UL:
      if ( md->seq_num > prev_seq_num ) {
        //
        // We're changing line types: flush out_buf.
        //
        print_lead_chars();
        print_line( out_len, true );
        prev_seq_num = md->seq_num;
      } else if ( !out_len && !is_blank_line( line ) ) {
        //
        // Same line type, but new line: hang indent.
        //
        put_tabs_spaces( 0, md->indent_hang );
      }
      line_width = opt_line_width - md->indent_left;
      opt_lead_spaces = md->indent_left;
      opt_hang_spaces = md->indent_hang;
      return true;

    default:
      line_width = opt_line_width;
      opt_lead_spaces = 0;
      opt_hang_spaces = 0;
      return true;
  } // switch
}

static void print_lead_chars( void ) {
  if ( proto_buf[0] ) {
    FPRINTF( fout, "%s%s", proto_buf, out_len ? proto_tws : "" );
  } else if ( out_len ) {
    for ( size_t i = 0; i < opt_lead_tabs; ++i )
      FPUTC( '\t', fout );
    for ( size_t i = 0; i < opt_lead_spaces; ++i )
      FPUTC( ' ', fout );
  }
}

static void print_line( size_t len, bool do_eol ) {
  out_buf[ len ] = '\0';
  if ( len ) {
    FPUTS( out_buf, fout );
    if ( do_eol )
      print_eol();
  }
  out_len = out_width = 0;
}

static void put_tabs_spaces( size_t tabs, size_t spaces ) {
  out_width += tabs * opt_tab_spaces + spaces;
  for ( ; tabs > 0; --tabs )
    out_buf[ out_len++ ] = '\t';
  for ( ; spaces > 0; --spaces )
    out_buf[ out_len++ ] = ' ';
}

/**
 * Reads the next line of input.  If wrapping Markdown, adjust wrap's settings.
 *
 * @param line The line to read into.
 * @return Returns the number of bytes read.
 */
static size_t read_line( line_buf_t line ) {
  size_t bytes_read;

  while ( (bytes_read = buf_read( line, fin )) ) {
    // Don't pass IPC lines through the Markdown parser.
    if ( !opt_markdown || line[0] == WIPC_HELLO || markdown_adjust( line ) )
      break;
  } // while

  if ( !bytes_read )
    MD_DEBUG( "====================\n" );
  return bytes_read;
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
"  -l char    Specify end-of-lines as input/Unix/Windows [default: input].\n"
"  -L string  Specify string to prepend to every line.\n"
"  -m number  Mirror tabs.\n"
"  -M number  Mirror spaces.\n"
"  -n         Does not treat newlines as paragraph delimeters.\n"
"  -N         Treat every newline as a paragraph delimiter.\n"
"  -o file    Write to this file [default: stdout].\n"
"  -p chars   Specify additional paragraph delimiter characters.\n"
"  -P         Treat leading whitespace on first line as prototype.\n"
"  -s number  Specify tab-spaces equivalence [default: %d].\n"
"  -S number  Prepend leading spaces after leading tabs to each line.\n"
"  -t number  Prepend leading tabs to each line.\n"
"  -T         Treat paragraph first line as title.\n"
"  -u         Format Markdown.\n"
"  -v         Print version an exit.\n"
"  -w number  Specify line width [default: %d]\n"
"  -W         Treat line beginning with whitespace as paragraph delimiter.\n"
    , me, me, CONF_FILE_NAME, TAB_SPACES_DEFAULT, LINE_WIDTH_DEFAULT
  );
  exit( EX_USAGE );
}

/**
 * Sends an already formatted IPC (interprocess communication) message (if not
 * empty) to wrapc.
 *
 * @param fout The FILE to send to.
 * @param msg The message to send.  If sent, the buffer is truncated.
 */
static void wipc_send( FILE *fout, char *msg ) {
  if ( msg[0] ) {
    WIPC_SENDF( fout, /*IPC_code=*/msg[0], "%s", msg + 1 );
    msg[0] = '\0';
    if ( ipc_line_width ) {
      line_width = opt_line_width = ipc_line_width;
      ipc_line_width = 0;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
