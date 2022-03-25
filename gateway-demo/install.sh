#!/bin/bash

set -e

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

DESTDIR=$1

SCRIPT_DIR=$(cd $(dirname $0) >/dev/null 2>&1; pwd -P)

HTML_ROOT=$DESTDIR/var/www/html
mkdir -p $HTML_ROOT

pushd $SCRIPT_DIR/html
cp -a * $HTML_ROOT
popd

pushd $SCRIPT_DIR/cgi-bin
./install.sh $DESTDIR
popd
