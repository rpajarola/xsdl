dnl Xsdl - an X server that renders into an SDL2 window.
dnl
dnl Standalone build of the kdrive Xsdl server, extracted from xorg-server.
dnl macOS only: X libraries, XKB data and fonts come from XQuartz, SDL2 from
dnl Homebrew or MacPorts.

AC_PREREQ([2.69])
AC_INIT([xsdl], [0.1.0], [rp@servium.ch])
AC_CONFIG_SRCDIR([hw/kdrive/sdl/sdl.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_LIBOBJ_DIR([os])
AC_CONFIG_HEADERS([include/dix-config.h include/xkb-config.h include/version-config.h])
AC_CANONICAL_HOST
case "$host_os" in
darwin*) ;;
*) AC_MSG_ERROR([Xsdl only builds on macOS; use Xephyr or Xvfb elsewhere]) ;;
esac
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Wno-portability])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_AR
AC_PROG_RANLIB
AC_C_BIGENDIAN
AC_CHECK_SIZEOF([unsigned long])
PKG_PROG_PKG_CONFIG

AC_ARG_WITH([x11-prefix],
    [AS_HELP_STRING([--with-x11-prefix=DIR], [XQuartz installation (default: /opt/X11)])],
    [X11_PREFIX="$withval"], [X11_PREFIX=/opt/X11])
if test ! -d "$X11_PREFIX/lib/pkgconfig"; then
    AC_MSG_ERROR([XQuartz not found in $X11_PREFIX; install it from https://www.xquartz.org or use --with-x11-prefix])
fi
dnl Search XQuartz first; pkg-config itself still finds Homebrew's SDL2.
PKG_CONFIG_PATH="$X11_PREFIX/lib/pkgconfig:$X11_PREFIX/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PKG_CONFIG_PATH

dnl The X server version this tree was taken from (xorg-server 26.1.99).
AC_DEFINE([VENDOR_NAME], ["The X.Org Foundation"], [Vendor name])
AC_DEFINE([VENDOR_NAME_SHORT], ["X.Org"], [Short vendor name])
AC_DEFINE([VENDOR_RELEASE], [12601099], [Vendor release])
AC_DEFINE([VENDOR_MAN_VERSION], ["Version 26.1.99"], [Version for man pages])
AC_DEFINE([VENDOR_WEB], ["http://wiki.x.org"], [Vendor web address])

dnl ------------------------------------------------------------------
dnl Required packages
dnl ------------------------------------------------------------------

PKG_CHECK_MODULES([XSERVER], [
    xproto >= 7.0.34
    randrproto >= 1.6.0
    renderproto >= 0.11
    xextproto >= 7.2.99.901
    inputproto >= 2.3.99.1
    kbproto >= 1.0.3
    fontsproto >= 2.1.3
    fixesproto >= 6.1
    damageproto >= 1.1
    xcmiscproto >= 1.2.0
    bigreqsproto >= 1.1.0
    presentproto >= 1.4
    videoproto
    compositeproto >= 0.4
    recordproto >= 1.13.99.1
    scrnsaverproto >= 1.1
    resourceproto >= 1.2.0
    xineramaproto
    xf86vidmodeproto >= 2.2.99.1
    dpmsproto >= 1.2
    xtrans >= 1.3.5
    pixman-1
    xkbfile
    xfont2 >= 2.0
    xau
])

PKG_CHECK_MODULES([SDL], [sdl2 >= 2.0.18])
SDL_VERSION=`$PKG_CONFIG --modversion sdl2`

PKG_CHECK_MODULES([XDMCP], [xdmcp], [have_xdmcp=yes], [have_xdmcp=no])
if test "x$have_xdmcp" = xyes; then
    AC_DEFINE([XDMCP], [1], [Support the X Display Manager Control Protocol])
    save_LIBS="$LIBS"
    LIBS="$LIBS $XDMCP_LIBS"
    AC_CHECK_FUNC([XdmcpWrap],
        [AC_DEFINE([HASXDMAUTH], [1], [Support XDM-AUTHENTICATION-1])])
    LIBS="$save_LIBS"
fi
AM_CONDITIONAL([XDMCP], [test "x$have_xdmcp" = xyes])

PKG_CHECK_MODULES([XSHMFENCE], [xshmfence >= 1.1],
    [AC_DEFINE([HAVE_XSHMFENCE], [1], [Use libxshmfence])], [:])

dnl ------------------------------------------------------------------
dnl Platform
dnl ------------------------------------------------------------------

AC_DEFINE([CSRG_BASED], [1], [BSD-derived system])

AC_CHECK_MEMBER([struct sockaddr_in.sin_len],
    [AC_DEFINE([BSD44SOCKETS], [1], [struct sockaddr_in has sin_len])], [],
    [#include <sys/types.h>
     #include <sys/socket.h>
     #include <netinet/in.h>])

AC_CHECK_DECL([SCM_RIGHTS],
    [AC_DEFINE([XTRANS_SEND_FDS], [1], [Pass file descriptors over sockets])],
    [], [#include <sys/socket.h>])

AC_CHECK_HEADERS([dlfcn.h execinfo.h sys/ucred.h sys/un.h])
AC_CHECK_FUNCS([arc4random_buf backtrace cbrt getaddrinfo getifaddrs \
    getpeereid inet_ntop issetugid mkostemp poll setitimer sigaction \
    sigprocmask strcasecmp strcasestr strncasecmp strndup strlcpy vasprintf \
    reallocarray timingsafe_memcmp])
dnl Older macOS releases lack these.
AC_REPLACE_FUNCS([reallocarray timingsafe_memcmp])

AC_CHECK_MEMBER([struct xucred.cr_pid],
    [AC_DEFINE([HAVE_XUCRED_CR_PID], [1], [struct xucred has cr_pid])], [],
    [#include <sys/ucred.h>])

AC_DEFINE([BUSFAULT], [1], [Handle SIGBUS on shared memory fences])

AC_MSG_CHECKING([for a monotonic clock])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _POSIX_C_SOURCE 200112L
#include <time.h>
#include <unistd.h>
#ifndef CLOCK_MONOTONIC
#error no CLOCK_MONOTONIC
#endif
]])], [AC_DEFINE([MONOTONIC_CLOCK], [1], [Have CLOCK_MONOTONIC])
       AC_MSG_RESULT([yes])], [AC_MSG_RESULT([no])])

dnl Input thread
AC_SEARCH_LIBS([pthread_create], [pthread], [have_pthread=yes], [have_pthread=no])
if test "x$have_pthread" = xyes; then
    AC_DEFINE([INPUTTHREAD], [1], [Process input in a separate thread])
    AC_MSG_CHECKING([pthread_setname_np flavour])
    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Werror=implicit-function-declaration"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]],
        [[pthread_setname_np(pthread_self(), "x");]])],
        [AC_DEFINE([HAVE_PTHREAD_SETNAME_NP_WITH_TID], [1], [pthread_setname_np(tid, name)])
         AC_MSG_RESULT([with tid])],
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]],
            [[pthread_setname_np("x");]])],
            [AC_DEFINE([HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID], [1], [pthread_setname_np(name)])
             AC_MSG_RESULT([without tid])],
            [AC_MSG_RESULT([none])])])
    CFLAGS="$save_CFLAGS"
fi

dnl SHA1 for the X-Resource extension's client IDs and MIT-SHM fences
AC_CHECK_HEADER([CommonCrypto/CommonDigest.h],
    [AC_DEFINE([HAVE_SHA1_IN_COMMONCRYPTO], [1], [Use CommonCrypto SHA1])],
    [AC_MSG_ERROR([CommonCrypto not found; are the Xcode command line tools installed?])])

dnl ------------------------------------------------------------------
dnl Fixed feature set: the extensions the server sources are built for
dnl ------------------------------------------------------------------

if test "$ac_cv_sizeof_unsigned_long" = 8; then
    AC_DEFINE([_XSERVER64], [1], [64-bit longs; vital for Xlib headers])
fi
if test "x$ac_cv_c_bigendian" = xyes; then
    AC_DEFINE([X_BYTE_ORDER], [X_BIG_ENDIAN], [Byte order])
else
    AC_DEFINE([X_BYTE_ORDER], [X_LITTLE_ENDIAN], [Byte order])
fi

for m in COMPOSITE DAMAGE DBE DPMSExtension MITSHM PANORAMIX PRESENT RANDR \
         RENDER RES SCREENSAVER SHAPE XACE XFIXES XINERAMA XINPUT XRECORD \
         XTEST XV XvExtension XvMCExtension X_REGISTRY_RESOURCE \
         TCPCONN UNIXCONN IPv6 _XTYPEDEF_POINTER _XITYPEDEF_POINTER; do
    AC_DEFINE_UNQUOTED([$m], [1])
done

dnl ------------------------------------------------------------------
dnl Paths
dnl ------------------------------------------------------------------

AC_ARG_WITH([xkb-path],
    [AS_HELP_STRING([--with-xkb-path=DIR], [XKB data directory])],
    [XKBPATH="$withval"], [XKBPATH=""])
AC_ARG_WITH([xkb-bin-directory],
    [AS_HELP_STRING([--with-xkb-bin-directory=DIR], [directory containing xkbcomp])],
    [XKB_BIN_DIRECTORY="$withval"], [XKB_BIN_DIRECTORY=""])
AC_ARG_WITH([xkb-output],
    [AS_HELP_STRING([--with-xkb-output=DIR], [directory for compiled keymaps (default: /tmp)])],
    [XKM_OUTPUT_DIR="$withval"], [XKM_OUTPUT_DIR="/tmp"])
AC_ARG_WITH([default-font-path],
    [AS_HELP_STRING([--with-default-font-path=PATH], [comma separated font path])],
    [FONTPATH="$withval"], [FONTPATH=""])
AC_ARG_ENABLE([listen-tcp],
    [AS_HELP_STRING([--enable-listen-tcp], [listen on TCP by default])],
    [listen_tcp="$enableval"], [listen_tcp=no])

test -z "$XKBPATH" && XKBPATH="$X11_PREFIX/share/X11/xkb"
test -z "$XKB_BIN_DIRECTORY" && XKB_BIN_DIRECTORY="$X11_PREFIX/bin"
if test -z "$FONTPATH"; then
    fontroot="$X11_PREFIX/share/fonts"
    FONTPATH="$fontroot/misc/,$fontroot/TTF/,$fontroot/OTF/,$fontroot/Type1/,$fontroot/100dpi/,$fontroot/75dpi/"
fi
case "$XKM_OUTPUT_DIR" in */) ;; *) XKM_OUTPUT_DIR="$XKM_OUTPUT_DIR/" ;; esac

AC_DEFINE_UNQUOTED([XKB_BASE_DIRECTORY], ["$XKBPATH"], [XKB data directory])
AC_DEFINE_UNQUOTED([XKB_BIN_DIRECTORY], ["$XKB_BIN_DIRECTORY"], [xkbcomp directory])
AC_DEFINE_UNQUOTED([XKM_OUTPUT_DIR], ["$XKM_OUTPUT_DIR"], [Compiled keymap directory])
AC_DEFINE([XKB_DFLT_RULES], ["evdev"], [Default XKB rules])
AC_DEFINE([XKB_DFLT_MODEL], ["pc105"], [Default XKB model])
AC_DEFINE([XKB_DFLT_LAYOUT], ["us"], [Default XKB layout])
AC_DEFINE([XKB_DFLT_VARIANT], [""], [Default XKB variant])
AC_DEFINE([XKB_DFLT_OPTIONS], [""], [Default XKB options])
AC_DEFINE_UNQUOTED([COMPILEDDEFAULTFONTPATH], ["$FONTPATH"], [Default font path])

LIBDIR=`test "x$prefix" = xNONE && prefix=$ac_default_prefix
         test "x$exec_prefix" = xNONE && exec_prefix=$prefix
         eval echo "$libdir"`
AC_DEFINE_UNQUOTED([SERVER_MISC_CONFIG_PATH], ["$LIBDIR/xsdl"],
    [Directory for protocol.txt])

AC_DEFINE([LISTEN_UNIX], [1], [Listen on Unix sockets by default])
AC_DEFINE([LISTEN_LOCAL], [1], [Listen on local connections by default])
if test "x$listen_tcp" = xyes; then
    AC_DEFINE([LISTEN_TCP], [1], [Listen on TCP by default])
fi

dnl ------------------------------------------------------------------

XSDL_CFLAGS="-DHAS_FCHOWN -DHAS_STICKY_DIR_BIT -fno-strict-aliasing -fvisibility=hidden"
AC_SUBST([XSDL_CFLAGS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_MSG_RESULT([
  Xsdl $VERSION
    SDL:          $SDL_VERSION
    XQuartz:      $X11_PREFIX
    XDMCP:        $have_xdmcp
    XKB data:     $XKBPATH
    xkbcomp:      $XKB_BIN_DIRECTORY
    Font path:    $FONTPATH
    Listen TCP:   $listen_tcp
])
