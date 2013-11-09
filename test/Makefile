##
#	wrap -- text reformatter
#	test/Makefile
#
#	Copyright (C) 2013  Paul J. Lucas
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
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

ROOT=	..
SRC=	$(ROOT)/src
WRAP=	$(SRC)/wrap

all: test

check test tests: $(WRAP)
	@WRAP=$(WRAP) ./run_tests.sh

$(WRAP): $(SRC)/Makefile
	cd $(SRC) && make

$(SRC)/Makefile:
	cd $(ROOT) && ./configure

# automake-required targets
distdir:
install install-data install-exec uninstall:
install-dvi install-html install-info install-ps install-pdf:
check installcheck:
mostlyclean clean distclean maintainer-clean:
dvi pdf ps info html tags ctags:

# vim:set noet sw=8 ts=8:
