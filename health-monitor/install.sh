#!/bin/bash

set -e

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

DESTDIR=$1
SCRIPT_DIR=$(readlink -e $(dirname $0))

cd $SCRIPT_DIR/health-monitor-agent
rm -rf build

mkdir build
pushd build
cmake ..
DESTDIR=$DESTDIR make install
popd
rm -rf build
