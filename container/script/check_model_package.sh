#!/bin/bash

# Copyright (C) 2021 Intel Corporation

#set -xe

files=`ls`
install_files=`ls debian/*.install`

for f in $files; do
	echo $f
	found_in_install=0
	for inf in $install_files; do
		ii=`cat $inf | awk '{print $1}'`
		for i in $ii; do
			#echo $files | grep $i --color
			if [ "$i" = "$f" ]; then
				found_in_install=1
				break
			fi
		done
	done
	if [ "x$found_in_install" = "x0" ]; then
		echo "     MISS " $f
	fi
done

