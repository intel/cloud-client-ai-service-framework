#!/bin/bash

set -e

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

DESTDIR=$1

SCRIPT_DIR=$(cd $(dirname $0) >/dev/null 2>&1; pwd -P)

if [ "x$CONFIG_RESTFUL_SERVICE" = "xy" ]; then
	pushd $SCRIPT_DIR/runit
	./install.sh $DESTDIR
	popd

	pushd $SCRIPT_DIR/fcgi
	./install.sh $DESTDIR
	popd
fi

if [ "x$CONFIG_GRPC_SERVICE" = "xy" ]; then
	pushd $SCRIPT_DIR/grpc
	./install.sh $DESTDIR
	popd
fi

pushd $SCRIPT_DIR/secure
./install.sh $DESTDIR
popd
