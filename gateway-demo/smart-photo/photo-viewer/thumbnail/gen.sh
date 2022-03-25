#!/bin/bash

src_dir=$1

if [ "x$src_dir" = "x" ]; then
	src_dir="../photos.o/"
fi

for f in `ls $src_dir`; do
	echo $f
	./image_w.py "${src_dir}/$f" "crop_$f"
done


