#!/bin/sh -e
# Copyright (c) 2025 Roger Brown.
# Licensed under the MIT License.

if test -z "$MAINTAINER"
then
	if git config user.email > /dev/null
	then
		MAINTAINER="$(git config user.email)"
	else
		echo MAINTAINER not set 1>&2
		false
	fi
fi

cleanup()
{
	rm -rf root rpms rpm.spec
}

cleanup

trap cleanup 0

VERSION=1.0
RELEASE=1
PKGNAME=systemd-echo

if dpkg --print-architecture 2>/dev/null
then
	mkdir -p root/DEBIAN root/usr/lib/systemd/system

	cp echo.socket root/usr/lib/systemd/system/echo.socket
	cp echo@.service root/usr/lib/systemd/system/echo@.service

	SIZE=$(du -sk root | while read A B; do echo $A; done)

	cat > root/DEBIAN/control <<EOF
Package: $PKGNAME
Version: $VERSION-$RELEASE
Architecture: all
Installed-Size: $SIZE
Maintainer: $MAINTAINER
Section: misc
Priority: extra
Depends: systemd
Description: echo
 .
EOF

	dpkg-deb --root-owner-group --build root "$PKGNAME"_"$VERSION-$RELEASE"_all.deb

	cleanup
fi

if rpmbuild --version 2>/dev/null
then
	mkdir -p root/usr/lib/systemd/system
	cp echo.socket root/usr/lib/systemd/system/echo.socket
	cp echo@.service root/usr/lib/systemd/system/echo@.service

	cat >rpm.spec <<EOF
Summary: echo service
Name: $PKGNAME
Version: $VERSION
Release: $RELEASE
Group: Applications/System
License: MIT
Requires: systemd
BuildArch: noarch
Prefix: /
%description
echo service

%files
%defattr(-,root,root)
/usr/lib/systemd/system/echo@.service
/usr/lib/systemd/system/echo.socket
%clean
EOF

	rpmbuild --buildroot "$PWD/root" --define "_rpmdir $PWD/rpms" -bb "$PWD/rpm.spec"

	find rpms -type f -name "*.rpm" | while read N
	do
		mv "$N" .
	done
fi
