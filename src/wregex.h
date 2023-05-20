/*
**      wrap -- text reformatter
**      src/wregex.h
**
**      Copyright (C) 2017  Paul J. Lucas
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

#ifndef wrap_wregex_H
#define wrap_wregex_H

/**
 * @file
 * Contains macros for e-mail and URI regular expressions as well as a wrapper
 * API around POSIX regex (that could easily be swapped out for, say, PCRE2).
 */

// local
#include "pjl_config.h"                 /* must go first */

// standard
#include <regex.h>
#include <stdbool.h>
#include <stddef.h>                     /* for size_t */

///////////////////////////////////////////////////////////////////////////////

// general
#define RE_ALNUM    "[:alnum:]"         /* matches Unicode letters */
#define RE_DOMAIN   RE_HOST "(" "\\." RE_HOST ")*" RE_TLD
#define RE_HOST     "[" RE_ALNUM "]" "([" RE_ALNUM "-]{0,61}" "[" RE_ALNUM "])?"
#define RE_TLD      "\\." "[" RE_ALNUM "]{2,63}"

// regular expressions for any URI (RFC 3986)
#define RE_PCT_ENC  "%(" "[[:xdigit:]]{2}" "|" "%" ")"
#define RE_PORT     "[1-9][0-9]{1,4}"
#define RE_SUBDELIM "!$&'()*+,;="
#define RE_UNRESERV RE_ALNUM "._~-"
#define RE_USER     "(" "[" RE_SUBDELIM ":" RE_UNRESERV "]" "|" RE_PCT_ENC ")+"

// regular expressions for an e-mail address (RFC 5322)
#define RE_ATEXT    RE_ALNUM "!#$%&'*+/=?^_`{|}~-"
#define RE_DOT_ATOM "[" RE_ATEXT "]" "([" "\\." RE_ATEXT "]*" "[" RE_ATEXT "])?"
#define RE_LOCAL    RE_DOT_ATOM

// regular expressions for file/FTP/HTTP URIs (RFC 3986)
#define RE_FILEAUTH "//" "(" RE_USER "@" ")?" "(" RE_HOST ")?"
#define RE_FRAGMENT "#" RE_Q_OR_F "*"
#define RE_HOSTPORT RE_DOMAIN "(:" RE_PORT ")?"
#define RE_PATH     "/" RE_PCHAR "*"
#define RE_PCHAR    "([" RE_SUBDELIM "/:@" RE_UNRESERV "]" "|" RE_PCT_ENC ")"
#define RE_QUERY    "\\?" RE_Q_OR_F "*"
#define RE_Q_OR_F   "([" RE_SUBDELIM "/:?@" RE_UNRESERV "]" "|" RE_PCT_ENC ")"

/**
 * Regular expression for an e-mail address.  Note that this regular expression
 * isn't complete in that it doesn't match a quoted string for the local-part
 * nor an IP address for the domain, but it will work for most e-mail
 * addresses.
 *
 * @sa [RFC 5322: Internet Message Format, Section 3.4.1, Addr-Spec
 * Specification](https://datatracker.ietf.org/doc/html/rfc5322#section-3.4.1)
 */
#define WRAP_RE_EMAIL             "(mailto:)?" RE_LOCAL "@" RE_DOMAIN

/**
 * Regular expression for a file URI.
 *
 * @sa [RFC 8089: The "file" URI Scheme](https://datatracker.ietf.org/doc/html/rfc8089)
 */
#define WRAP_RE_FILE_URI          "file:" "(" RE_FILEAUTH ")?" RE_PATH

/**
 * Regular expression for an FTP URI.
 *
 * @sa [RFC 3986: Uniform Resource Identifier (URI): Generic Syntax](https://datatracker.ietf.org/doc/html/rfc3986)
 */
#define WRAP_RE_FTP_URI           \
  "ftp://"                        \
  "(" RE_USER "@" ")?"            \
  RE_HOSTPORT                     \
  "(" RE_PATH ")?"

/**
 * Regular expression for an HTTP URI.
 *
 * @sa [RFC 3986: Uniform Resource Identifier (URI): Generic Syntax](https://datatracker.ietf.org/doc/html/rfc3986)
 */
#define WRAP_RE_HTTP_URI          \
  "https?://"                     \
  "(" RE_USER "@" ")?"            \
  RE_HOSTPORT                     \
  "(" RE_PATH ")?"                \
  "(" RE_QUERY ")?"               \
  "(" RE_FRAGMENT ")?"

/**
 * Regular expression for all URIs not to wrap at hyphens.
 */
#define WRAP_RE                   \
  "(" WRAP_RE_EMAIL ")"     "|"   \
  "(" WRAP_RE_FILE_URI ")"  "|"   \
  "(" WRAP_RE_FTP_URI ")"   "|"   \
  "(" WRAP_RE_HTTP_URI ")"

typedef regex_t wregex_t;

///////////////////////////////////////////////////////////////////////////////

/**
 * Compiles a regular expression pattern.
 *
 * @param re A pointer to the wregex_t to compile to.
 * @param pattern The regular expression pattern to compile.
 * @return Returns 0 on success or non-zero for an invalid \a pattern.
 */
NODISCARD
int regex_compile( wregex_t *re, char const *pattern );

/**
 * Gets the regular expression error message corresponding to \a err_code.
 *
 * @param re The wregex_t involved in the error.
 * @param err_code The error code.
 * @return Returns a pointer to a static buffer containing the error message.
 */
NODISCARD
char const* regex_error( wregex_t *re, int err_code );

/**
 * Frees all memory used by a wregex_t.
 *
 * @param re A pointer to the wregex_t to free.
 */
void regex_free( wregex_t *re );

/**
 * Attempts to match \a s against the previously compiled regular expression
 * pattern in \a re.
 *
 * @param re A pointer to the wregex_t to match against.
 * @param s The string to match.
 * @param offset The offset into \a s to start.
 * @param range A pointer to an array of size 2 to receive the beginning
 * position and one past the end position of the match -- set only if not NULL
 * and there was a match.
 * @return Returns `true` only if there was a match.
 */
NODISCARD
bool regex_match( wregex_t *re, char const *s, size_t offset,
                  size_t *range );

///////////////////////////////////////////////////////////////////////////////

#endif /* wrap_wregex_H */
/* vim:set et sw=2 ts=2: */
