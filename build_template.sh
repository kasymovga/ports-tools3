#PORTSROOT='/var/ports'
. "$PORTSROOT/ports.conf"
PORTS_ARCH=${PORTS_ARCH:-`uname -m`}
SCRIPTSDIR="$PORTSROOT/pkgblds-scripts"
CONFSDIR="$PORTSROOT/ports.conf.d"
NOSOURCE=''
KEEPOLD=''
NAME=''
VERSION=''
SOURCES_NAME=''
SOURCES_VERSION=''
BUILD=''
DEPENDS=''
OPTIONAL_DEPENDS=''
BUILD_DEPENDS=''
VERSION_DEPENDS=''
PORTDIR="$PORTSROOT/pkgblds/$PORT_PATH"
test -f "$PORTDIR/build.sh" && . "$PORTDIR/build.sh"
BUILD="$BUILD-$PORTS_ARCH"

test -z "$SOURCES_NAME" && SOURCES_NAME="$NAME"
test -z "$SOURCES_VERSION" && SOURCES_VERSION="$VERSION"
case "$NAME" in
lib*)
	PACKAGE_PREF="`expr substr "$NAME" 1 4`"
	;;
*)
	PACKAGE_PREF="`expr substr "$NAME" 1 1`"
	;;
esac
case "$SOURCES_NAME" in
lib*)
	SOURCES_PREF="`expr substr "$SOURCES_NAME" 1 4`"
	;;
*)
	SOURCES_PREF="`expr substr "$SOURCES_NAME" 1 1`"
	;;
esac
PACKAGE_BASE_DIR=packages/"$PORTS_ARCH"/"$PACKAGE_PREF"/"$NAME"
SOURCES_BASE_DIR=distfiles/"$SOURCES_PREF"/"$SOURCES_NAME"
PACKAGE_DIR="$PORTSROOT"/"$PACKAGE_BASE_DIR"
SOURCES_DIR="$PORTSROOT"/"$SOURCES_BASE_DIR"
PKGFILENAME="$NAME#$VERSION-$BUILD.pkg"

ARC_SUFFIXES='.tar.xz .tar.bz2 .tar.gz .tar.lzma .tgz .tbz .txz .tlz .cpio.xz .cpio.gz .cpio.bz2 .cpio.lzma .cgz .cbz .clz .cxz'
mkdir -p fr || exit 1
BUILDDIR="`pwd`"
FAKEROOTDIR="$BUILDDIR/fr"

get_cache_package() {
	echo get_cache_package "$@"
	unpack_by_name "$PACKAGE_DIR/$PKGFILENAME" "$FAKEROOTDIR"
	return $?
}

get_cache_sources() {
	echo get_cache_sources "$@"
	unpack_by_name "$SOURCES_DIR/$SOURCES_NAME-$SOURCES_VERSION" .
	return $?
}

get_sync_package() {
	echo get_sync_package "$@"
	case "$SYNC_TO" in
	http://*|ftp://)
		PKGFILENAME_URL="`echo "$PKGFILENAME"|sed 's/#/%23/'`"
		for SUF in $ARC_SUFFIXES
		do
			wget "$SYNC_TO/$PACKAGE_BASE_DIR/$PKGFILENAME_URL$SUF" && unpack "`pwd`/$PKGFILENAME$SUF" "$FAKEROOTDIR" && return 0
		done
		;;
	*)
		unpack_by_name "$SYNC_TO/$PACKAGE_BASE_DIR/$PKGFILENAME" "$FAKEROOTDIR" && return 0
		;;
	esac
	return 1
}

get_sync_sources() {
	echo get_sync_sources "$@"
	case "$SYNC_TO" in
	http://*|ftp://)
		for SUF in $ARC_SUFFIXES
		do
			wget "$SYNC_TO/$SOURCES_BASE_DIR/$SOURCES_NAME-$SOURCES_VERSION$SUF" && unpack "`pwd`/$SOURCES_NAME-$SOURCES_VERSION$SUF" . && return 0
		done
		;;
	*)
		unpack_by_name "$SYNC_TO/$SOURCES_BASE_DIR/$SOURCES_NAME-$SOURCES_VERSION" . && return 0
		;;
	esac
	return 1
}

uncpio() {
	echo uncpio "$@"
	DESTINATION="$1"
	cd "$DESTINATION"
	cpio -imd
	CPIO_RET="$?"
	cd - > /dev/null
	return "$CPIO_RET"
}

unpack() {
	echo unpack "$@"
	SOURCE="$1"
	DESTINATION="$2"
	case "$1" in
	*.tar.gz|*.tgz)
		zcat "$1" | tar x
		;;
	*.tar.bz2|*.tbz)
		bzcat "$1" | tar x
		;;
	*.tar.xz|*.txz)
		xzcat "$1" | tar x
		;;
	*.tar.lzma|*.tlz)
		lzcat "$1" | tar x
		;;
	*.cpio.gz|*.cgz)
		gzcat "$1" | uncpio "$2"
		;;
	*.cpio.bz2|*.cbz)
		bzcat "$1" | uncpio "$2"
		;;
	*.cpio.xz|*.cxz)
		xzcat "$1" | uncpio "$2"
		;;
	*.cpio.lzma|*.clz)
		lzcat "$1" | uncpio "$2"
		;;
	*)
		false
	esac
	return $?
}

unpack_by_name() {
	echo unpack_by_name "$@"
	SOURCE="$1"
	DESTINATION="$2"
	for SUF in $ARC_SUFFIXES
	do
		test -f "$SOURCE$SUF" || continue
		unpack "$SOURCE$SUF" "$DESTINATION"
		return $?
	done
	return 1
}

save_package_to_cache() {
	echo save_package_to_cache "$@"
	if test "$CACHE_PACKAGES" = none
	then
		return 0
	fi
	mkdir -p "$PACKAGE_DIR"
	if test "$CACHE_PACKAGES" != all
	then
		rm -f "$PACKAGE_DIR"/*
	fi
	cd "$FAKEROOTDIR" || return 1
	find `ls -A`| cpio --owner root.root -o 2> /dev/null| xz > "$PACKAGE_DIR/$PKGFILENAME.cpio.xz"
	return $?
}

save_sources_to_cache() {
	echo save_sources_to_cache "$@"
	if test "$CACHE_SOURCES" = none
	then
		return 0
	fi
	mkdir -p "$SOURCES_DIR"
	if test "$CACHE_SOURCES" != all
	then
		rm -f "$SOURCES_DIR"/*
	fi
	tar c "$SOURCES_NAME-$SOURCES_VERSION" | xz > "$SOURCES_DIR/$SOURCES_NAME-$SOURCES_VERSION.tar.xz"
	return $?
}

_get_package() {
	if get_cache_package
	then
		return 0
	fi
	if get_sync_package
	then
		save_package_to_cache
		return 0
	fi
	return 1
}

_get_source() {
	if get_cache_sources
	then
		echo -n
	else 
		if get_sync_sources
		then
			echo -n
		else
			if port_get_sources
			then
				cd "$BUILDDIR"
				if test ! -d "$SOURCES_NAME-$SOURCES_VERSION"
				then
					unpack_by_name "$SOURCES_NAME-$SOURCES_VERSION" . || return 1
				fi
			else
				return 1
			fi
		fi
		save_sources_to_cache
	fi
	return 0
}

_build() {
	if test "$NOSOURCE" != y
	then
		cd "$SOURCES_NAME-$SOURCES_VERSION" || return 1
	else
		cd "$BUILDDIR"
	fi
	if test -f "$PORTDIR/fr.tar"
	then
		tar x -C "$FAKEROOTDIR" -f "$PORTDIR/fr.tar" || return 1
	fi
	if test -d "$PORTDIR/fr"
	then
		cp -dR "$PORTDIR/fr/"* "$FAKEROOTDIR" || return 1
	fi
	if port_build
	then
		echo -n "$NAME" > "$FAKEROOTDIR/.name" || return 1
		echo -n "$VERSION-$BUILD" > "$FAKEROOTDIR/.version" || return 1
		for F in .install .noupdate .preinstall
		do
			if test -f "$PORTDIR/$F"
			then
				cp "$PORTDIR/$F" "$FAKEROOTDIR" || exit 1
			fi
		done
		cd "$BUILDDIR"
		save_package_to_cache
		return 0
	fi
	return 1
}

echo Script for "$NAME/$VERSION-$BUILD" started.
case "$1" in
get_package)
	echo Try get package...
	_get_package && exit 0
	;;
build_package)
	if test "$NOSOURCE" != y
	then
		echo Try get sources...
		_get_source || {
			echo "ERROR: Cannot get sources $SOURCES_NAME-$SOURCES_VERSION for $NAME/$VERSION"
			exit 1
		}
	fi
	echo Try build...
	_build || {
		echo "ERROR: Cannot build $NAME/$VERSION"
		exit 1
	}
	exit 0
	;;
esac
echo Script for "$NAME/$VERSION-$BUILD" failed.
exit 1

