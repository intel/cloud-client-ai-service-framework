#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -e

head_line_number=$1
if test "x" != "x$head_line_number"; then
	head_line_number=$(expr $head_line_number + 1)
else
	head_line_number=2
fi

tail_line_number=$(expr ${head_line_number} + 1)

docker images service_runtime | head -${head_line_number}
echo ""
echo "* remove:"
docker images service_runtime | tail -n +${tail_line_number}

remove_all_flag=''
for tag in `docker images service_runtime | tail -n +${tail_line_number} \
	| awk '{print $2}'`
do
	delete_img_flag=''
	while test "x$remove_all_flag" == "x"; do
	echo ""
	read -p "Remove service_runtime:$tag [Yes/No/All]? " yn
	case $yn in
		[Nn]* )
			break
			;;
		[Yy]* )
			delete_img_flag="True"
			break
			;;
		[Aa]* )
			remove_all_flag="True"
			break
			;;
		* )
			;;
	esac
	done

	if test "x$remove_all_flag" != "x" \
		|| test "x$delete_img_flag" != "x"; then
		docker image rm service_runtime:$tag
	fi
done
