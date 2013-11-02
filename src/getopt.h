/*
**      pjl_getopt() -- get option character from command line argument list
**      getopt.h
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
**      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef pjl_getopt_H
#define pjl_getopt_H

/*****************************************************************************/

extern int opterr;
extern int optind;
extern char *optarg;
extern int optopt;

/**
 * A version of getopt(3) that:
 *  + Allows options to have optional arguments via "x?".
 *  + Can be called more than once.
 *
 * @param argc The total number of arguments + 1.
 * @param argv The arguments.  By convention, \c argv[0] must equal the name of
 * the executable and \c argv[argc] must equal NULL.
 * @param opts The valid option letters.  Letters followed by \c ':' require an
 * argument; letters followed by \c '?' may have an argument.  Whether an
 * optional argument is present is determined by whether the following argument
 * starts with a \c '-': if it does, it's considered the \e next argument and
 * not the argument for the current option.
 * @return If there is a next option, returns the letter of said option; if
 * there is no next option, returns \c EOF; if an option letter is encountered
 * that is not in \a opts or an option that requires an argument doesn't have
 * one, returns \c '?' and sets \c optopt to the character that caused the
 * error.
 */
int pjl_getopt( int argc, char const *argv[], char const *opts );

/*****************************************************************************/

#endif /* pjl_getopt_H */
/* vim:set et sw=2 ts=2: */
