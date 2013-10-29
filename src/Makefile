##
#	wrap -- text reformatter
#	Makefile
#
#	Copyright (C) 1996-2013  Paul J. Lucas
#
#	This program is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the Licence, or
#	(at your option) any later version.
# 
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
# 
#	You should have received a copy of the GNU General Public License
#	along with this program; if not, write to the Free Software
#	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##

CC=	gcc
WARN=	-Wall -Wextra -Wc++-compat -Wredundant-decls -Wwrite-strings
CFLAGS=	-ansi -pedantic-errors $(WARN) -O3

########## You shouldn't have to change anything below this line. #############

TARGET=	wrap wrapc

##
# Build rules
##
 
all: $(TARGET)

wrap: alias.o common.o config.o getopt.o pattern.o wrap.o
	$(CC) -o $@ $^

wrapc: common.o getopt.o wrapc.o
	$(CC) -o $@ $^

alias.o:	alias.c	  alias.h
config.o:	config.c  config.h  alias.h   common.h  pattern.h
common.o:	common.c  common.h
getopt.o:	getopt.c
pattern.o:	pattern.c pattern.h common.h
wrap.o:  	wrap.c    alias.h   common.h  config.h  getopt.h  pattern.h
wrapc.o: 	wrapc.c   common.h  getopt.h

##
# Utility rules
##

clean:
	rm -f *.o core*

distclean: clean
	rm -f $(TARGET)

# vim:set noet sw=8 ts=8:
