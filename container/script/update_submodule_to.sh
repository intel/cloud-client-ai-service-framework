#!/bin/bash

# Copyright (C) 2020 Intel Corporation

BRANCH=$1

if test "x$BRANCH" = "x"; then
	BRANCH=devel
fi

git submodule foreach 'git reset --hard; git clean -df'
git submodule foreach "git fetch origin; git checkout $BRANCH"
git submodule foreach "git pull origin $BRANCH"

git status
echo '*** git add submodule ***'
SUBMODULES=`git submodule  | awk '{print $2}'`
for subm in $SUBMODULES; do
	echo $subm
	git add $subm
done

echo '*** What is new in this integration ***'
git submodule foreach 'git log --oneline origin/master..HEAD'

echo '*** submodule versions ***'
git submodule
