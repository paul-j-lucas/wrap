/*
**      wrap -- text reformatter
**      src/unicode.h
**
**      Copyright (C) 1996-2025  Paul J. Lucas
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

#ifndef wrap_unicode_H
#define wrap_unicode_H

/**
 * @file
 * Contains constants and types for Unicode code-points and UTF-8 byte
 * sequences as well as functions for manipulating them.
 */

// local
#include "pjl_config.h"                 /* must go first */
#include "util.h"

/// @cond DOXYGEN_IGNORE

// standard
#include <ctype.h>
#include <inttypes.h>                   /* for uint*_t */
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */
#include <string.h>                     /* for memmove(3) */
#if HAVE_CHAR8_T || HAVE_CHAR32_T
#include <uchar.h>
#endif /* HAVE_CHAR8_T || HAVE_CHAR32_T */
#include <wctype.h>

/// @endcond

#if !HAVE_CHAR8_T
/// 8-bit character type (borrowed from C23).
typedef uint8_t char8_t;
#endif /* !HAVE_CHAR8_T */
#if !HAVE_CHAR32_T
/// 32-bit character type (borrowed from C11).
typedef uint32_t char32_t;
#endif /* !HAVE_CHAR32_T */

/**
 * @defgroup unicode-group Unicode Support
 * Defines macros, types, and functions for working with Unicode.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/// Unicode byte order mark (BOM).
#define CP_BYTE_ORDER_MARK        0x00FEFFu

/// A UTF-32 version of `EOF`.
#define CP_EOF                    ((char32_t)EOF)

/// Value for invalid Unicode code-point.
#define CP_INVALID                0x1FFFFFu

/// Max number of bytes needed for a UTF-8 character.
#define UTF8_CHAR_SIZE_MAX        6

/**
 * UTF-8 character.
 */
typedef char utf8c_t[ UTF8_CHAR_SIZE_MAX ];

////////// extern functions ///////////////////////////////////////////////////

/**
 * Checks whether \a cp is an alphabetic character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is an alphabetic character.
 *
 * @sa cp_is_ascii()
 * @sa cp_is_control()
 * @sa cp_is_space()
 */
NODISCARD
inline bool cp_is_alpha( char32_t cp ) {
  return iswalpha( STATIC_CAST( wint_t, cp ) );
}

/**
 * Checks whether \a cp is an ASCII character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is an ASCII character.
 *
 * @sa cp_is_alpha()
 * @sa cp_is_control()
 * @sa cp_is_space()
 */
NODISCARD
inline bool cp_is_ascii( char32_t cp ) {
  return cp <= 0x7F;
}

/**
 * Checks whether \a cp is a control character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is a control character.
 *
 * @sa cp_is_alpha()
 * @sa cp_is_ascii()
 * @sa cp_is_space()
 */
NODISCARD
inline bool cp_is_control( char32_t cp ) {
  return cp_is_ascii( cp ) && iscntrl( STATIC_CAST( int, cp ) );
}

/**
 * Checks whether \a cp is an "end-of-sentence" character
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is an end-of-sentence character.
 *
 * @sa cp_is_eos_ext()
 */
NODISCARD
bool cp_is_eos( char32_t cp );

/**
 * Checks whether \a cp is an "end-of-sentence-extender" Unicode character,
 * that is a character that extends being in the end-of-sentence state, e.g.,
 * a `)` following a period.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is an end-of-sentence-extender
 * character.
 *
 * @sa cp_is_eos()
 */
NODISCARD
bool cp_is_eos_ext( char32_t cp );

/**
 * Checks whether the given Unicode code-point is a hyphen-like character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp is a Unicode hyphen-like character.
 *
 * @sa cp_is_hyphen_adjacent()
 */
NODISCARD
bool cp_is_hyphen( char32_t cp );

/**
 * Checks whether \a cp is a "hyphen adjacent" Unicode character, that is a
 * character that can appear adjacent to a hyphen on either side.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns `true` only if \a cp can appear on either side of a hyphen.
 *
 * @sa cp_is_hyphen()
 */
NODISCARD
inline bool cp_is_hyphen_adjacent( char32_t cp ) {
  return cp_is_alpha( cp );
}

/**
 * Checks whether \a cp is a space character.
 *
 * @param cp The Unicode code-point to check.
 * @return Returns \a true only if \a cp is a space character.
 *
 * @sa cp_is_alpha()
 * @sa cp_is_ascii()
 * @sa cp_is_control()
 */
NODISCARD
inline bool cp_is_space( char32_t cp ) {
  return iswspace( STATIC_CAST( wint_t, cp ) );
}

/**
 * Decodes a UTF-8 encoded character into its corresponding Unicode code-point.
 *
 * @note This inline version is optimized for the common case of ASCII.
 *
 * @param s A pointer to the first byte of the UTF-8 encoded character.
 * @return Returns said code-point or \ref #CP_INVALID if the UTF-8 byte
 * sequence is invalid.
 *
 * @sa utf8_decode_impl()
 */
NODISCARD
inline char32_t utf8_decode( char const *s ) {
  extern char32_t utf8_decode_impl( char const* );
  char32_t const cp = STATIC_CAST( char8_t, *s );
  return cp_is_ascii( cp ) ? cp : utf8_decode_impl( s );
}

/**
 * Checks whether the given byte is not the first byte of a UTF-8 byte sequence
 * of an encoded character.
 *
 * @param c The byte to check.
 * @return Returns `true` only if the byte is not the first byte of a byte
 * sequence of a UTF-8 encoded character.
 *
 * @sa utf8_is_start()
 */
NODISCARD
inline bool utf8_is_cont( char c ) {
  char8_t const c8 = STATIC_CAST( char8_t, c );
  return c8 >= 0x80 && c8 < 0xC0;
}

/**
 * Checks whether the given byte is the first byte of a UTF-8 byte sequence of
 * an encoded character.
 *
 * @param c The byte to check.
 * @return Returns `true` only if the byte is the first byte of a byte sequence
 * of a UTF-8 encoded character.
 *
 * @sa utf8_is_cont()
 */
NODISCARD
inline bool utf8_is_start( char c ) {
  char8_t const c8 = STATIC_CAST( char8_t, c );
  return c8 <= 0x7F || (c8 >= 0xC2 && c8 < 0xFE);
}

/**
 * Gets the number of bytes for the UTF-8 encoding of a Unicode code-point
 * given its first byte.
 *
 * @param c The first byte of the UTF-8 encoded code-point.
 * @return Returns 1-6, or 0 if \a c is invalid.
 */
NODISCARD
inline size_t utf8_len( char c ) {
  extern char8_t const UTF8_LEN_TABLE[];
  return (size_t)UTF8_LEN_TABLE[ STATIC_CAST( char8_t, c ) ];
}

/**
 * Copies all of the bytes of a UTF-8 encoded Unicode character.
 *
 * @param dest A pointer to the destination.
 * @param src A pointer to the source.
 * @return Returns the number of bytes copied.
 */
PJL_DISCARD
inline size_t utf8_copy_char( char *dest, char const *src ) {
  size_t const len = utf8_len( src[0] );
  memmove( dest, src, len );
  return len;
}

/**
 * Given a pointer to any byte within a UTF-8 encoded string, synchronizes in
 * reverse to find the first byte of the UTF-8 character byte sequence the
 * pointer is pointing within.
 *
 * @note This inline version is optimized for the common case of ASCII.
 *
 * @param buf A pointer to the beginning of the buffer.
 * @param pos A pointer to any byte with the buffer.
 * @return Returns a pointer less than or equal to \a pos that points to the
 * first byte of a UTF-8 encoded character byte sequence or NULL if there is
 * none.
 *
 * @sa utf8_rsync_impl()
 */
NODISCARD
inline char const* utf8_rsync( char const *buf, char const *pos ) {
  extern char const* utf8_rsync_impl( char const*, char const* );
  char32_t const cp = STATIC_CAST( char8_t, *pos );
  return cp_is_ascii( cp ) ? pos : utf8_rsync_impl( buf, pos );
}

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* wrap_unicode_H */
/* vim:set et sw=2 ts=2: */
