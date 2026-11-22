#!/bin/sh
set -eu
: "${VERSION:?}" "${URL:?}"
apt-get update
apt-get install -y build-essential debhelper fakeroot meson ninja-build
rm -rf debian
cp -r packaging/debian debian
sed -i "s|@VERSION@|$VERSION|g; s|@URL@|$URL|g" debian/control debian/copyright
printf 'yalocc (%s) unstable; urgency=medium\n\n  * Release %s.\n\n -- yalocc <noreply@anmitali.dev>  %s\n' \
    "$VERSION" "$VERSION" "$(date -R)" > debian/changelog
dpkg-buildpackage -b -us -uc
mkdir -p dist
cp ../yalocc_*.deb dist/
rm -rf debian
