#!/bin/bash

set -e

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

DESTDIR=$1

# runit service
mkdir -p $DESTDIR/etc/sv
cp -a ./lighttpd $DESTDIR/etc/sv
mkdir -p $DESTDIR/etc/runit/runsvdir/default

# enable lighttpd by default
ln -sf /etc/sv/lighttpd $DESTDIR/etc/runit/runsvdir/default/lighttpd
