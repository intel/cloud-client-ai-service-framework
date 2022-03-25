#!/bin/bash

set -ex

if [ $# != 1 ]; then
    echo "usage ./install.sh DESTDIR"
    exit 1
fi

DESTDIR=$1
echo $DESTDIR

SCRIPT_DIR=$(cd $(dirname $0) >/dev/null 2>&1; pwd -P)
echo $SCRIPT_DIR

mkdir -p $DESTDIR/etc/pam.d
cp -f $SCRIPT_DIR/http $DESTDIR/etc/pam.d/
