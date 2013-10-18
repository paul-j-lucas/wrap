/*
**      wrap -- text reformatter
**      common.h -- Common declarations.
**
**      Copyright (C) 1996-2013  Paul J. Lucas
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

#ifndef wrap_common_H
#define wrap_common_H

/*****************************************************************************/

#define DEFAULT_LINE_LENGTH 80          /* wrap text to this line length */
#define DEFAULT_TAB_SPACES  8           /* number of spaces a tab equals */
#define ERROR(E)            { perror( me ); exit( E ); }
#define LINE_BUF_SIZE       1024        /* hopefully, no one will exceed this */
#define WRAP_VERSION        "3.0"

/* exit(3) status codes */
#define EXIT_OK           0
#define EXIT_USAGE        1
#define EXIT_EXEC_ERROR   2
#define EXIT_FORK_ERROR   3
#define EXIT_OPEN_READ    4
#define EXIT_OPEN_WRITE   5
#define EXIT_PIPE_ERROR   6
#define EXIT_READ_ERROR   7
#define EXIT_WRITE_ERROR  8

#ifndef __cplusplus
# ifdef bool
#   undef bool
# endif
# ifdef true
#   undef true
# endif
# ifdef false
#   undef false
# endif
# define bool   int
# define true   1
# define false  0
#endif /* __cplusplus */

/*****************************************************************************/

#endif /* wrap_common_H */
/* vim:set et sw=2 ts=2: */
