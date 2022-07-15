#!/bin/bash

set -xe

rm -rf dist
./script/gen_summary.py
cp -a ./src/README.md ./dist/src
cp -a ./src/media/ ./dist/src
cp -a ./src/book.json ./dist/src

./node_modules/.bin/gitbook install ./dist/src
./node_modules/.bin/gitbook build ./dist/src

echo "To show the documnet in the webpage, please run
./node_modules/.bin/gitbook serve ./dist/src"
