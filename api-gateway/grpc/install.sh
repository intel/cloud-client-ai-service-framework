#!/bin/bash

set -e

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

DESTDIR=$1

SCRIPT_DIR=$(cd $(dirname $0) >/dev/null 2>&1; pwd -P)

mkdir -p $SCRIPT_DIR/build
pushd $SCRIPT_DIR/build
export DESTDIR
. /opt/intel/openvino/bin/setupvars.sh
cmake ..
make VERBOSE=1 -j4 install/strip
popd

rm -rf $SCRIPT_DIR/build
