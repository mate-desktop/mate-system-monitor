#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Ubuntu
requires=(
	ccache      # Use ccache to speed up build
	meson       # Used for meson build
	ninja-build # Required backend for meson
)

requires+=(
	autopoint
	autoconf-archive
	g++
	gettext
	git
	libglib2.0-dev
	libglibmm-2.4-dev
	libgtk-3-dev
	libgtkmm-3.0-dev
	libgtop2-dev
	librsvg2-dev
	libsystemd-dev
	libwnck-3-dev
	libxml2-dev
	libpolkit-gobject-1-dev
	make
	mate-common
	polkitd
	quilt
	yelp-tools
)

infobegin "Update system"
apt-get update -y
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
