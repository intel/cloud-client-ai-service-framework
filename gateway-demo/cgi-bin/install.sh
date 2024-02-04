#!/bin/bash

set -ex

if [ $# != 1 ]; then
    echo "usage ./install.sh DESTDIR"
    exit 1
fi

DESTDIR=$1
echo $DESTDIR

SCRIPT_DIR=$(cd $(dirname $0)>/dev/null 2>&1;pwd -P)
echo $SCRIPT_DIR


# install python fcgi programs

mkdir -p $DESTDIR/opt/fcgi/cgi-bin
cp $SCRIPT_DIR/fcgi_*_qq.py $DESTDIR/opt/fcgi/cgi-bin/

## install config files
# disable service by default
mkdir -p $DESTDIR/etc/lighttpd/conf-available/
cp $SCRIPT_DIR/16-*.conf $DESTDIR/etc/lighttpd/conf-available/

