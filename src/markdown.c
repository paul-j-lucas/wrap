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

typedef ssize_t stack_pos_t;

// local constant definitions
static size_t const HTML_ELEMENT_MAX   = 10;  // "blockquote"
static size_t const MD_ATX_CHAR_MAX    =  6;  // max num of # in atx header
static size_t const MD_CODE_INDENT_MIN =  4;  // min indent for code
static size_t const MD_HR_CHAR_MIN     =  3;  // min num of ***, ---, or ___
static size_t const MD_LINK_INDENT_MAX =  3;  // max indent for [id]: URI
static size_t const MD_LIST_INDENT_MAX =  4;  // max indent for all lists
static size_t const MD_OL_INDENT_MIN   =  3;  // ordered list min indent
static size_t const MD_UL_INDENT_MIN   =  2;  // unordered list min indent

static size_t const STATE_ALLOC_DEFAULT   = 5;
static size_t const STATE_ALLOC_INCREMENT = 5;

// local variable definitions
static unsigned     html_depth;         // how many nested outer elements
static md_seq_t     next_seq_num;
static char         outer_html_element[ HTML_ELEMENT_MAX + 1 ];
static md_state_t  *stack;              // global stack of states
static stack_pos_t  stack_top = -1;     // index of the top of the stack

// local functions
static bool         md_nested_within( md_line_t );

////////// inline functions ///////////////////////////////////////////////////

/**
 * Checks whether \a c is an HTML element character.
 *
 * @param c The character to check.
 * @return Returns \c true only if \a c is an HTML element character.
 */
static inline bool is_html_char( char c ) {
  return isalpha( c ) || c == '-';
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
  return (stack_size() - (top_is( MD_CODE ) ? 1 : 0)) * MD_CODE_INDENT_MIN;
}

/**
 * Checks whether \a line_type can nest.
 *
 * @param line_type The line type to check.
 * @return Returns \c true only if \a line_type can nest.
 */
static inline bool md_is_nestable( md_line_t line_type ) {
  return line_type == MD_OL || line_type == MD_UL;
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
 * Checks whether the given string is a URI (begins with a URI scheme followed
 * by a ':').
 *
 * See also:
 *  + RFC 3986: "Uniform Resource Identifier (URI): Generic Syntax," Section
 *    3.1, "Scheme," Network Working Group of the Internet Engineering Task
 *    Force, January 2005.
 *
 * @param s The null-terminated string to check.
 * @return Returns \c true only if \a s is a URI scheme.
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
 * As a special case, we also allow 2 spaces per indent for unordered lists.
 *
 * @param indent_left The raw indent (in spaces).
 * @return Returns the preferred divisor.
 */
static md_indent_t md_indent_divisor( md_indent_t indent_left ) {
  bool const within_ul = md_nested_within( MD_UL );
  md_indent_t const mod_a =             indent_left % MD_LIST_INDENT_MAX;
  md_indent_t const mod_b =             indent_left % MD_OL_INDENT_MIN  ;
  md_indent_t const mod_c = within_ul ? indent_left % MD_UL_INDENT_MIN  : 9;
  return mod_a <= mod_b ?
    (mod_a <= mod_c ? MD_LIST_INDENT_MAX : MD_UL_INDENT_MIN) :
    (mod_b <= mod_c ? MD_OL_INDENT_MIN   : MD_UL_INDENT_MIN);
}

/**
 * Checks whether the line is a Markdown atx header line, a sequence of one to
 * six \c # characters starting in colume 1.
 *
 * @param s The null-terminated line to check.
 * @return Returns \c true only if it is.
 */
static bool md_is_atx_header( char const *s ) {
  assert( s );
  assert( s[0] == '#' );

  unsigned n_atx = 0;
  for ( ; *s; ++s ) {
    if ( *s != '#' )
      break;
    if ( ++n_atx > MD_ATX_CHAR_MAX )
      return false;
  } // for
  return n_atx > 0;
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

  unsigned n_hr = 0;
  for ( char const hr = *s; *s && !is_eol( *s ); ++s ) {
    if ( !is_space( *s ) ) {
      if ( *s != hr )
        return false;
      ++n_hr;
    }
  } // for

  return n_hr >= MD_HR_CHAR_MIN;
}

/**
 * Checks whether the line is a block-level HTML element.
 *
 * @param s The null-terminated line to check.
 * @param is_end_tag A pointer to the variable to receive whether the HTML tag
 * is an end tag.
 * @return Returns \c true only if the line is a block-level HTML element.
 */
static char const* md_is_html( char const *s, bool *is_end_tag ) {
  assert( s );
  assert( s[0] == '<' );

  if ( !*++s )
    return false;
  if ( (*is_end_tag = s[0] == '/') )
    ++s;

  static char element_buf[ HTML_ELEMENT_MAX + 1 ];
  size_t element_len = 0;

  for ( ; *s && is_html_char( *s ) && element_len < sizeof element_buf - 1;
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
  return *s && is_blank_line( s ) ? element_buf : NULL;
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

  while ( *++s && !is_eol( *s ) ) {
    if ( *s == ']' ) {
      if ( !(*++s == ':' && is_space( *++s )) )
        break;
      SKIP_WS( s );
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
        SKIP_WS( s );
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
 * Checks whether the line is a Markdown ordered list element: a sequence of
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
  if ( d - s && d[0] == '.' && is_space( d[1] ) ) {
    *indent_hang = d[1] == '\t' ?
      MD_LIST_INDENT_MAX :
      MD_OL_INDENT_MIN + is_space( d[2] );
    return true;
  }
  return false;
}

/**
 * Checks whether the line is a Markdown unordered list element: a *, -, or +
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
  assert( indent_hang );

  if ( is_space( s[1] ) ) {
    if ( s[1] == '\t' )
      *indent_hang = MD_LIST_INDENT_MAX;
    else {
      *indent_hang = MD_UL_INDENT_MIN;
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
 * Checks whether the line is a Markdown Setext header line, a sequence of one
 * or more \c = or \c - characters starting in colume 1.
 *
 * @param s The null-terminated line to check where \a s[0] is used as the
 * header character to match.
 * @return Returns \c true onlt if it is.
 */
static bool md_is_Setext_header( char const *s ) {
  assert( s );
  assert( s[0] == '=' || s[0] == '-' );

  for ( char const c = *s; *s; ++s ) {
    if ( is_eol( *s ) )
      break;
    if ( *s != c )
      return false;
  } // for
  return true;
}

/**
 * Checks whether we're currently nested within \a line_type.
 *
 * @param line_type The line type to check for.
 * @return Returns \c true only if the nearest enclosing nestable type is
 * \a line_type.
 */
static bool md_nested_within( md_line_t line_type ) {
  for ( stack_pos_t pos = stack_top; pos >= 0; --pos ) {
    md_line_t const pos_line_type = STACK(pos).line_type;
    if ( md_is_nestable( pos_line_type ) )
      return pos_line_type == line_type;
  } // for
  return false;
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

  PREV_BOOL( link_label_has_title, false );

  switch ( TOP.line_type ) {
    case MD_HEADER_ATX:
    case MD_HEADER_LINE:
    case MD_HR:
      //
      // These tokens are "one-shot," i.e., they never span multipe lines, so
      // pop them off the stack.
      //
      stack_pop();
      break;

    case MD_HTML:
      //
      // If html_depth is zero, pop the MD_HTML off the stack.  The reason we
      // don't pop the stack as soon as it goes to zero (below) is because we
      // still want to return the last HTML line as MD_HTML.
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

  if ( !top_is( MD_HTML ) ) {
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
  // Markdown that must occur in column 1: header lines.
  //
  switch ( s[0] ) {
    case '#':                           // atx header
      if ( md_is_atx_header( s ) )
        CLEAR_RETURN( MD_HEADER_ATX );
      break;
    case '=':                           // Setext 1st-level header
    case '-':                           // Setext 2nd-level header
      if ( !blank_line && md_is_Setext_header( s ) )
        CLEAR_RETURN( MD_HEADER_LINE );
      break;
  } // switch

  switch ( nws[0] ) {
    case '*':
    case '-':
    case '_':
      //
      // Markdown horizontal rules.
      //
      if ( md_is_hr( nws ) )
        CLEAR_RETURN( MD_HR );
      break;

    case '<': {
      //
      // Block-level HTML.
      //
      bool is_end_tag;
      char const *const html_element = md_is_html( nws, &is_end_tag );
      if ( html_element ) {
        if ( top_is( MD_HTML ) ) {
          assert( html_depth > 0 );
          if ( strcmp( html_element, outer_html_element ) == 0 )
            html_depth += is_end_tag ? -1 : 1;
        } else {
          stack_push( MD_HTML, indent_left, 0 );
          if ( !is_end_tag ) {
            //
            // We have to count and balance only nested elements that are the
            // same as the outermost element.
            //
            strcpy( outer_html_element, html_element );
            ++html_depth;
          }
        }
      }
      break;
    }

    case '[':
      //
      // Markdown link labels.
      //
      if ( indent_left <= MD_LINK_INDENT_MAX &&
           md_is_link_label( nws, &prev_link_label_has_title ) ) {
        CLEAR_RETURN( MD_LINK_LABEL );
      }
      break;
  } // switch

  if ( top_is( MD_HTML ) ) {
    //
    // As long as we're in the MD_HTML state, we can just return without
    // further checks.
    //
    return &TOP;
  }

  /////////////////////////////////////////////////////////////////////////////

  md_line_t   curr_line_type = MD_TEXT;
  md_indent_t indent_hang;
  md_ol_t     ol_num;

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
  } // switch

  //
  // Based on the indent, previous, and current line types, calculate the depth
  // of the current line.
  //
  unsigned depth = indent_left / md_indent_divisor( indent_left );
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

  md_indent_t const nested_list_indent = TOP.depth * MD_LIST_INDENT_MAX;

  switch ( curr_line_type ) {

    case MD_OL:
      if ( !top_is( MD_OL ) || indent_left >= nested_list_indent ) {
        stack_push( MD_OL, indent_left, indent_hang );
        TOP.ol_num = ol_num;
      } else {
        TOP.seq_num = ++next_seq_num;
        if ( TOP.ol_num == 9 )
          ++TOP.indent_hang;
        md_renumber_ol( nws, ol_num, ++TOP.ol_num );
      }
      break;

    case MD_UL:
      if ( !top_is( MD_UL ) || indent_left >= nested_list_indent )
        stack_push( MD_UL, indent_left, indent_hang );
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
