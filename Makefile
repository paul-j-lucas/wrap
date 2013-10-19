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

CC=	cc
CFLAGS=	-O3

########## You shouldn't have to change anything below this line. #############

TARGET=	wrap wrapc

##
# Build rules
##
 
all: $(TARGET)

wrap: common.o wrap.o
	$(CC) -o $@ $^

wrapc: common.o wrapc.o
	$(CC) -o $@ $^

common.o:	common.c common.h
wrap.o:  	wrap.c   common.h
wrapc.o: 	wrapc.c  common.h

##
# Utility rules
##

clean:
	rm -f *.o core*

distclean: clean
	rm -f $(TARGET)

# vim:set noet sw=8 ts=8:
