#! /bin/sh
##
#       wrap -- text reformatter
#       test/run_tests.sh
#
#       Copyright (C) 2013  Paul J. Lucas
#
#       This program is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation; either version 2 of the Licence, or
#       (at your option) any later version.
#
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#
#       You should have received a copy of the GNU General Public License
#       along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

if [ -z "$WRAP" ]
then
  echo "$0: error: \$WRAP not set" >&2
  exit 1
fi

OUTPUT=/tmp/wrap_test_output_$$_
trap "x=$?; rm -f /tmp/*_$$_* 2>/dev/null; exit $x" EXIT HUP INT TERM

for TEST_FILE in *.test
do
  IFS='|' read CONFIG OPTIONS INPUT < $TEST_FILE
  CONFIG=`echo $CONFIG`                 # trims whitespace
  if [ "$CONFIG" != /dev/null ]
  then CONFIG="data/$CONFIG"
  fi
  INPUT="data/`echo $INPUT`"            # trims whitespace
  EXPECTED="expected/`echo $TEST_FILE | sed 's/test$/txt/'`"

  #echo $WRAP -c $CONFIG $OPTIONS -f $INPUT -o $OUTPUT
  if $WRAP -c $CONFIG $OPTIONS -f $INPUT -o $OUTPUT
  then
    if diff $OUTPUT $EXPECTED
    then echo "PASS: $TEST_FILE"
    else echo "FAIL: $TEST_FILE"
    fi
  fi
done

# vim:set et sw=2 ts=2:
