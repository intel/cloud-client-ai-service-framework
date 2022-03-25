#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -e

if [ $# != 1 ];then
	echo "Usage: ./set_deb_version.sh <version>"
	exit 1
fi

if [ ! -e "release_build.sh" ]; then
	echo "error: this script should run under the 'release' folder."
	exit 1
fi

version=$1

changelogs=$(find . -name "changelog")

for c in $changelogs; do
	echo $c
	cp $c ${c}.bak
	sed -i "0,/(.*)/{s/(.*)/($version)/g}" $c
	diff -Naur $c ${c}.bak || true
done
