#!/bin/sh
set -eu
out="$1"
tar czf "$out/yalocc-$VERSION.tar.gz" \
    --exclude=./.git --exclude=./build --exclude=./dist --exclude=./debian \
    --transform "s,^\.,yalocc-$VERSION," .
