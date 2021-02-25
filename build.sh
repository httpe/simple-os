#!/bin/sh
set -e
. ./headers.sh

if [ -n "$*" ]; then
  TO_BUILD="$*"
else
  TO_BUILD=$PROJECTS
fi

for PROJECT in $TO_BUILD; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE all install)
done
