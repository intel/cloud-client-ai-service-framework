#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -xe

SCRIPT_DIR=$(dirname $(readlink -f $0))

export PYTHONPATH=$SCRIPT_DIR/../

for script in `find . -name post_*.py`; do
	echo $script
	python3 $script
done
