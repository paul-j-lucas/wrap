/*
**      PJL Library
**      src/type_traits.h
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

#ifndef pjl_type_traits_H
#define pjl_type_traits_H

/**
 * @file
 * Declares utility constants, macros, and functions.
 */

// local
#include "pjl_config.h"                 /* must go first */

/// @cond DOXYGEN_IGNORE

#if HAVE___BUILTIN_TYPES_COMPATIBLE_P && HAVE_TYPEOF
# define WITH_IS_SAME_TYPE
#endif

/// @endcond

/**
 * @defgroup type-traits-group Type Traits
 * Macros for type traits.
 * @{
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * Checks (at compile-time) whether \a A is an array.
 *
 * @param A The alleged array to check.
 * @return Returns 1 (true) only if \a A is an array; 0 (false) otherwise.
 *
 * @sa https://stackoverflow.com/a/77881417/99089
 *
 * @sa #IS_POINTER_EXPR()
 */
#ifdef HAVE_TYPEOF
# define IS_ARRAY_EXPR(A)     \
    _Generic( &(A),           \
      typeof(*(A)) (*)[]: 1,  \
      default           : 0   \
    )
#else
# define IS_ARRAY_EXPR(A)         1
#endif /* HAVE_TYPEOF */

/**
 * Checks (at compile-time) whether \a P is a pointer.
 *
 * @param P The alleged pointer to check.
 * @return Returns 1 (true) only if \a P is a pointer; 0 (false) otherwise.
 *
 * @sa #IS_ARRAY_EXPR()
 */
#ifdef HAVE_TYPEOF
#define IS_POINTER_EXPR(P)      \
  _Generic( &(typeof((P))){ },  \
    typeof(*(P)) ** : 1,        \
    default         : 0         \
  )
#else
# define IS_POINTER_EXPR(P)       1
#endif /* HAVE_TYPEOF */

/**
 * Checks (at compile-time) whether \a T1 and \a T2 are the same type.
 *
 * @param T1 The first type or expression.
 * @param T2 The second type or expression.
 * @return Returns 1 (true) only if \a T1 and \a T2 are the same type; 0
 * (false) otherwise.
 */
#ifdef WITH_IS_SAME_TYPE
# define IS_SAME_TYPE(T1,T2) \
    __builtin_types_compatible_p( __typeof__(T1), __typeof__(T2) )
#endif /* WITH_IS_SAME_TYPE */

/**
 * Like C11's `_Static_assert()` except that is can be used in an expression.
 *
 * @param EXPR The expression to check.
 * @param MSG The string literal of the error message to print only if \a EXPR
 * evaluates to 0 (false).
 * @return Always returns 1.
 */
#define STATIC_ASSERT_EXPR(EXPR,MSG) \
  (!!sizeof( struct { static_assert( (EXPR), MSG ); int required; } ))

///////////////////////////////////////////////////////////////////////////////

/** @} */

#endif /* pjl_type_traits_H */
/* vim:set et sw=2 ts=2: */
