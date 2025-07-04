/*
**      wrap -- text reformatter
**      src/wrap.c
**
**      Copyright (C) 1996-2024  Paul J. Lucas
**
**      This program is free software: you can redistribute it and/or modify
**      it under the terms of the GNU General Public License as published by
**      the Free Software Foundation, either version 3 of the License, or
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

/**
 * @file
 * Implements **wrap**(1).
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "alias.h"
#include "common.h"
#include "markdown.h"
#include "options.h"
#include "pattern.h"
#include "unicode.h"
#include "util.h"
#include "wregex.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), ... */
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

/// @endcond

/**
 * @defgroup wrap-group Wrap
 * Implements the **wrap**(1), a a filter for reformatting text by wrapping and
 * filling lines to a given line-width.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

#define WIPC_DEFER(BUF,SIZE,CODE) WPIC_DEFERF( BUF, SIZE, CODE, "%c", '\n' )

#define WIPC_DEFERF(BUF,SIZE,CODE,FORMAT,...) \
  snprintf( (BUF), (SIZE), ("%c" FORMAT), (CODE), __VA_ARGS__ )

/**
 * Hyphenation states.
 */
enum hyphen {                           // H = hyphen-adjacent char
  HYPHEN_NO,                            ///< Didn't encounter a hyphen.
  HYPHEN_MAYBE,                         ///< Encountered `H-`.
  HYPHEN_YES                            ///< Encountered `H-H`.
};
typedef enum hyphen hyphen_t;

/**
 * Line indentation type.
 */
enum indent {
  INDENT_NONE,                          ///< No indentation.

  /// Indent only first line of a paragraph.
  INDENT_LINE,

  /// Indent all lines but first of a paragraph.
  INDENT_HANG
};
typedef enum indent indent_t;

// extern variable definitions
char const         *me;                 // executable name

// local variable definitions
static wregex_t     block_regex;        ///< Compiled from opt_block_regex.
static size_t       consec_newlines;    ///< Number of consecutive newlines.
static bool         encountered_nonws;  ///< Encountered a non-whitespace char?
static hyphen_t     hyphen;             ///< Hyphen state.
static indent_t     indent = INDENT_LINE;
static line_buf_t   input_buf;          ///< Input buffer.
static line_buf_t   ipc_buf;            ///< Deferred IPC message.
static size_t       ipc_width;          ///< Deferred IPC line width.
static bool         is_long_line;       ///< Line longer than line_width?
static bool         is_preformatted;    ///< Passing through preformatted text?
static size_t       line_width;         ///< Maximum width of a line.
static size_t       nonws_no_wrap_range[2];
static wregex_t     nonws_no_wrap_regex;
static line_buf_t   output_buf;         ///< Output buffer.
static size_t       output_len;         ///< Number of characters in output_buf.
static size_t       output_width;       ///< Actual width of output_buf.
static line_buf_t   proto_buf;          ///< Prototype buffer.
static line_buf_t   proto_tws;          // prototype trailing whitespace, if any
static size_t       put_spaces;         ///< Spaces to put between words.
static bool         was_eos_char;       ///< Prev char an end-of-sentence char?

// local functions
NODISCARD
static char32_t     buf_getcp( char const**, utf8c_t );

NODISCARD
static size_t       buf_readline( void );

static void         delimit_paragraph( void );
static void         init( int, char const*[] );

NODISCARD
static bool         markdown_adjust( void );

static void         markdown_reset( void );
static void         put_lead_chars( void );
static void         put_line( size_t, bool );
static void         put_tabs_spaces( size_t, size_t );

_Noreturn
static void         usage( int );

static void         wipc_parse( char const** );
static void         wipc_send( char* );
static void         wrap_cleanup( void );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether the "block" regular expression matches the input buffer.
 *
 * @return Returns `true` only if it does.
 */
NODISCARD
static inline bool block_regex_matches( void ) {
  return  opt_block_regex != NULL &&
          regex_match( &block_regex, input_buf, 0, NULL );
}

/**
 * Checks whether \a cp is a paragraph delimiter Unicode character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is a paragraph delimiter character.
 */
NODISCARD
static inline bool cp_is_para_delim( char32_t cp ) {
  return  opt_para_delims != NULL && cp_is_ascii( cp ) &&
          strchr( opt_para_delims, STATIC_CAST( int, cp ) ) != NULL;
}

/**
 * Prints an end-of-line and sends any pending IPC message to **wrapc**(1).
 */
static inline void put_eol( void ) {
  PUTS( eol() );
  wipc_send( ipc_buf );
}

////////// main ///////////////////////////////////////////////////////////////

/**
 * The main entry point.
 *
 * @param argc The command-line argument count.
 * @param argv The command-line argument values.
 * @return Returns 0 on success, non-zero on failure.
 */
int main( int argc, char const *argv[] ) {
  wait_for_debugger_attach( "WRAP_DEBUG" );
  init( argc, argv );

  bool        next_line_is_title = opt_title_line;
  char const *pb = input_buf;           // pointer to current byte
  utf8c_t     utf8c;                    // current character's UTF-8 byte(s)
  size_t      wrap_pos = 0;             // position at which we can wrap

  /////////////////////////////////////////////////////////////////////////////

  for ( char32_t cp, cp_prev = '\n';    // current and previous codepoints
        (cp = buf_getcp( &pb, utf8c )) != CP_EOF; cp_prev = cp ) {

    if ( cp == CP_BYTE_ORDER_MARK || cp == CP_INVALID )
      continue;

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE NEWLINE(s)
    ///////////////////////////////////////////////////////////////////////////

    if ( cp == '\r' ) {
      //
      // The code is simpler if we always strip \r and add it back later (if
      // opt_eol is EOL_WINDOWS).
      //
      continue;
    }

    if ( cp == '\n' ) {
      encountered_nonws = false;

      if ( ++consec_newlines >= opt_newlines_delimit ) {
        //
        // At least opt_newlines_delimit consecutive newlines: set that the
        // next line is a title line and delimit the paragraph.
        //
        next_line_is_title = opt_title_line;
        delimit_paragraph();
        continue;
      }
      if ( output_len > 0 && true_clear( &next_line_is_title ) ) {
        //
        // The first line of the next paragraph is title line and the buffer
        // isn't empty (there is a title): print the title.
        //
        delimit_paragraph();
        indent = INDENT_HANG;
        continue;
      }
      if ( was_eos_char ) {
        if ( opt_eos_delimit ) {
          //
          // End-of-sentence characters delimit paragraphs and the previous
          // character was an end-of-sentence character: delimit the paragraph.
          //
          delimit_paragraph();
        } else {
          //
          // We are joining a line after the end of a sentence: force requested
          // number of spaces.
          //
          put_spaces = opt_eos_spaces;
        }
        continue;
      }
      if ( hyphen == HYPHEN_MAYBE ) {
        //
        // We've encountered H-\n meaning that a potentially hyphenated word
        // ends a line: eat the newline so the word can potentially be rejoined
        // to the next word when wrapped, e.g.:
        //
        //      non-
        //      whitespace
        //
        // can become:
        //
        //      non-whitespace
        //
        // instead of:
        //
        //      non- whitespace
        //
        continue;
      }
    } else {
      consec_newlines = 0;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE WHITESPACE
    ///////////////////////////////////////////////////////////////////////////

    if ( cp_is_space( cp ) ) {
      if (  //
            // We've been handling a "long line" and finally got a whitespace
            // character at which we can finally wrap: delimit the paragraph.
            //
            is_long_line ||
            //
            // Leading whitespace characters delimit paragraphs and the
            // previous character was a newline which means this whitespace
            // character is at the beginning of a line: delimit the paragraph.
            //
            (opt_lead_ws_delimit && cp_prev == '\n') ||
            //
            // End-of-sentence characters delimit paragraphs and the previous
            // character was an end-of-sentence character: delimit the
            // paragraph.
            //
            (opt_eos_delimit && was_eos_char) ||
            //
            // The previous character was a paragraph-delimiter character (set
            // only if opt_para_delims was set): delimit the paragraph.
            //
            cp_is_para_delim( cp_prev ) ) {
        delimit_paragraph();
      }
      else if ( hyphen == HYPHEN_MAYBE && !encountered_nonws ) {
        //
        // This case is similar to above: we've encountered H-\n meaning that a
        // potentially hyphenated word ended a line and we've only encountered
        // leading whitespace on the next line so far: eat the space so the
        // word can potentially be rejoined to the next word when wrapped.
        //
      }
      else if ( output_len > 0 &&
                put_spaces < (was_eos_char ? opt_eos_spaces : 1) ) {
        //
        // We are not at the beginning of a line: remember to insert 1 space
        // later and allow opt_eos_spaces after the end of a sentence.
        //
        ++put_spaces;
      }
      continue;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  DISCARD CONTROL CHARACTERS
    ///////////////////////////////////////////////////////////////////////////

    if ( cp_is_control( cp ) )
      continue;

    ///////////////////////////////////////////////////////////////////////////
    //  HANDLE LEADING-PARAGRAPH-DELIMITERS, LEADING-DOT, END-OF-SENTENCE, AND
    //  PARAGRAPH-DELIMITERS
    ///////////////////////////////////////////////////////////////////////////

    if ( cp_prev == '\n' ) {
      if ( opt_lead_dot_ignore && cp == '.' ) {
        consec_newlines = 0;
        delimit_paragraph();
        PUTS( input_buf );              // print the line as-is
        //
        // Make state as if line never happened.
        //
        PJL_DISCARD_RV( buf_readline() );
        pb = input_buf;
        cp = '\n';                      // so cp_prev will become this (again)
        continue;
      }
      if ( block_regex_matches() ) {
        delimit_paragraph();
        if ( opt_markdown ) {
          markdown_init();
          markdown_reset();
        }
      }
      else if ( hyphen == HYPHEN_MAYBE && !cp_is_hyphen_adjacent( cp ) ) {
        //
        // We had encountered H-\n on the previous line meaning that a
        // potentially hyphenated word ends a line, but the first character on
        // the next line is not a "hyphen adjacent character" so forget about
        // hyphenation and put the previously eaten whitespace back.
        //
        hyphen = HYPHEN_NO;
        put_spaces = 1;
      }
    }

    was_eos_char = cp_is_eos( cp ) || (was_eos_char && cp_is_eos_ext( cp ));

    ///////////////////////////////////////////////////////////////////////////
    //  INSERT SPACES
    ///////////////////////////////////////////////////////////////////////////

    if ( put_spaces > 0 ) {
      if ( output_len > 0 ) {
        //
        // Mark position at a space to perform a wrap if necessary.
        //
        wrap_pos = output_len;
        output_width += put_spaces;
        do {
          output_buf[ output_len++ ] = ' ';
        } while ( --put_spaces > 0 );
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

    switch ( indent ) {
      case INDENT_NONE:
        break;
      case INDENT_HANG:
        put_tabs_spaces( opt_hang_tabs, opt_hang_spaces );
        break;
      case INDENT_LINE:
        put_tabs_spaces( opt_indt_tabs, opt_indt_spaces );
        break;
    } // switch
    indent = INDENT_NONE;

    ///////////////////////////////////////////////////////////////////////////
    //  INSERT NON-SPACE CHARACTER
    ///////////////////////////////////////////////////////////////////////////

    encountered_nonws = true;

    if ( !opt_no_hyphen ) {
      size_t const pos = STATIC_CAST( size_t, pb - input_buf );
      if ( pos >= nonws_no_wrap_range[1] || pos < nonws_no_wrap_range[0] ) {
        //
        // We're outside the non-whitespace-no-wrap range.
        //
        if ( hyphen == HYPHEN_MAYBE ) {
          if ( cp_is_hyphen_adjacent( cp ) ) {
            //
            // We've encountered H-H meaning that this is definitely a
            // hyphenated word: set wrap_pos to be here.
            //
            hyphen = HYPHEN_YES;
            wrap_pos = output_len;
          }
          else if ( !cp_is_hyphen( cp ) ) {
            //
            // We've encountered H-X meaning that this is not a hyphenated
            // word.
            //
            hyphen = HYPHEN_NO;
          }
          else {
            //
            // We've encountered H-- meaning that this is still potentially a
            // hyphenated word.
            //
          }
        }
        else if ( cp_is_hyphen_adjacent( cp_prev ) && cp_is_hyphen( cp ) ) {
          //
          // We've encountered H- meaning that this is potentially a
          // hyphenated word.
          //
          hyphen = HYPHEN_MAYBE;
        }
      }
    }

    output_len += utf8_copy_char( output_buf + output_len, utf8c );
    if ( ++output_width < line_width )
      continue;                         // haven't exceeded line width yet

    ///////////////////////////////////////////////////////////////////////////
    //  EXCEEDED LINE WIDTH; PRINT LINE OUT
    ///////////////////////////////////////////////////////////////////////////

    if ( wrap_pos == 0 ) {
      //
      // We've exceeded the line width, but haven't encountered a whitespace
      // character at which to wrap; therefore, we've got a "long line."
      //
      if ( !is_long_line )
        put_lead_chars();
      put_line( output_len, /*do_eol=*/false );
      is_long_line = true;
      continue;
    }

    //
    // A call to put_line() will terminate output_buf with a NULL at wrap_pos
    // that is ordinarily at a space and so doesn't need to be preserved.
    // However, when wrapping at a hyphen, it's at the character past the
    // hyphen that must be preserved in the output so we keep a copy of it to
    // be restored after the call to put_line().
    //
    char const c_past_hyphen = output_buf[ wrap_pos ];

    size_t const prev_output_len = output_len;
    put_lead_chars();
    put_line( wrap_pos, /*do_eol=*/true );

    if ( hyphen != HYPHEN_NO ) {
      //
      // Per the above comment, put the preserved character back and include it
      // in the slide-to-the-left (below).
      //
      output_buf[ wrap_pos-- ] = c_past_hyphen;
    }

    put_tabs_spaces( opt_hang_tabs, opt_hang_spaces );

    //
    // Slide the partial word to the left where we can pick up from where we
    // left off the next time around.
    //
    for ( size_t from_pos = wrap_pos + 1/*null*/;
          from_pos < prev_output_len; ) {
      char const *const from = output_buf + from_pos;
      size_t const len = utf8_len( from[0] );
      if ( !cp_is_space( utf8_decode( from ) ) ) {
        utf8_copy_char( output_buf + output_len, from );
        output_len += len;
        ++output_width;
      }
      from_pos += len;
    } // for

    hyphen = HYPHEN_NO;
    is_long_line = false;
    wrap_pos = 0;
  } // for

  /////////////////////////////////////////////////////////////////////////////

  FERROR( stdin );
  if ( output_len > 0 ) {               // print left-over text
    if ( !is_long_line )
      put_lead_chars();
    put_line( output_len, /*do_eol=*/true );
  }
  exit( EX_OK );
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets the next character from the input.
 *
 * @param ppc A pointer to the pointer to character to advance.
 * @return Returns said character or \c EOF.
 */
NODISCARD
static int buf_getc( char const **ppc ) {
  assert( ppc != NULL );
  assert( *ppc != NULL );
  static bool check_for_nonws_no_wrap_match = true;

  while ( **ppc == '\0' ) {
read_line:
    if ( unlikely( buf_readline() == 0 ) )
      return EOF;
    *ppc = input_buf;
    nonws_no_wrap_range[1] = 0;
    check_for_nonws_no_wrap_match = true;
    //
    // When wrapping Markdown, we have to strip leading whitespace from lines
    // since it interferes with indenting.
    //
    if ( !opt_markdown || *SKIP_CHARS( *ppc, WS_STR ) != '\0' )
      break;
  } // while

  if ( !opt_no_hyphen && check_for_nonws_no_wrap_match ) {
    size_t const pos = STATIC_CAST( size_t, *ppc - input_buf );
    //
    // If there was a previous non-whitespace-no-wrap range and we're past it,
    // see if there is another match on the same line.
    //
    if ( pos >= nonws_no_wrap_range[1] ) {
      check_for_nonws_no_wrap_match = regex_match(
        &nonws_no_wrap_regex, input_buf, pos, nonws_no_wrap_range
      );
    }
  }

  int const c = *(*ppc)++;

  if ( !opt_data_link_esc )
    return c;

  if ( c == WIPC_CODE_HELLO ) {
    wipc_parse( ppc );
    goto read_line;
  }

  if ( is_preformatted ) {
    PUTS( input_buf );
    goto read_line;
  }

  return c;
}

/**
 * Gets bytes comprising the next UTF-8 character and its corresponding Unicode
 * code-point from the input.
 *
 * @param ppc A pointer to the pointer to character to advance.
 * @param utf8c The buffer to put the UTF-8 bytes into.
 * @return Returns said code-point or \c CP_EOF.
 */
NODISCARD
static char32_t buf_getcp( char const **ppc, utf8c_t utf8c ) {
  int c;
  if ( unlikely( (c = buf_getc( ppc )) == EOF ) )
    return CP_EOF;
  size_t const len = utf8_len( STATIC_CAST( char, c ) );
  if ( unlikely( len == 0 ) )
    return CP_INVALID;
  utf8c[0] = STATIC_CAST( char, c );
  for ( size_t i = 1; i < len; ++i ) {
    if ( unlikely( (c = buf_getc( ppc )) == EOF ) )
      return CP_EOF;
    if ( unlikely( !utf8_is_cont( STATIC_CAST( char, c ) ) ) )
      return CP_INVALID;
    utf8c[i] = STATIC_CAST( char, c );
  } // for

  return utf8_decode( utf8c );
}

/**
 * Reads the next line of input.  If wrapping Markdown, adjust wrap's settings.
 *
 * @return Returns the number of bytes read.
 */
NODISCARD
static size_t buf_readline( void ) {
  size_t bytes_read;

  while ( (bytes_read = check_readline( input_buf, stdin )) > 0 ) {
    if ( !opt_markdown )
      break;
    //
    // We're doing Markdown: we might have to adjust wrap's indent, hang-
    // indent, and line-width for each Markdown line.
    //
    // However, don't pass either IPC lines or any lines while is_preformatted
    // is true through the Markdown parser.
    //
    if ( input_buf[0] == WIPC_CODE_HELLO || is_preformatted )
      break;

    if ( markdown_adjust() )
      break;
  } // while

#ifdef DEBUG_MARKDOWN
  if ( bytes_read == 0 )
    MD_DEBUG( "====================\n" );
#endif /* DEBUG_MARKDOWN */
  return bytes_read;
}

/**
 * Delimits a paragraph.
 */
static void delimit_paragraph( void ) {
  if ( output_len > 0 ) {
    //
    // Print what's in the buffer before delimiting the paragraph.  If we've
    // been handling a "long line," it's now finally ended; otherwise, print
    // the leading characters.
    //
    if ( !true_clear( &is_long_line ) )
      put_lead_chars();
    put_line( output_len, /*do_eol=*/true );
  } else if ( is_long_line ) {
    put_eol();                          // delimit the "long line"
  }

  encountered_nonws = false;
  hyphen = HYPHEN_NO;
  indent = opt_markdown ? INDENT_NONE : INDENT_LINE;
  put_spaces = 0;
  was_eos_char = false;

  if ( consec_newlines == 2 ||
      (consec_newlines > 2 && opt_newlines_delimit == 1) ) {
    put_lead_chars();
    put_eol();
  }
}

/**
 * Sets-up clean-up, parses command-line options, reads the conf. file, sets-up
 * I/O, and probes the input for end-of-line type.
 *
 * @param argc The number of command-line arguments from main().
 * @param argv The command-line arguments from main().
 */
static void init( int argc, char const *argv[] ) {
  ASSERT_RUN_ONCE();
  ATEXIT( common_cleanup );
  ATEXIT( wrap_cleanup );

  options_init( argc, argv, usage );
  setlocale_utf8();

  if ( opt_markdown ) {
    markdown_init();
    opt_tab_spaces = MD_TAB_SPACES;
  }

  int const temp_width = STATIC_CAST( int, opt_line_width ) -
    STATIC_CAST( int,
      2 * (opt_mirror_tabs * opt_tab_spaces + opt_mirror_spaces) +
      opt_lead_tabs * opt_tab_spaces + opt_lead_spaces
    );

  if ( temp_width < LINE_WIDTH_MINIMUM ) {
    fatal_error( EX_USAGE,
      "line-width (%d) is too small (<%d)\n",
      temp_width, LINE_WIDTH_MINIMUM
    );
  }
  opt_line_width = line_width = STATIC_CAST( size_t, temp_width );

  opt_lead_tabs   += opt_mirror_tabs;
  opt_lead_spaces += opt_mirror_spaces;

  if ( !opt_no_hyphen ) {
    int const regex_err_code = regex_compile( &nonws_no_wrap_regex, WRAP_RE );
    if ( regex_err_code != 0 ) {
      fatal_error( EX_SOFTWARE,
        "internal regular expression error (%d): %s\n",
        regex_err_code, regex_error( &nonws_no_wrap_regex, regex_err_code )
      );
    }
  }

  if ( opt_block_regex != NULL ) {
    if ( opt_block_regex[0] != '^' ) {
      char *const temp =
        free_later( MALLOC( char, strlen( opt_block_regex ) + 1/*\0*/ ) );
      temp[0] = '^';
      strcpy( temp + 1, opt_block_regex );
      opt_block_regex = temp;
    }
    int const regex_err_code = regex_compile( &block_regex, opt_block_regex );
    if ( regex_err_code != 0 ) {
      fatal_error( EX_USAGE,
        "\"%s\": regular expression error (%d): %s\n",
        opt_block_regex, regex_err_code,
        regex_error( &block_regex, regex_err_code )
      );
    }
  }

  size_t const bytes_read = buf_readline();
  if ( bytes_read == 0 )
    exit( EX_OK );

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
    opt_eol = is_windows_eol( input_buf, bytes_read ) ? EOL_WINDOWS : EOL_UNIX;
  }

  //
  // Copy the prototype and calculate its width.
  //
  if ( opt_lead_string != NULL || opt_prototype ) {
    size_t proto_len = 0;
    size_t proto_width = 0;
    for ( char const *s = opt_lead_string != NULL ? opt_lead_string : input_buf;
          *s != '\0';
          ++s, ++proto_len ) {
      if ( opt_prototype && !is_space( *s ) )
        break;
      if ( proto_len == sizeof proto_buf - 1 )
        break;
      proto_buf[ proto_len ] = *s;
      proto_width += *s == '\t' ?
        (opt_tab_spaces - proto_len % opt_tab_spaces) : 1;
    } // for
    line_width = opt_line_width - proto_width;
    if ( opt_lead_string != NULL ) {
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
}

/**
 * Adjusts wrap's indent, hang-indent, and line-width for each Markdown line.
 *
 * @return Returns `true` if we're to proceed or \c false if another line
 * should be read.
 */
NODISCARD
static bool markdown_adjust( void ) {
  static md_line_t  prev_line_type;
  static md_seq_t   prev_seq_num = MD_SEQ_NUM_INIT;

  md_state_t const *const md = markdown_parse( input_buf );
  MD_DEBUG(
    "T=%c N=%2u D=%u L=%u H=%u|%s",
    STATIC_CAST( char, md->line_type ), md->seq_num, md->depth,
    md->indent_left, md->indent_hang, input_buf
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
        if ( is_blank_line( input_buf ) ) {
          //
          // Prevent blank lines immediately after these Markdown line types
          // from being swallowed by wrap by just printing them directly.
          //
          PUTS( input_buf );
        }
        break;
      case MD_DL:
      case MD_FOOTNOTE_DEF:
      case MD_HEADER_LINE:
      case MD_NONE:
      case MD_OL:
      case MD_TEXT:
      case MD_UL:
        // nothing to do
        break;
    } // switch

    if ( md->line_type == MD_FOOTNOTE_DEF && !md->footnote_def_has_text ) {
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
      PUTS( input_buf );
      input_buf[0] = '\0';
    }

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
      // Flush output_buf and print the Markdown line as-is "behind wrap's
      // back" because these line types are never wrapped.
      //
      put_lead_chars();
      put_line( output_len, /*do_eol=*/true );
      PUTS( input_buf );
      return false;

    case MD_DL:
    case MD_FOOTNOTE_DEF:
    case MD_OL:
    case MD_UL:
      if ( md->seq_num > prev_seq_num ) {
        //
        // We're changing line types: flush output_buf.
        //
        put_lead_chars();
        put_line( output_len, /*do_eol=*/true );
        prev_seq_num = md->seq_num;
      }
      else if ( output_len == 0 && !is_blank_line( input_buf ) ) {
        //
        // Same line type, but new line: hang indent.
        //
        put_tabs_spaces( /*tabs=*/0, md->indent_hang );
      }
      line_width = opt_line_width - md->indent_left;
      opt_lead_spaces = md->indent_left;
      opt_hang_spaces = md->indent_hang;
      return true;

    case MD_NONE:
    case MD_TEXT:
      markdown_reset();
      return true;
  } // switch

  UNEXPECTED_INT_VALUE( md->line_type );
}

/**
 * Resets variables affected by the Markdown parser.
 */
static void markdown_reset( void ) {
  line_width = opt_line_width;
  opt_hang_spaces = opt_lead_spaces = 0;
}

/**
 * Prints the leading characters for lines.
 */
static void put_lead_chars( void ) {
  if ( proto_buf[0] != '\0' ) {
    PRINTF( "%s%s", proto_buf, output_len > 0 ? proto_tws : "" );
  }
  else if ( output_len > 0 ) {
    for ( size_t i = 0; i < opt_lead_tabs; ++i )
      PUTC( '\t' );
    for ( size_t i = 0; i < opt_lead_spaces; ++i )
      PUTC( ' ' );
  }
}

/**
 * Prints the current output buffer as a line and resets the output buffer's
 * length.
 *
 * @param len The length of the output buffer.
 * @param do_eol If `true`, prints and end-of-line afterwards.
 */
static void put_line( size_t len, bool do_eol ) {
  output_buf[ len ] = '\0';
  if ( len > 0 ) {
    PUTS( output_buf );
    if ( do_eol )
      put_eol();
  }
  output_len = output_width = 0;
}

/**
 * Puts \a tabs tabs and \a spaces spaces (in that order) into the output
 * buffer and increments the output width accordingly.
 *
 * @param tabs The number of tabs to put.
 * @param spaces The number of spaces to put.
 */
static void put_tabs_spaces( size_t tabs, size_t spaces ) {
  output_width += tabs * opt_tab_spaces + spaces;
  while ( tabs-- > 0 )
    output_buf[ output_len++ ] = '\t';
  while ( spaces-- > 0 )
    output_buf[ output_len++ ] = ' ';
}

/**
 * Prints the usage message and exits.
 *
 * @param status The status to exit with.  If it is `EX_OK`, prints to standard
 * output; otherwise prints to standard error.
 */
static void usage( int status ) {
  fprintf( status == EX_OK ? stdout : stderr,
"usage: " PACKAGE " [options]\n"
"options:\n"
"  --alias=NAME           " UOPT(ALIAS)
                          "Use alias from configuration file.\n"
"  --all-newlines-delimit " UOPT(ALL_NEWLINES_DELIMIT)
                          "Treat newlines as paragraph delimiters.\n"
"  --block-regex=REGEX    " UOPT(BLOCK_REGEX)
                          "Block leading regular expression.\n"
"  --config=FILE          " UOPT(CONFIG)
                          "Configuration file path [default: ~/" CONF_FILE_NAME_DEFAULT "].\n"
"  --dot-ignore           " UOPT(DOT_IGNORE)
                          "Do not alter lines that begin with '.' (dot).\n"
"  --eol=STR              " UOPT(EOL) "\n"
"      Set line-endings as input/Unix/Windows [default: input].\n"
"  --eos-delimit          " UOPT(EOS_DELIMIT) "\n"
"      Treat whitespace after end-of-sentence as a paragraph delimiter.\n"
"  --eos-spaces=NUM       " UOPT(EOS_SPACES)
                          "Spaces after end-of-sentence [default: " STRINGIFY(EOS_SPACES_DEFAULT) "].\n"
"  --file=FILE            " UOPT(FILE)
                          "Read from this file [default: stdin].\n"
"  --file-name=NAME       " UOPT(FILE_NAME)
                          "Filename for stdin.\n"
"  --hang-spaces=NUM      " UOPT(HANG_SPACES) "\n"
"      Hang-indent spaces after tabs for all but first line of every paragraph.\n"
"  --hang-tabs=NUM        " UOPT(HANG_TABS) "\n"
"      Hang-indent tabs for all but first line of every paragraph.\n"
"  --help                 " UOPT(HELP)
                          "Print this help and exit.\n"
"  --indent-spaces=NUM    " UOPT(INDENT_SPACES) "\n"
"      Indent spaces after tabs for first line of every paragraph.\n"
"  --indent-tabs=NUM      " UOPT(INDENT_TABS)
                          "Indent tabs for first line of every paragraph.\n"
"  --lead-spaces=NUM      " UOPT(LEAD_SPACES)
                          "Prepend leading spaces after tabs to every line.\n"
"  --lead-string=STR      " UOPT(LEAD_STRING)
                          "String to prepend to every line.\n"
"  --lead-tabs=NUM        " UOPT(LEAD_TABS)
                          "Prepend leading tabs to every line.\n"
"  --markdown             " UOPT(MARKDOWN)
                          "Format Markdown.\n"
"  --mirror-spaces=NUM    " UOPT(MIRROR_SPACES)
                          "Mirror spaces.\n"
"  --mirror-tabs=NUM      " UOPT(MIRROR_TABS)
                          "Mirror tabs.\n"
"  --no-config            " UOPT(NO_CONFIG)
                          "Suppress reading configuration file.\n"
"  --no-hyphen            " UOPT(NO_HYPHEN)
                          "Suppress wrapping at hyphen characters.\n"
"  --no-newlines-delimit  " UOPT(NO_NEWLINES_DELIMIT)
                          "Do not treat newlines as paragraph delimiters.\n"
"  --output=FILE          " UOPT(OUTPUT)
                          "Write to this file [default: stdout].\n"
"  --para-chars=STR       " UOPT(PARA_CHARS)
                          "Additional paragraph delimiter characters.\n"
"  --prototype            " UOPT(PROTOTYPE) "\n"
"      Treat leading whitespace on first line as prototype.\n"
"  --tab-spaces=NUM       " UOPT(TAB_SPACES)
                          "Tab-spaces equivalence [default: " STRINGIFY(TAB_SPACES_DEFAULT) "].\n"
"  --title                " UOPT(TITLE_LINE)
                          "Treat paragraph's first line as title.\n"
"  --version              " UOPT(VERSION)
                          "Print version and exit.\n"
"  --whitespace-delimit   " UOPT(WHITESPACE_DELIMIT) "\n"
"      Treat lines beginning with whitespace as paragraph delimiters.\n"
"  --width=NUM|terminal   " UOPT(WIDTH)
                          "Line width [default: " STRINGIFY(LINE_WIDTH_DEFAULT) "].\n"
"\n"
PACKAGE_NAME " home page: " PACKAGE_URL "\n"
"Report bugs to: " PACKAGE_BUGREPORT "\n"
  );
  exit( status );
}

/**
 * Parses a \ref wipc_code.
 *
 * @param ppc A pointer to the pointer to character to advance.  It must be
 * positioned at the IPC code after #WIPC_CODE_HELLO.
 */
static void wipc_parse( char const **ppc ) {
  assert( ppc != NULL );
  assert( *ppc != NULL );

  char const c = *(*ppc)++;
  if ( unlikely( c == '\0' ) )
    return;

  switch ( STATIC_CAST( wipc_code_t, c ) ) {
    case WIPC_CODE_HELLO:               // shouldn't happen
      break;

    case WIPC_CODE_DELIMIT_PARAGRAPH:
      consec_newlines = 0;
      delimit_paragraph();
      WIPC_SEND( stdout, WIPC_CODE_DELIMIT_PARAGRAPH );
      break;

    case WIPC_CODE_NEW_LEADER:
      NO_OP;
      //
      // We've been told by wrapc (child 1) that the comment characters and/or
      // leading whitespace has changed: we have to echo it back to the other
      // wrapc process (parent).
      //
      // If an output line has already been started, we have to defer the IPC
      // until just after the line is sent; otherwise, we must send it
      // immediately.
      //
      char *sep;
      size_t const new_line_width = strtoul( *ppc, &sep, 10 );
      if ( output_len > 0 ) {
        WIPC_DEFERF(
          ipc_buf, sizeof ipc_buf,
          WIPC_CODE_NEW_LEADER, "%zu" WIPC_PARAM_SEP "%s",
          new_line_width, sep + 1
        );
        ipc_width = new_line_width;
      } else {
        WIPC_SENDF(
          stdout,
          WIPC_CODE_NEW_LEADER, "%zu" WIPC_PARAM_SEP "%s",
          new_line_width, sep + 1
        );
        line_width = opt_line_width = new_line_width;
      }
      break;

    case WIPC_CODE_PREFORMATTED_BEGIN:
      delimit_paragraph();
      WIPC_SEND( stdout, WIPC_CODE_PREFORMATTED_BEGIN );
      is_preformatted = true;
      break;

    case WIPC_CODE_PREFORMATTED_END:
      consec_newlines = 1;
      delimit_paragraph();
      WIPC_SEND( stdout, WIPC_CODE_PREFORMATTED_END );
      is_preformatted = false;
      break;

    case WIPC_CODE_WRAP_END:
      //
      // We've been told by wrapc (child 1) that we've reached the end of the
      // comment: dump any remaining buffer, propagate the interprocess message
      // to the other wrapc process (parent), and pass text through verbatim.
      //
      consec_newlines = 0;
      delimit_paragraph();
      WIPC_SEND( stdout, WIPC_CODE_WRAP_END );
      fcopy( stdin, stdout );
      exit( EX_OK );
  } // switch
}

/**
 * Sends an already formatted IPC (interprocess communication) message (if not
 * empty) to **wrapc**(1).
 *
 * @param msg The message to send.  If sent, the buffer is truncated.
 */
static void wipc_send( char *msg ) {
  assert( msg != NULL );
  if ( msg[0] != '\0' ) {
    WIPC_SENDF( stdout, /*IPC_code=*/msg[0], "%s", msg + 1 );
    msg[0] = '\0';
    if ( ipc_width > 0 ) {
      line_width = opt_line_width = ipc_width;
      ipc_width = 0;
    }
  }
}

/**
 * Cleans up wrap data.
 */
static void wrap_cleanup( void ) {
  regex_free( &block_regex );
  regex_free( &nonws_no_wrap_regex );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

/* vim:set et sw=2 ts=2: */
