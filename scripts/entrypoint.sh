#!/bin/sh -l

ret="$(make -s test 2>/dev/null)"

echo "$ret"

if [ "$(echo "$ret" | grep 'not ok')" != "" ]; then
  exit 1
fi

exit $?
