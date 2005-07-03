/*
**      wrap -- text reformatter
**      c_compat.h -- Macros to aid in writing K&R C, ANSI C, and C++
**
**      Copyright (C) 1996-2005  Paul J. Lucas
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
**      along with this program; if not, write to the Free Software
**      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef C_compat_H
#define C_compat_H

#if !defined(__STDC__) && !defined(__cplusplus)
#   define KNR_C                        /* K&R "classic" C */
#endif

/*****************************************************************************
 *
 *      Macros to deal with function prototypes and definitions.
 *
 *      Example:
 *
 *          // foo.h
 *          void foo PJL_PROTO(( int a, int b, int c ));
 *
 *          // foo.c
 *          void foo PJL_ARG_LIST(( a, b, c ))
 *              PJL_ARG_DEF( int a )
 *              PJL_ARG_DEF( int b )
 *              PJL_ARG_END( int c )
 *          {
 *              // body of foo
 *          }
 *
 *      Note double parentheses in PJL_PROTO and PJL_ARG_LIST.
 *
 *****************************************************************************/

#ifdef KNR_C

#   define PJL_PROTO(args)      ()      /* throw away prototype */
#   define PJL_ARG_LIST(args)   args    /* leave intact */
#   define PJL_ARG_DEF(arg)     arg ;   /* append ; */
#   define PJL_ARG_END(arg)     arg ;   /* same as above in K&R C */

#else

#   define PJL_PROTO(args)      args    /* leave intact */
#   define PJL_ARG_LIST(args)   (       /* open ( -- throw away list */
#   define PJL_ARG_DEF(arg)     arg ,   /* append , */
#   define PJL_ARG_END(arg)     arg )   /* last argument -- close ) */

#endif

/*****************************************************************************
 *
 *      Macros to deal with bool, true, and false.
 *
 *****************************************************************************/

#ifndef __cplusplus

#   ifdef bool
#       undef bool
#   endif
#   ifdef true
#       undef true
#   endif
#   ifdef false
#       undef false
#   endif
#   define bool     int
#   define true     1
#   define false    0

#endif

/*****************************************************************************
 *
 *  Macros to deal with const and inline.
 *
 *****************************************************************************/

#ifdef KNR_C

#   define const                        /* throw away const */
#   define inline                       /* throw away inline */

#endif

#endif  /* C_compat_H */
/* vim:set et sw=4 ts=4: */
