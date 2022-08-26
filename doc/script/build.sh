#!/bin/bash

set -xe

rm -rf dist
./script/gen_summary.py
cp -a ./src/README.md ./dist/src
cp -a ./src/media/ ./dist/src
cp -a ./src/book.json ./dist/src

# npm install -g gitbook-cli
gitbook install ./dist/src
gitbook build ./dist/src

echo "To show the documnet in the webpage, please run
gitbook serve ./dist/src"
