/*
**      wrap -- text reformatter
**      markdown.c
**
**      Copyright (C) 2016  Paul J. Lucas
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
#include "common.h"
#include "markdown.h"
#include "util.h"

// standard
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

#define CLEAR_RETURN(TOKEN) \
  BLOCK( stack_clear(); stack_push( TOKEN, 0, 0 ); return &TOP; )

#define PREV_BOOL(NAME,INIT)        \
  static bool prev_##NAME = (INIT); \
  bool const NAME = prev_##NAME;    \
  prev_##NAME = false

#define STACK(N)            (stack[ stack_top - (N) ])
#define TOP                 STACK(0)

/**
 * PHP Markdown Extra code fence info.
 */
struct md_code_fence {
  char    cf_c;                         // character of the fence: ~ or `
  size_t  cf_len;                       // length of the fence
};
typedef struct md_code_fence md_code_fence_t;

typedef ssize_t stack_pos_t;

// local constant definitions
#define             HTML_ELEMENT_CHAR_MAX    10   /* "blockquote" */
static size_t const MD_ATX_CHAR_MAX        =  6;  // max num of # in atx header
static size_t const MD_CODE_FENCE_CHAR_MIN =  3;  // min num of ~~~ or ```
static size_t const MD_CODE_INDENT_MIN     =  4;  // min indent for code
static size_t const MD_FOOTNOTE_INDENT     =  4;
static size_t const MD_HR_CHAR_MIN         =  3;  // min num of ***, ---, or ___
static size_t const MD_LINK_INDENT_MAX     =  3;  // max indent for [id]: URI
static size_t const MD_LIST_INDENT_MAX     =  4;  // max indent for all lists
static size_t const MD_DL_UL_INDENT_MIN    =  2;  // unordered list min indent
static size_t const MD_OL_CHAR_MAX         =  9;  // ordered list max digits
static size_t const MD_OL_INDENT_MIN       =  3;  // ordered list min indent

static size_t const STATE_ALLOC_DEFAULT    = 5;
static size_t const STATE_ALLOC_INCREMENT  = 5;

// local variable definitions
static md_seq_t     next_seq_num;
static md_state_t  *stack;              // global stack of states
static stack_pos_t  stack_top = -1;     // index of the top of the stack

// local functions
static bool         md_is_code_fence( char const*, md_code_fence_t* );
static bool         md_is_dl_ul_helper( char const*, md_indent_t* );
static md_line_t    md_nested_within( void );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a c is an HTML element character.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is an HTML element character.
 */
static inline bool is_html_element_char( char c ) {
  return isalpha( c ) || c == '-';
}

/**
 * Initialized an md_code_fence_t.
 *
 * @param fence A pointer to the md_code_fence_t to initialize.
 */
static inline void md_code_fence_init( md_code_fence_t *fence ) {
  fence->cf_c  = '\0';
  fence->cf_len = 0;
}

/**
 * Checks whether the line is the end of a PHP Markdown Extra code fence.
 *
 * @param s The null-terminated line to check.
 * @param fence A pointer to the \c struct containing the fence info.
 * @return Returns \c true only if it is.
 */
static inline bool md_is_code_fence_end( char const *s,
                                         md_code_fence_t *fence ) {
  return (s[0] == '~' || s[0] == '`') && md_is_code_fence( s, fence );
}

/**
 * Checks whether the line is a Markdown link title attribute.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if it is.
 */
static inline bool md_is_link_title( char const *s ) {
  return *s == '"' || *s == '\'' || *s == '(';
}

/**
 * Checks whether \a line_type can nest.
 *
 * @param line_type The line type to check.
 * @return Returns \c true only if \a line_type can nest.
 */
static inline bool md_is_nestable( md_line_t line_type ) {
  switch ( line_type ) {
    case MD_DL:
    case MD_FOOTNOTE_DEF:
    case MD_OL:
    case MD_UL:
      return true;
    default:
      return false;
  } // switch
}

/**
 * Checks whether \a c is a Markdown ordered list separator character.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is an ordered list separator character.
 */
static inline bool md_is_ol_sep_char( char c ) {
  return c == '.' || c == ')';
}

/**
 * Clears the Markdown stack down to the initial element.
 */
static inline void stack_clear( void ) {
  stack_top = 0;
}

/**
 * Checks whether the Markdown state stack is empty.
 *
 * @return Returns \c true only if it is.
 */
static inline bool stack_empty( void ) {
  return stack_top < 0;
}

/**
 * Pops a Markdown state from the stack.
 */
static inline void stack_pop( void ) {
  MD_DEBUG( "%s()\n", __func__ );
  assert( !stack_empty() );
  --stack_top;
}

/**
 * Gets the size of the Markdown state stack.
 *
 * @return Returns said size.
 */
static inline size_t stack_size( void ) {
  return (size_t)(stack_top + 1);
}

/**
 * Checks whether the line type that's part of the Markdown state on the top of
 * the stack is a particular type.
 *
 * @param line_type The line type to check for.
 * @return Returns \c true only if it is.
 */
static inline bool top_is( md_line_t line_type ) {
  return TOP.line_type == line_type;
}

/**
 * Calculates the minimum indent needed to be considered a line of code.
 *
 * @return Returns said indent.
 */
static md_indent_t md_code_indent_min( void ) {
  return (stack_size() - top_is( MD_CODE )) * MD_CODE_INDENT_MIN;
}

////////// local functions ////////////////////////////////////////////////////

/**
 * Gets a pointer to the first non-whitespace character in \a s.
 *
 * @param s The null-terminated line to use.
 * @param indent_left A pointer to receive the amount of indentation (with tabs
 * converted to equivalent spaces).
 * @return Returns a pointer to the first non-whitespace character.
 */
static char* first_nws( char *s, md_indent_t *indent_left ) {
  assert( s );
  assert( indent_left );

  *indent_left = 0;
  for ( size_t tab_pos = 0; *s; ++s, ++tab_pos ) {
    switch ( *s ) {
      case '\t':
        *indent_left += TAB_SPACES_MARKDOWN - tab_pos % TAB_SPACES_MARKDOWN;
        break;
      case '\r':
      case '\n':
        break;
      case ' ':
        ++*indent_left;
        break;
      default:
        goto done;
    } // switch
  } // for

done:
  return s;
}

/**
 * Checks whether the given string is a URI scheme followed by a ':'.
 *
 * See also:
 *  + RFC 3986: "Uniform Resource Identifier (URI): Generic Syntax," Section
 *    3.1, "Scheme," Network Working Group of the Internet Engineering Task
 *    Force, January 2005.
 *
 * @param s The null-terminated string to check.
 * @return Returns a pointer within \a s just after the ':' if \a s is a URI
 * scheme or null otherwise.
 */
static char const* is_uri_scheme( char const *s ) {
  assert( s );
  if ( isalpha( *s ) ) {                // must be ^[a-zA-Z][a-zA-Z0-9.+-]*
    while ( *++s ) {
      if ( *s == ':' )
        return s + 1;
      if ( !(isalnum( *s ) || *s == '.' || *s == '+' || *s == '-') )
        return NULL;
    } // while
  }
  return NULL;
}

/**
 * Markdown allows 3, but prefers 4, spaces per indent.  Given an indent, gets
 * its preferred divisor.  For example, for \a indent values of 3, 6, 7, or 9
 * spaces, returns 3; for value of 4, 5, 8, or 12, returns 4.
 *
 * As a special case, we also allow 2 spaces per indent for definition and
 * unordered lists.
 *
 * @param indent_left The raw indent (in spaces).
 * @return Returns the preferred divisor.
 */
static md_indent_t md_indent_divisor( md_indent_t indent_left ) {
  md_line_t const line_type = md_nested_within();
  bool const dl_or_ul = line_type == MD_DL || line_type == MD_UL;
  md_indent_t const mod_a =            indent_left % MD_LIST_INDENT_MAX     ;
  md_indent_t const mod_b =            indent_left % MD_OL_INDENT_MIN       ;
  md_indent_t const mod_c = dl_or_ul ? indent_left % MD_DL_UL_INDENT_MIN : 9;
  return mod_a <= mod_b ?
    (mod_a <= mod_c ? MD_LIST_INDENT_MAX : MD_DL_UL_INDENT_MIN) :
    (mod_b <= mod_c ? MD_OL_INDENT_MIN   : MD_DL_UL_INDENT_MIN);
}

/**
 * Checks whether the line is a Markdown atx header line, a sequence of one to
 * six \c # characters starting in column 1 and followed by a whitespace
 * character.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if it is.
 */
static bool md_is_atx_header( char const *s ) {
  assert( s );
  assert( s[0] == '#' );

  size_t n_atx = 0;
  for ( ; *s == '#'; ++s ) {
    if ( ++n_atx > MD_ATX_CHAR_MAX )
      return false;
  } // for
  return n_atx > 0 && isspace( *s );
}

/**
 * Checks whether the line is a PHP Markdown Extra code fence, a series of 3 or
 * more \c ~ or \c ` characters.
 *
 * @param s The null-terminated line to check.
 * @param fence A pointer to the \c struct containing the fence info: if 0,
 * return new fence info; otherwise checks to see if \a s matches the existing
 * fence info.
 * @return Returns \c true only if it is.
 */
static bool md_is_code_fence( char const *s, md_code_fence_t *fence ) {
  assert( s );
  assert( fence );
  assert( s[0] == '~' || s[0] == '`' );
  assert(
    (!fence->cf_c && !fence->cf_len) ||
    ( fence->cf_c &&  fence->cf_len >= MD_CODE_FENCE_CHAR_MIN)
  );

  char const c = fence->cf_c ? fence->cf_c : s[0];
  size_t len = 0;

  for ( ; *s == c; ++s, ++len )
    /* empty */;

  if ( fence->cf_len )
    return len >= fence->cf_len;
  if ( len >= MD_CODE_FENCE_CHAR_MIN ) {
    fence->cf_c   = c;
    fence->cf_len = len;
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a PHP Markdown Extra definition list item: a :
 * followed by whitespace.
 *
 * @param s The null-terminated line to check.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns \c true only if \a s is a PHP Markdown Extra definition list
 * item.
 */
static bool md_is_dl( char const *s, md_indent_t *indent_hang ) {
  assert( s );
  assert( s[0] == ':' );
  return md_is_dl_ul_helper( s, indent_hang );
}

/**
 * Checks whether the line is either a Markdown unordered list item or a PHP
 * Markdown Extra definition list item.
 *
 * @param s The null-terminated line to check.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns \c true only if \a s is a PHP Markdown Extra definition list
 * item.
 */
static bool md_is_dl_ul_helper( char const *s, md_indent_t *indent_hang ) {
  assert( indent_hang );
  if ( is_space( s[1] ) ) {
    if ( s[1] == '\t' )
      *indent_hang = MD_LIST_INDENT_MAX;
    else {
      *indent_hang = MD_DL_UL_INDENT_MIN;
      if ( is_space( s[2] ) ) {
        ++*indent_hang;
        if ( is_space( s[3] ) )
          ++*indent_hang;
      }
    }
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a PHP Markdown Extra footnote definition.
 *
 * @param s The null-terminated line to check.
 * @param def_text A pointer to the variable to receive whether the footnote
 * definition line contains any text other than the marker.
 * @return Returns \c true onlf if \a s is a PHP Markdown Extra footnote
 * definition.
 */
static bool md_is_footnote_def( char const *s, bool *def_text ) {
  assert( s );
  assert( s[0] == '[' );
  assert( def_text );

  if ( *++s == '^' ) {
    while ( *++s ) {
      if ( *s == ']' ) {
        if ( !(*++s == ':' && isspace( *++s )) )
          break;
        SKIP_CHARS( s, WS_STR );
        *def_text = *s && !isspace( *s );
        return true;
      }
    } // while
  }
  return false;
}

/**
 * Checks whether the line is a Markdown horizontal rule, a series of 3 or more
 * \c *, \c -, or \c _ characters that may optionally be separated by
 * whitespace.
 *
 * @param s The null-terminated line to check where \a s[0] is used as the rule
 * character to match.
 * @return Returns \c true only if it is.
 */
static bool md_is_hr( char const *s ) {
  assert( s );
  assert( s[0] == '*' || s[0] == '-' || s[0] == '_' );

  size_t n_hr = 0;
  for ( char const hr = *s; *s; ++s ) {
    if ( !isspace( *s ) ) {
      if ( *s != hr )
        return false;
      ++n_hr;
    }
  } // for

  return n_hr >= MD_HR_CHAR_MIN;
}

/**
 * Checks whether the line is a PHP Markdown Extra HTML abbreviation.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if the line is a PHP Markdown Extra HTML
 * abbreviation.
 */
static bool md_is_html_abbr( char const *s ) {
  assert( s );
  assert( s[0] == '*' );

  if ( *++s == '[' ) {
    while ( *++s ) {
      switch ( *s ) {
        case '\\':
          ++s;
          break;
        case ']':
          return *++s == ':';
      } // switch
    } // while
  }
  return false;
}

/**
 * Checks whether the line is a block-level HTML element.
 *
 * @param s The null-terminated line to check.
 * @param is_end_tag A pointer to the variable to receive whether the HTML tag
 * is an end tag.
 * @return Returns the HTML element only if the line is a block-level HTML
 * element or NULL if not.
 */
static char const* md_is_html_block( char const *s, bool *is_end_tag ) {
  assert( s );
  assert( s[0] == '<' );

  if ( !*++s )
    return false;
  if ( (*is_end_tag = s[0] == '/') )
    ++s;

  static char element_buf[ HTML_ELEMENT_CHAR_MAX + 1/*null*/ ];
  size_t element_len = 0;

  for ( ; is_html_element_char( *s ) && element_len < sizeof element_buf - 1;
        ++element_len, ++s ) {
    element_buf[ element_len ] = tolower( *s );
  } // for
  element_buf[ element_len ] = '\0';

  //
  // We check to see if the element is on a line by itself. If it isn't, assume
  // it's a span-level element at the beginning of a line, e.g.:
  //
  //      <em>span</em>
  //
  while ( *s && *s++ != '>' )           // skip until after closing '>'
    /* empty */;
  return is_blank_line( s ) ? element_buf : NULL;
}

/**
 * Checks whether the line is a Markdown link label.
 *
 * @param s The null-terminated line to check.
 * @param has_title A pointer to the variable to receive whether the link label
 * has a title attribute on the same line.
 * @return Returns \c true only if \a s is a Markdown link label.
 */
static bool md_is_link_label( char const *s, bool *has_title ) {
  assert( s );
  assert( s[0] == '[' );
  assert( has_title );

  while ( *++s ) {
    if ( *s == ']' ) {
      if ( !(*++s == ':' && is_space( *++s )) )
        break;
      SKIP_CHARS( s, WS_STR );
      if ( *s == '<' )
        ++s;
      //
      // We only check whether the URI scheme is valid and, if it is, that's
      // good enough and we don't bother validating the syntax of the URI.
      //
      if ( !(s = is_uri_scheme( s )) )
        break;
      s += strcspn( s, " \t\r\n" );
      *has_title = false;
      if ( !is_blank_line( s ) ) {
        SKIP_CHARS( s, WS_STR );
        if ( !md_is_link_title( s ) )
          break;
        *has_title = true;
      }
      return true;
    }
  } // while

  return false;
}

/**
 * Checks whether the line is a Markdown ordered list item: a sequence of
 * digits followed by a period and whitespace.
 *
 * @param s The null-terminated line to check.
 * @param ol_num A pointer to the variable to receive the ordered list number.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns \c true only if \a s is a Markdown ordered list item.
 */
static bool md_is_ol( char const *s, md_ol_t *ol_num,
                      md_indent_t *indent_hang ) {
  assert( s );
  assert( isdigit( s[0] ) );
  assert( ol_num );
  assert( indent_hang );

  *ol_num = 0;
  char const *d = s;
  for ( ; isdigit( *d ); ++d )
    *ol_num = *ol_num * 10 + (*d - '0');

  size_t const len = d - s;
  if ( len >= 1 && len <= MD_OL_CHAR_MAX && md_is_ol_sep_char( d[0] ) &&
       is_space( d[1] ) ) {
    *indent_hang = d[1] == '\t' ?
      MD_LIST_INDENT_MAX :
      MD_OL_INDENT_MIN + is_space( d[2] );
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a PHP Markdown Extra table line.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if \a s is a PHP Markdown Extra table line.
 */
static bool md_is_table( char const *s ) {
  assert( s );
  bool nws = false;                     // encountered non-whitespace?

  for ( ; *s; ++s ) {
    switch ( *s ) {
      case '\\':
        ++s;
        break;
      case '|':
        if ( nws ) {
          //
          // Insist on encountering at least one non-whitespace character on
          // the line before returning that it's a table.
          //
          return true;
        }
        break;
      default:
        if ( !isspace( *s ) )
          nws = true;
    } // switch
  } // for
  return false;
}

/**
 * Checks whether the line is a Markdown unordered list item: a *, -, or +
 * followed by whitespace.
 *
 * @param s The null-terminated line to check.
 * @param indent_hang A pointer to the variable to receive the relative hang
 * indent (in spaces).
 * @return Returns \c true only if \a s is a Markdown unordered list item.
 */
static bool md_is_ul( char const *s, md_indent_t *indent_hang ) {
  assert( s );
  assert( s[0] == '*' || s[0] == '-' || s[0] == '+' );
  return md_is_dl_ul_helper( s, indent_hang );
}

/**
 * Checks whether the line is a Markdown Setext header line, a sequence of one
 * or more \c = or \c - characters starting in column 1.
 *
 * @param s The null-terminated line to check where \a s[0] is used as the
 * header character to match.
 * @return Returns \c true onlt if it is.
 */
static bool md_is_Setext_header( char const *s ) {
  assert( s );
  assert( s[0] == '=' || s[0] == '-' );

  for ( char const c = *s; *s && !is_eol( *s ); ++s ) {
    if ( *s != c )
      return is_blank_line( s );
  } // for
  return true;
}

/**
 * Checks the innermost enclosing nestable line type, if any.
 *
 * @return Returns said line true or MD_NONE if none.
 */
static md_line_t md_nested_within( void ) {
  for ( stack_pos_t pos = stack_top; pos >= 0; --pos ) {
    md_line_t const line_type = STACK(pos).line_type;
    if ( md_is_nestable( line_type ) )
      return line_type;
  } // for
  return MD_NONE;
}

/**
 * Correctly numbers a Markdown ordered list by replacing whatever number is
 * present with \a new_n.
 *
 * @param s The null-terminated line starting with a Markdown ordered list
 * where \a s[0] is assumed to be a digit.  The line is renumbered in-place.
 * @param old_n The old nunber.
 * @param new_n The new number.
 */
static void md_renumber_ol( char *s, md_ol_t old_n, md_ol_t new_n ) {
  assert( s );
  if ( new_n != old_n ) {
    size_t const s_len = strlen( s );
    size_t const old_digits = 1 + (old_n > 9) + (old_n > 99);

    // convert new_n to a string
    char new_buf[11];                   // enough for sizeof(md_ol_t) == 4
    snprintf( new_buf, sizeof new_buf, "%u", new_n );
    size_t const new_digits = strlen( new_buf );

    // ensure there's the exact right amount of space for new_n
    if ( old_digits < new_digits )
      memmove( s+2, s+1, s_len - old_digits + 1 );
    else if ( old_digits > new_digits )
      memmove( s+1, s+2, s_len - old_digits + 1 );

    // copy new_n into place
    memcpy( s, new_buf, new_digits );
  }
}

/**
 * Pushes a new Markdown state onto the stack.
 *
 * @param line_type The type of line.
 * @param indent_left The left indent (in spaces).
 * @param indent_hang The indent relative to \a indent_left for a hang-indent.
 */
static void stack_push( md_line_t line_type, md_indent_t indent_left,
                        md_indent_t indent_hang ) {
  static size_t stack_capacity = 0;

  MD_DEBUG(
    "%s(): T=%c L=%u H=%u\n",
    __func__, line_type, indent_left, indent_hang
  );

  ++stack_top;
  if ( !stack_capacity ) {
    stack_capacity = STATE_ALLOC_DEFAULT;
    stack = MALLOC( md_state_t, stack_capacity );
  } else if ( (size_t)stack_top >= stack_capacity ) {
    stack_capacity += STATE_ALLOC_INCREMENT;
    REALLOC( stack, md_state_t, stack_capacity );
  }
  if ( !stack )
    PERROR_EXIT( EX_OSERR );

  md_state_t *const top = &TOP;
  top->line_type   = line_type;
  top->seq_num     = ++next_seq_num;
  top->depth       = stack_size() ? stack_size() - 1 : 0;
  top->indent_left = indent_left;
  top->indent_hang = indent_hang;
  top->ol_num      = 0;
}

////////// extern functions ///////////////////////////////////////////////////

void markdown_cleanup( void ) {
  free( stack );
}

md_state_t const* markdown_parse( char *s ) {
  assert( s );

  if ( stack_empty() ) {
    //
    // Initialize the stack so that it always contains at least one element.
    //
    stack_push( MD_TEXT, 0, 0 );
  }

  md_indent_t indent_left;
  char *const nws = first_nws( s, &indent_left );

  static md_code_fence_t  code_fence;
  static md_depth_t       html_depth;   // how many nested outer elements
  static char             html_outer_element[ HTML_ELEMENT_CHAR_MAX + 1 ];

  PREV_BOOL( code_fence_end, false );
  PREV_BOOL( link_label_has_title, false );

  switch ( TOP.line_type ) {
    case MD_CODE:
      //
      // Check to see whether we've hit the end of a PHP Markdown Extra code
      // fence.
      //
      if ( code_fence_end )
        stack_pop();
      else if ( code_fence.cf_c ) {
        //
        // If code_fence.cf_c is set, that distinguishes a code fence from
        // indented code.
        //
        if ( md_is_code_fence_end( s, &code_fence ) )
          prev_code_fence_end = true;
        //
        // As long as we're in the MD_CODE state, we can just return without
        // further checks.
        //
        return &TOP;
      }
      break;

    case MD_HEADER_ATX:
    case MD_HEADER_LINE:
    case MD_HR:
    case MD_HTML_ABBR:
      //
      // These tokens are "one-shot," i.e., they never span multipe lines, so
      // pop them off the stack.
      //
      stack_pop();
      break;

    case MD_HTML_BLOCK:
      //
      // If html_depth is zero, pop the MD_HTML_BLOCK off the stack.  The
      // reason we don't pop the stack as soon as it goes to zero (below) is
      // because we still want to return the last HTML line as MD_HTML_BLOCK.
      //
      if ( !html_depth )
        stack_pop();
      break;

    case MD_LINK_LABEL:
      //
      // Markdown link label title attributes.
      //
      if ( !link_label_has_title && md_is_link_title( nws ) )
        return &TOP;
      stack_pop();
      break;

    case MD_TABLE:
      //
      // PHP Markdown Extra table.
      //
      if ( md_is_table( s ) ) {
        //
        // As long as we're in the MD_TABLE state and lines continue to be
        // table lines, we can just return without further checks.
        //
        return &TOP;
      }
      stack_pop();
      break;

    default:
      /* suppress warning */;
  } // switch

  //
  // While blank lines don't change state directly, we do have to keep track of
  // when we've seen one because:
  //
  //  + They help determine whether the line after the blank line is a
  //    continuation of the current state based on indentation.
  //  + They disambiguate "---" between a Setext 2nd-level header (that has to
  //    have a text line before it) and a horizontal rule (that doesn't).
  //
  // We have to start out prev_blank_line = true because if a "---" occurs as
  // the first line, there is no text line before it so it must be a horizontal
  // rule.
  //
  PREV_BOOL( blank_line, true );
  if ( !nws[0] ) {
    prev_blank_line = true;
    return &TOP;
  }

  if ( !top_is( MD_HTML_BLOCK ) ) {
    //
    // Markdown code blocks.
    //
    md_indent_t const code_indent_min = md_code_indent_min();
    if ( indent_left >= code_indent_min ) {
      if ( !top_is( MD_CODE ) )
        stack_push( MD_CODE, code_indent_min, 0 );
      //
      // As long as we're in the MD_CODE state, we can just return without
      // further checks.
      //
      return &TOP;
    }
  }

  //
  // Markdown that must not occur with indentation.
  //
  switch ( s[0] ) {
    // atx headers.
    case '#':
      if ( md_is_atx_header( s ) )
        CLEAR_RETURN( MD_HEADER_ATX );
      break;

    // Setext headers.
    case '=':
    case '-':
      if ( !blank_line && md_is_Setext_header( s ) )
        CLEAR_RETURN( MD_HEADER_LINE );
      break;

    // PHP Markdown Extra code fences.
    case '~':
    case '`':
      md_code_fence_init( &code_fence );
      if ( md_is_code_fence( s, &code_fence ) )
        CLEAR_RETURN( MD_CODE );
      break;
  } // switch

  //
  // Markdown that may occur with indentation.
  //
  switch ( nws[0] ) {

    // PHP Markdown Extra abbreviations.
    case '*':
      if ( md_is_html_abbr( nws ) )
        CLEAR_RETURN( MD_HTML_ABBR );
      // no break;

    // Markdown horizontal rules.
    case '-':
    case '_':
      if ( md_is_hr( nws ) )
        CLEAR_RETURN( MD_HR );
      break;

    // Block-level HTML.
    case '<': {
      bool is_end_tag;
      char const *const html_element = md_is_html_block( nws, &is_end_tag );
      if ( html_element ) {
        if ( top_is( MD_HTML_BLOCK ) ) {
          assert( html_depth > 0 );
          if ( strcmp( html_element, html_outer_element ) == 0 )
            html_depth += is_end_tag ? -1 : 1;
        } else {
          stack_push( MD_HTML_BLOCK, indent_left, 0 );
          if ( !is_end_tag ) {
            //
            // We have to count and balance only nested elements that are the
            // same as the outermost element.
            //
            strcpy( html_outer_element, html_element );
            ++html_depth;
          }
        }
      }
      break;
    }

    // Markdown link labels or PHP Markdown Extra footnote definitions.
    case '[':
      if ( indent_left <= MD_LINK_INDENT_MAX ) {
        bool fn_def_text;
        if ( md_is_footnote_def( nws, &fn_def_text ) ) {
          stack_clear();
          stack_push( MD_FOOTNOTE_DEF, 0, MD_FOOTNOTE_INDENT );
          TOP.fn_def_text = fn_def_text;
          return &TOP;
        }
        if ( md_is_link_label( nws, &prev_link_label_has_title ) )
          CLEAR_RETURN( MD_LINK_LABEL );
      }
      break;
  } // switch

  if ( top_is( MD_HTML_BLOCK ) ) {
    //
    // As long as we're in the MD_HTML_BLOCK state, we can just return without
    // further checks.
    //
    return &TOP;
  }

  /////////////////////////////////////////////////////////////////////////////

  md_line_t   curr_line_type = MD_NONE;
  md_indent_t indent_hang = 0;
  md_ol_t     ol_num = 0;

  //
  // We first have to determine the type of the current line because it affects
  // the depth calculation.
  //
  switch ( nws[0] ) {

    // Ordered lists.
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      if ( md_is_ol( nws, &ol_num, &indent_hang ) )
        curr_line_type = MD_OL;
      break;

    // Unordered lists.
    case '*':
    case '+':
    case '-':
      if ( md_is_ul( nws, &indent_hang ) )
        curr_line_type = MD_UL;
      break;

    // Definition lists.
    case ':':
      if ( md_is_dl( nws, &indent_hang ) )
        curr_line_type = MD_DL;
  } // switch

  //
  // Based on the indent, previous, and current line types, calculate the depth
  // of the current line.
  //
  md_depth_t depth = indent_left / md_indent_divisor( indent_left );
  if ( (!blank_line && md_is_nestable( TOP.line_type )) ||
       md_is_nestable( curr_line_type ) ) {
    ++depth;
  }

  //
  // Pop states for decreases in indentation.
  //
  MD_DEBUG( "pop stack? D=%u TOP.D=%u\n", depth, TOP.depth );
  while ( depth < TOP.depth ) {
    MD_DEBUG( "+ D=%u < TOP.D=%u => ", depth, TOP.depth );
    stack_pop();
  } // while

  md_indent_t const nested_indent_min = TOP.depth * MD_LIST_INDENT_MAX;

  switch ( curr_line_type ) {

    case MD_NONE:
      if ( blank_line && md_is_table( s ) ) {
        assert( !top_is( MD_TABLE ) );
        if ( indent_left >= nested_indent_min )
          stack_push( MD_TABLE, indent_left, 0 );
      }
      break;

    case MD_OL:
      if ( !top_is( MD_OL ) || indent_left >= nested_indent_min ) {
        stack_push( MD_OL, indent_left, indent_hang );
        TOP.ol_num = ol_num;
      } else {
        TOP.seq_num = ++next_seq_num;
        if ( TOP.ol_num == 9 )
          ++TOP.indent_hang;
        md_renumber_ol( nws, ol_num, ++TOP.ol_num );
      }
      break;

    case MD_DL:
    case MD_UL:
      if ( !top_is( curr_line_type ) || indent_left >= nested_indent_min )
        stack_push( curr_line_type, indent_left, indent_hang );
      else
        TOP.seq_num = ++next_seq_num;
      break;

    default:
      /* suppress warning */;
  } // switch

  return &TOP;
}

///////////////////////////////////////////////////////////////////////////////
/* vim:set et sw=2 ts=2: */
