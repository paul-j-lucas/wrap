/*
**      wrap -- text reformatter
**      unicode.h
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

#ifndef wrap_unicode_H
#define wrap_unicode_H

// local
#include "config.h"
#include "options.h"

// standard
#include <ctype.h>
#include <inttypes.h>                   /* for uint32_t */
#include <stddef.h>                     /* for size_t */
#include <string.h>

_GL_INLINE_HEADER_BEGIN
#ifndef WRAP_UNICODE_INLINE
# define WRAP_UNICODE_INLINE _GL_INLINE
#endif /* WRAP_UNICODE_INLINE */

///////////////////////////////////////////////////////////////////////////////

/**
 * Unicode code-point.
 */
typedef uint32_t codepoint_t;

#define CP_BYTE_ORDER_MARK        0x00FEFFu
#define CP_EOF                    ((codepoint_t)EOF)
#define CP_INVALID                0x1FFFFFu
#define UTF8_CHAR_SIZE_MAX        6     /* max bytes needed for UTF-8 char */

////////// extern functions ///////////////////////////////////////////////////

/**
 * Checks whether \a cp is an ASCII character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if \a cp is an ASCII character.
 */
WRAP_UNICODE_INLINE bool cp_is_ascii( codepoint_t cp ) {
  return cp <= 127;
}

/**
 * Checks whether \a cp is a control character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if \a cp is a control character.
 */
WRAP_UNICODE_INLINE bool cp_is_control( codepoint_t cp ) {
  return cp_is_ascii( cp ) && iscntrl( (int)cp );
}

/**
 * Checks whether \a cp is an "end-of-sentence" character
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \cp true only if \a cp is an end-of-sentence character.
 */
bool cp_is_eos( codepoint_t cp );

/**
 * Checks whether \a cp is an "end-of-sentence-extender" Unicode character,
 * that is a character that extends being in the end-of-sentence state, e.g.,
 * a ')' following a '.'.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if \a cp is an end-of-sentence-extender
 * character.
 */
bool cp_is_eos_ext( codepoint_t cp );

/**
 * Checks whether the given Unicode code-point is a hyphen-like character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if \a cp is a Unicode hyphen-like character.
 */
bool cp_is_hyphen( codepoint_t cp );

/**
 * Checks whether \a cp is a "hyphen adjacent" Unicode character, that is a
 * character that can appear adjacent to a hyphen on either side.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if \a cp can appear on either side of a hyphen.
 */
WRAP_UNICODE_INLINE bool cp_is_hyphen_adjacent( codepoint_t cp ) {
  return cp_is_ascii( cp ) && isalpha( (int)cp );
}

/**
 * Checks wherher \a cp is a leading paragraph delimiter Unicode character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if \a cp is a leading paragraph delimiter code-
 * point.
 */
WRAP_UNICODE_INLINE bool cp_is_lead_para_delim( codepoint_t cp ) {
  return  opt_lead_para_delims && cp_is_ascii( cp ) &&
          strchr( opt_lead_para_delims, (int)cp ) != NULL;
}

/**
 * Checks wherher \a cp is a paragraph delimiter Unicode character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \c true only if \a cp is a paragraph delimiter character.
 */
WRAP_UNICODE_INLINE bool cp_is_para_delim( codepoint_t cp ) {
  return  opt_para_delims && cp_is_ascii( cp ) &&
          strchr( opt_para_delims, (int)cp ) != NULL;
}

/**
 * Checks whether \a cp is a space character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \a true only if \a cp is a space character.
 */
bool cp_is_space( codepoint_t cp );

/**
 * Decodes a UTF-8 encoded character into its corresponding Unicode code-point.
 * This inline version is optimized for the common case of ASCII.
 *
 * @param s A pointer to the first byte of the UTF-8 encoded character.
 * @return Returns said code-point or \c CP_INVALID if the UTF-8 byte sequence
 * is invalid.
 */
WRAP_UNICODE_INLINE codepoint_t utf8_decode( char const *s ) {
  extern codepoint_t utf8_decode_impl( char const* );
  return (unsigned char)*s <= 127 ? (codepoint_t)*s : utf8_decode_impl( s );
}

/**
 * Checks whether the given byte is not the first byte of a UTF-8 byte sequence
 * of an encoded character.
 *
 * @param c The byte to check.
 * @return Returns \c true only if the byte is not the first byte of a byte
 * sequence of a UTF-8 encoded character.
 */
WRAP_UNICODE_INLINE bool utf8_is_cont( char c ) {
  return (unsigned char)c >= 0x80 && (unsigned char)c < 0xC0;
}

/**
 * Gets the number of bytes for the UTF-8 encoding of a Unicode code-point
 * given its first byte.
 *
 * @param c The first byte of the UTF-8 encoded code-point.
 * @return Returns 1-6, or 0 if \a c is invalid.
 */
WRAP_UNICODE_INLINE size_t utf8_len( char c ) {
  extern char const UTF8_LEN_TABLE[];
  return (size_t)UTF8_LEN_TABLE[ (unsigned char)c ];
}

/**
 * Copies all of the bytes of a UTF-8 encoded Unicode character.
 *
 * @param dest A pointer to the destination.
 * @param src A pointer to the source.
 * @return Returns the number of bytes copied.
 */
WRAP_UNICODE_INLINE size_t utf8_copy_char( char *dest, char const *src ) {
  size_t const len = utf8_len( src[0] );
  memmove( dest, src, len );
  return len;
}

///////////////////////////////////////////////////////////////////////////////

_GL_INLINE_HEADER_END

#endif /* wrap_unicode_H */
/* vim:set et sw=2 ts=2: */
