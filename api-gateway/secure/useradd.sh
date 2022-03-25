#!/bin/bash

# require: db5.3-util

dbfile="apiuser.db"

[[ $1 == '-d' ]] && dbfile=$2 && shift 2

[[ -z $1 || -z $2 ]] &&
    echo "usage: $0 [-d dbfile] username password" &&
    exit 1

{ echo $1; openssl passwd -6 "$2"; } | db5.3_load -Tt hash "$dbfile"
