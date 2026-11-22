#!/bin/sh
set -eu
: "${VERSION:?}" "${URL:?}"
dnf install -y rpm-build redhat-rpm-config meson gcc tar gzip
top="$PWD/rpmtop"
rm -rf "$top"
mkdir -p "$top/SOURCES" "$top/SPECS"
sh packaging/make-tarball.sh "$top/SOURCES"
sed "s|@VERSION@|$VERSION|g; s|@URL@|$URL|g" packaging/yalocc.spec > "$top/SPECS/yalocc.spec"
rpmbuild -bb --define "_topdir $top" "$top/SPECS/yalocc.spec"
mkdir -p dist
cp "$top"/RPMS/*/*.rpm dist/
rm -rf "$top"
