#!/bin/sh
set -eu
: "${VERSION:?}" "${URL:?}"
pacman -Syu --noconfirm base-devel meson
work="$PWD/archwork"
rm -rf "$work"
mkdir -p "$work"
sh packaging/make-tarball.sh "$work"
sed "s|@VERSION@|$VERSION|g; s|@URL@|$URL|g" packaging/PKGBUILD > "$work/PKGBUILD"
useradd -m builder
chown -R builder "$work"
su builder -c "cd '$work' && makepkg -f --noconfirm"
mkdir -p dist
cp "$work"/*.pkg.tar.zst dist/
rm -rf "$work"
