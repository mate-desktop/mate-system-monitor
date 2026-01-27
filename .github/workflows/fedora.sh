#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

requires+=(
	autoconf-archive
	gcc
	gcc-c++
	dbus-glib-devel
	desktop-file-utils
	git
	gtk3-devel
	gtkmm30-devel
	libgtop2-devel
	librsvg2-devel
	libwnck3-devel
	libxml2-devel
	make
	mate-common
	polkit-devel
	redhat-rpm-config
	systemd-devel
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
