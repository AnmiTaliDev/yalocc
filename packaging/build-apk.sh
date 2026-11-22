#!/bin/sh
set -eu
: "${VERSION:?}" "${URL:?}"
apk add --no-cache alpine-sdk meson
work="$PWD/apkwork"
rm -rf "$work"
mkdir -p "$work"
sh packaging/make-tarball.sh "$work"
sed "s|@VERSION@|$VERSION|g; s|@URL@|$URL|g" packaging/APKBUILD > "$work/APKBUILD"
adduser -D builder
addgroup builder abuild
chown -R builder "$work"
su builder -c "abuild-keygen -a -n"
cp /home/builder/.abuild/*.rsa.pub /etc/apk/keys/
su builder -c "cd '$work' && abuild checksum && abuild"
mkdir -p dist
cp /home/builder/packages/*/*/*.apk dist/
rm -rf "$work"
