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
#include <inttypes.h>                   /* for uint32_t */
#include <stddef.h>                     /* for size_t */
#include <stdio.h>
#include <stdlib.h>                     /* for exit(), ... */
#include <string.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////

#define WIPC_DEFERF(BUF,SIZE,CODE,FORMAT,...) \
  snprintf( (BUF), (SIZE), ("%c" FORMAT), (CODE), __VA_ARGS__ )

/**
 * Unicode code-point.
 */
typedef uint32_t codepoint_t;

/**
 * Hyphenation states.
 */
enum hyphen {
  HYPHEN_NO,
  HYPHEN_MAYBE,                         // encountered H-
  HYPHEN_YES                            // encountered H-H
};
typedef enum hyphen hyphen_t;

/**
 * Line indentation type.
 */
enum indent {
  INDENT_NONE,
  INDENT_LINE,
  INDENT_HANG
};
typedef enum indent indent_t;

// extern variable definitions
char const         *me;                 // executable name

// local constant definitions
static codepoint_t const  CP_BYTE_ORDER_MARK      = 0x00FEFF;
static codepoint_t const  CP_INVALID              = ~0;
static codepoint_t const  CP_SURROGATE_HIGH_START = 0x00D800;
static codepoint_t const  CP_SURROGATE_LOW_END    = 0x00DFFF;
static codepoint_t const  CP_VALID_MAX            = 0x10FFFF;

// local variable definitions
static line_buf_t   in_buf;
static line_buf_t   ipc_buf;            // deferred IPC message
static size_t       ipc_width;          // deferred IPC line width
static size_t       line_width;         // max width of line
static line_buf_t   out_buf;
static size_t       out_len;            // number of characters in buffer
static size_t       out_width;          // actual width of buffer
static line_buf_t   proto_buf;
static line_buf_t   proto_tws;          // prototype trailing whitespace, if any
static size_t       proto_len;
static size_t       proto_width;

// local variable definitions specific to wrap state
static unsigned     consec_newlines;    // number of consecutive newlines
static bool         encountered_non_whitespace;
static hyphen_t     hyphen;
static bool         ignore_lead_dot;    // ignore leading '.' line?
static indent_t     indent = INDENT_LINE;
static bool         is_long_line;       // line longer than line_width?
static char const  *pc = in_buf;        // pointer to current character
static unsigned     put_spaces;         // number of spaces to put between words
static bool         was_eos_char;       // prev char an end-of-sentence char?

// local functions
static int          buf_getc( char const** );
static void         delimit_paragraph( void );
static void         init( int, char const*[] );
static bool         is_hyphen( codepoint_t );
static void         markdown_reset();
static void         print_lead_chars( void );
static void         print_line( size_t, bool );
static void         put_tabs_spaces( size_t, size_t );
static size_t       read_line( line_buf_t );
static void         usage( void );
static codepoint_t  utf8_decode_impl( char const* );
static void         wipc_send( FILE*, char* );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether a given Unicode code-point is valid.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if valid.
 */
static inline bool is_codepoint_valid( codepoint_t cp ) {
  return  cp < CP_SURROGATE_HIGH_START
      || (cp > CP_SURROGATE_LOW_END && cp <= CP_VALID_MAX);
}

/**
 * Checks whether \a c is an "end-of-sentence character."
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is an end-of-sentence character.
 */
static inline bool is_eos_char( char c ) {
  return c == '.' || c == '?' || c == '!';
}

/**
 * Checks whether \a c is an "end-of-sentence-extender character," that is a
 * character that extends being in the end-of-sentence state, e.g., a ')'
 * following a '.'.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is an end-of-sentence-extender
 * character.
 */
static inline bool is_eos_ext_char( char c ) {
  return c == '"' || c == '\'' || c == ')' || c == ']';
}

/**
 * Checks whether \a c is a "hyphen adjacent character," that is a character
 * that can appear adjacent to a hyphen on either side.
 *
 * @param c The character to check. (The type is \c int because the type of the
 * argument to \c isalpha() is.)
 * @return Returns \c true only if \a c can appear on either side of a hyphen.
 */
static inline bool is_hyphen_adjacent_char( int c ) {
  return isalpha( c );
}

/**
 * Checks wherher \a c is a leading paragraph delimiter character.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is a leading paragraph delimiter
 * character.
 */
static inline bool is_lead_para_delim_char( char c ) {
  return opt_lead_para_delims && strchr( opt_lead_para_delims, c ) != NULL;
}

/**
 * Checks wherher \a c is a paragraph delimiter character.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is a paragraph delimiter character.
 */
static inline bool is_para_delim_char( char c ) {
  return opt_para_delims && strchr( opt_para_delims, c ) != NULL;
}

/**
 * Prints an end-of-line and sends any pending IPC message to wrapc.
 */
static inline void print_eol( void ) {
  FPUTS( eol(), fout );
  wipc_send( fout, ipc_buf );
}

/**
 * Decodes a UTF-8 encoded character into its corresponding Unicode code-point.
 * This inline version is optimized for the common case of ASCII.
 *
 * @param s A pointer to the first byte of the UTF-8 encoded character.
 * @return Returns said code-point.
 */
static inline codepoint_t utf8_decode( char const *s ) {
  unsigned char const *const u = (unsigned char const*)s;
  return *u <= 127 ? *u : utf8_decode_impl( s );
}

/**
 * Checks whether the given byte is not the first byte of a UTF-8 byte sequence
 * of an encoded character.
 *
 * @param c The byte to check.
 * @return Returns \c true only if the byte is not the first byte of a byte
 * sequence of a UTF-8 encoded character.
 */
static inline bool utf8_is_cont( char c ) {
  unsigned char const u = c;
  return u >= 0x80 && u < 0xC0;
}

/**
 * Gets the number of bytes for the UTF-8 encoding of a Unicode codepoint given
 * its first byte.
 *
 * @param c The first byte of the UTF-8 encoded codepoint.
 * @return Returns 1-6, or 0 if \a c is invalid.
 */
static inline size_t utf8_len( char c ) {
  static char const UTF8_LEN_TABLE[] = {
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
  return (size_t)UTF8_LEN_TABLE[ (unsigned char)c ];
}

////////// main ///////////////////////////////////////////////////////////////

int main( int argc, char const *argv[] ) {
  wait_for_debugger_attach( "WRAP_DEBUG" );
  init( argc, argv );

  char    c_prev = '\0';                // prev char
  bool    next_line_is_title = opt_title_line;
  size_t  wrap_pos = 0;                 // position at which we can wrap

  /////////////////////////////////////////////////////////////////////////////

  for ( int c; (c = buf_getc( &pc )) != EOF; c_prev = c ) {

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
      encountered_non_whitespace = false;

      if ( ++consec_newlines >= opt_newlines_delimit ) {
        //
        // At least opt_newlines_delimit consecutive newlines: set that the
        // next line is a title line and delimit the paragraph.
        //
        next_line_is_title = opt_title_line;
        delimit_paragraph();
        continue;
      }
      if ( out_len && true_reset( &next_line_is_title ) ) {
        //
        // The first line of the next paragraph is title line and the buffer
        // isn't empty (there is a title): print the title.
        //
        print_lead_chars();
        print_line( out_len, true );
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
          // We are joining a line after the end of a sentence: force 2 spaces.
          //
          put_spaces = 2;
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

    if ( isspace( c ) ) {
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
            (opt_lead_ws_delimit && c_prev == '\n') ||
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
            is_para_delim_char( c_prev ) ) {
        delimit_paragraph();
      }
      else if ( hyphen == HYPHEN_MAYBE && !encountered_non_whitespace ) {
        //
        // This case is similar to above: we've encountered H-\n meaning that a
        // potentially hyphenated word ended a line and we've only encountered
        // leading whitespace on the next line so far: eat the space so the
        // word can potentially be rejoined to the next word when wrapped.
        //
        continue;
      }
      else if ( out_len && put_spaces < 1u + was_eos_char ) {
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

    if ( c_prev == '\n' ) {
      if ( opt_lead_dot_ignore && c == '.' ) {
        ignore_lead_dot = true;
        delimit_paragraph();
        continue;
      }
      if ( is_lead_para_delim_char( c ) ) {
        delimit_paragraph();
        if ( opt_markdown ) {
          markdown_init();
          markdown_reset();
        }
        goto insert;
      }
      if ( hyphen == HYPHEN_MAYBE && !is_hyphen_adjacent_char( c ) ) {
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

    was_eos_char = is_eos_char( c ) || (was_eos_char && is_eos_ext_char( c ));

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

    encountered_non_whitespace = true;

    size_t len = utf8_len( c );         // bytes in UTF-8 encoded character
    if ( !len )                         // not a UTF-8 start byte
      continue;

    size_t tmp_out_len = out_len;       // use temp length if invalid UTF-8
    out_buf[ tmp_out_len++ ] = c;

    //
    // If we've just read the start byte of a multi-byte UTF-8 encoded
    // character, read the remaining bytes.  The entire muti-byte character is
    // always treated as having a width of 1.
    //
    for ( ; len > 1; --len ) {
      if ( (c = buf_getc( &pc )) == EOF )
        goto done;                      // premature end of UTF-8 character
      out_buf[ tmp_out_len++ ] = c;
      if ( !utf8_is_cont( c ) )         // not a UTF-8 continuation byte
        goto next_char;                 // skip entire UTF-8 character
    } // for

    char const *const utf8_c = out_buf + out_len;
    codepoint_t const cp = utf8_decode( utf8_c );
    if ( cp == CP_BYTE_ORDER_MARK || cp == CP_INVALID )
      continue;                         // discard UTF-8 BOM or invalid CP

    if ( !opt_no_hyphen ) {
      if ( hyphen == HYPHEN_MAYBE ) {
        if ( is_hyphen_adjacent_char( c ) ) {
          //
          // We've encountered H-H meaning that this is definitely a
          // hyphenated word: set wrap_pos to be here.
          //
          hyphen = HYPHEN_YES;
          wrap_pos = out_len;
        }
        else if ( !is_hyphen( cp ) ) {
          //
          // We've encountered H-X meaning that this is not a hyphenated word.
          //
          hyphen = HYPHEN_NO;
        }
        else {
          //
          // We've encountered H-- meaning that this is still potentially a
          // hyphenated word.
          //
        }
      } else {
        if ( is_hyphen_adjacent_char( c_prev ) && is_hyphen( cp ) ) {
          //
          // We've encountered H- meaning that this is potentially a hyphenated
          // word.
          //
          hyphen = HYPHEN_MAYBE;
        }
      }
    }

    out_len = tmp_out_len;

    if ( ++out_width < line_width )
      continue;                         // haven't exceeded line width yet

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

    char c_past_hyphen = '\0';
    if ( hyphen == HYPHEN_YES ) {
      //
      // A call to print_line() will terminate out_buf with a NULL at wrap_pos
      // that is ordinarily at a space and so doesn't need to be preserved.
      // However, during hyphenation, it's at the character past a '-' that
      // must be preserved in the output so we keep a copy of it to be restored
      // after the call to print_line().
      //
      c_past_hyphen = out_buf[ wrap_pos ];
    }

    print_line( wrap_pos, true );

    if ( hyphen == HYPHEN_YES ) {
      //
      // Per the above comment, put the preserved character back and include it
      // in the slide-to-the-left (below).
      //
      out_buf[ wrap_pos-- ] = c_past_hyphen;
    }

    put_tabs_spaces( opt_hang_tabs, opt_hang_spaces );

    //
    // Slide the partial word to the left where we can pick up from where we
    // left off the next time around.
    //
    for ( size_t from = wrap_pos + 1/*null*/; from < tmp_out_len; ++from ) {
      c = out_buf[ from ];
      if ( !isspace( c ) ) {
        out_buf[ out_len++ ] = c;
        ++out_width;
        for ( len = utf8_len( c ); len > 1; --len )
          out_buf[ out_len++ ] = out_buf[ ++from ];
      }
    } // for

    hyphen = HYPHEN_NO;
    wrap_pos = 0;

next_char:
    NO_OP;

  } // for

  /////////////////////////////////////////////////////////////////////////////

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
          ipc_width = new_line_width;
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
 * Delimits a paragraph.
 */
static void delimit_paragraph( void ) {
  if ( out_len ) {
    //
    // Print what's in the buffer before delimiting the paragraph.  If we've
    // been handling a "long line," it's now finally ended; otherwise, print
    // the leading characters.
    //
    if ( !true_reset( &is_long_line ) )
      print_lead_chars();
    print_line( out_len, true );
  } else if ( is_long_line )
    print_eol();                      // delimit the "long line"

  encountered_non_whitespace = false;
  hyphen = HYPHEN_NO;
  put_spaces = 0;
  was_eos_char = false;

  if ( true_reset( &ignore_lead_dot ) ) {
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

  if ( opt_markdown ) {
    markdown_init();
    opt_tab_spaces =  TAB_SPACES_MARKDOWN;
  }

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

  size_t const bytes_read = read_line( in_buf );
  if ( !bytes_read )
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
    opt_eol = bytes_read >= 2 && in_buf[ bytes_read - 2 ] == '\r' ?
      EOL_WINDOWS : EOL_UNIX;
  }

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
}

/**
 * Checks whether the given Unicode code-point is a hyphen-like character.
 *
 * @param cp The Unicode codepoint to check.
 * @return Returns \c true only if \a cp is a Unicode hyphen-like character.
 */
static bool is_hyphen( codepoint_t cp ) {
  //
  // This is (mostly) a union of code-points having the Dash and Hyphen
  // properties; see http://www.unicode.org/Public/UCD/latest/ucd/PropList.txt
  //
  switch ( cp ) {
    case 0x002D:  // HYPHEN-MINUS
    case 0x00AD:  // SOFT HYPHEN
    case 0x058A:  // ARMENIAN HYPHEN
    case 0x05BE:  // HEBREW PUNCTUATION MAQAF
    case 0x1400:  // CANADIAN SYLLABICS HYPHEN
    case 0x1806:  // MONGOLIAN SOFT HYPHEN
    case 0x2010:  // HYPHEN
//  case 0x2011:  // NON-BREAKING HYPHEN // obviously don't want to wrap here
    case 0x2013:  // EN DASH
    case 0x2014:  // EM DASH
    case 0x2015:  // HORIZONTAL BAR
    case 0x2027:  // HYPHENATION POINT
    case 0x2043:  // HYPHEN BULLET
    case 0x2053:  // SWUNG DASH
    case 0x2E17:  // DOUBLE OBLIQUE HYPHEN
    case 0x2E1A:  // HYPHEN WITH DIAERESIS
    case 0x2E40:  // DOUBLE HYPHEN
    case 0x301C:  // WAVE DASH
    case 0x3030:  // WAVY DASH
    case 0x30A0:  // KATAKANA-HIRAGANA DOUBLE HYPHEN
    case 0x30FB:  // KATAKANA MIDDLE DOT
    case 0xFE58:  // SMALL EM DASH
    case 0xFE63:  // SMALL HYPHEN-MINUS
    case 0xFF0D:  // FULLWIDTH HYPHEN-MINUS
    case 0xFF65:  // HALFWIDTH KATAKANA MIDDLE DOT
      return true;
    default:
      return false;
  } // switch
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
      markdown_reset();
      return true;
  } // switch
}

static void markdown_reset( void ) {
  line_width = opt_line_width;
  opt_hang_spaces = opt_lead_spaces = 0;
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

  while ( (bytes_read = check_readline( line, fin )) ) {
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
"  -d         Do not alter lines that begin with '.' (dot).\n"
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
"  -n         Do not treat newlines as paragraph delimeters.\n"
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
"  -w number  Specify line width [default: %d].\n"
"  -W         Treat line beginning with whitespace as paragraph delimiter.\n"
"  -y         Suppress wrapping at hyphen characters.\n"
    , me, me, CONF_FILE_NAME, TAB_SPACES_DEFAULT, LINE_WIDTH_DEFAULT
  );
  exit( EX_USAGE );
}

/**
 * Decodes a UTF-8 encoded character into its corresponding Unicode code-point.
 *
 * @param s A pointer to the first byte of the UTF-8 encoded character.
 * @return Returns said code-point.
 */
static codepoint_t utf8_decode_impl( char const *s ) {
  size_t const len = utf8_len( *s );
  assert( len >= 1 );

  codepoint_t cp = 0;
  unsigned char const *u = (unsigned char const*)s;

  switch ( len ) {
    case 6: cp += *u++; cp <<= 6;
    case 5: cp += *u++; cp <<= 6;
    case 4: cp += *u++; cp <<= 6;
    case 3: cp += *u++; cp <<= 6;
    case 2: cp += *u++; cp <<= 6;
    case 1: cp += *u;
  } // switch

  static codepoint_t const OFFSET_TABLE[] = {
    0, // unused
    0x0, 0x3080, 0xE2080, 0x3C82080, 0xFA082080, 0x82082080
  };
  cp -= OFFSET_TABLE[ len ];
  return is_codepoint_valid( cp ) ? cp : CP_INVALID;
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
    if ( ipc_width ) {
      line_width = opt_line_width = ipc_width;
      ipc_width = 0;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
