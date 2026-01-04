#! /usr/bin/env bash
##
#       wrap -- text reformatter
#       test/run_test.sh
#
#       Copyright (C) 2013-2026  Paul J. Lucas
#
#       This program is free software: you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation, either version 3 of the License, or
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

# Uncomment the following line for shell tracing.
#set -x

########## Functions ##########################################################

error() {
  exit_status=$1; shift
  echo $ME: $*
  exit $exit_status
}

assert_opt_is_yes_no() {
  assert_opt_not_empty "$1" "$2"
  case "$2" in
  yes|no)
    ;;
  *)
    error 64 "\"$2\": invalid argument; must be either \"yes\" or \"no\""
    ;;
  esac
}

assert_opt_not_empty() {
  case "x$2" in
  x|x--*)
    error 64 "$1 requires an argument" ;;
  esac
}

assert_path_exists() {
  [ -e "$1" ] || error 66 "$1: file not found"
}

local_basename() {
  ##
  # Autoconf, 11.15:
  #
  # basename
  #   Not all hosts have a working basename. You can use expr instead.
  ##
  expr "//$1" : '.*/\(.*\)'
}

pass() {
  print_result PASS $TEST_NAME
  {
    echo ":test-result: PASS"
    echo ":copy-in-global-log: no"
  } > $TRS_FILE
}

fail() {
  result=$1; shift; [ -n "$result" ] || result=FAIL
  print_result $result $TEST_NAME $*
  {
    echo ":test-result: $result"
    echo ":copy-in-global-log: yes"
  } > $TRS_FILE
}

print_result() {
  result=$1; shift
  COLOR=`eval echo \\$COLOR_$result`
  if [ "$COLOR" ]
  then echo $COLOR$result$COLOR_NONE: $*
  else echo $result: $*
  fi
}

usage() {
  [ "$1" ] && { echo "$ME: $*" >&2; usage; }
  cat >&2 <<END
usage: $ME --test-name NAME --log-file PATH --trs-file PATH [options] TEST-FILE
options:
  --collect-skipped-logs {yes|no}
  --color-tests {yes|no}
  --enable-hard-errors {yes|no}
  --expect-failure {yes|no}
END
  exit 1
}

########## Begin ##############################################################

ME=$(local_basename "$0")

[ "$BUILD_SRC" ] || {
  echo "$ME: \$BUILD_SRC not set" >&2
  exit 2
}

########## Process command-line ###############################################

while [ $# -gt 0 ]
do
  case $1 in
  --collect-skipped-logs)
    assert_opt_is_yes_no "$1" "$2"
    COLLECT_SKIPPED_LOGS=$2; shift
    ;;
  --color-tests)
    assert_opt_is_yes_no "$1" "$2"
    COLOR_TESTS=$2; shift
    ;;
  --enable-hard-errors)
    assert_opt_is_yes_no "$1" "$2"
    ENABLE_HARD_ERRORS=$2; shift
    ;;
  --expect-failure)
    assert_opt_is_yes_no "$1" "$2"
    EXPECT_FAILURE=$2; shift
    ;;
  --help)
    usage
    ;;
  --log-file)
    assert_opt_not_empty "$1" "$2"
    LOG_FILE=$2; shift
    ;;
  --test-name)
    assert_opt_not_empty "$1" "$2"
    TEST_NAME=$2; shift
    ;;
  --trs-file)
    assert_opt_not_empty "$1" "$2"
    TRS_FILE=$2; shift
    ;;
  --)
    shift
    break
    ;;
  -*)
    usage
    ;;
  *)
    break
    ;;
  esac
  shift
done

TEST=$1
[ "$TEST_NAME" ] || TEST_NAME=$TEST
[ "$LOG_FILE"  ] || usage "required --log-file not given"
[ "$TRS_FILE"  ] || usage "required --trs-file not given"
[ $# -ge 1     ] || usage "required test-file not given"

assert_path_exists "$TEST"
TEST_NAME=$(local_basename "$TEST_NAME")

########## Initialize #########################################################

if [ "$COLOR_TESTS" = yes -a -t 1 ]
then
  COLOR_BLUE="[1;34m"
  COLOR_GREEN="[0;32m"
  COLOR_LIGHT_GREEN="[1;32m"
  COLOR_MAGENTA="[0;35m"
  COLOR_NONE="[m"
  COLOR_RED="[0;31m"

  COLOR_ERROR=$COLOR_MAGENTA
  COLOR_FAIL=$COLOR_RED
  COLOR_PASS=$COLOR_GREEN
  COLOR_SKIP=$COLOR_BLUE
  COLOR_XFAIL=$COLOR_LIGHT_GREEN
  COLOR_XPASS=$COLOR_RED
fi

[ -n "$TMPDIR" ] || TMPDIR=/tmp
trap "x=$?; rm -f $TMPDIR/*_$$_* 2>/dev/null; exit $x" EXIT HUP INT TERM

##
# The automake framework sets $srcdir. If it's empty, it means this script was
# called by hand, so set it ourselves.
##
[ "$srcdir" ] || srcdir="."

DATA_DIR="$srcdir/data"
EXPECTED_DIR="$srcdir/expected"

##
# Must put BUILD_SRC first in PATH so we get the correct version of wrap/wrapc.
##
PATH="$BUILD_SRC:$PATH"

##
# Disable core dumps so we won't fill up the disk with them if a bunch of tests
# crash.
##
ulimit -c 0

########## Run test ###########################################################

run_regex_test() {
  if regex_test "$TEST" > "$LOG_FILE" 2>&1
  then pass
  else fail
  fi
}

run_wrap_test() {
  [ "$IFS" ] && IFS_old=$IFS
  IFS='|'; read COMMAND CONFIG OPTIONS INPUT EXPECTED_EXIT < $TEST
  [ "$IFS_old" ] && IFS=$IFS_old

  COMMAND=$(echo $COMMAND)              # trims whitespace
  CONFIG=$(echo $CONFIG)                # trims whitespace
  [ "$CONFIG" != /dev/null ] && CONFIG="$DATA_DIR/$CONFIG"
  INPUT=$DATA_DIR/$(echo $INPUT)        # trims whitespace
  ACTUAL_OUTPUT="$TMPDIR/wrap_stdout_$$_"
  EXPECTED_EXIT=$(echo $EXPECTED_EXIT)  # trims whitespace

  case "$TEST" in
  *crlf*) EXT=crlf ;;
  *)      EXT=txt ;;
  esac
  EXPECTED_OUTPUT="$EXPECTED_DIR/$(echo $TEST_NAME | sed s/test$/$EXT/)"

  #echo $COMMAND -c $CONFIG $OPTIONS -f $INPUT -o $ACTUAL_OUTPUT
  if $COMMAND -c"$CONFIG" $OPTIONS -f"$INPUT" -o"$ACTUAL_OUTPUT" 2> "$LOG_FILE"
  then
    if [ 0 -eq $EXPECTED_EXIT ]
    then
      if diff "$EXPECTED_OUTPUT" "$ACTUAL_OUTPUT" > "$LOG_FILE"
      then pass; mv "$ACTUAL_OUTPUT" "$LOG_FILE"
      else fail
      fi
    else
      fail
    fi
  else
    ACTUAL_EXIT=$?
    if [ $ACTUAL_EXIT -eq $EXPECTED_EXIT ]
    then
      pass
    else
      case $ACTUAL_EXIT in
      0)  fail ;;
      *)  fail ERROR ;;
      esac
    fi
  fi
}

##
# Must ensure these are unset so neither process will wait for a debugger.
##
unset WRAP_DEBUG WRAPC_DEBUG WRAPC_DEBUG_RSRW WRAPC_DEBUG_RW

case $TEST in
*.regex)  run_regex_test ;;
*.test)   run_wrap_test ;;
esac

# vim:set et sw=2 ts=2:
