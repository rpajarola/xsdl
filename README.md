# Xsdl

An X11 server for macOS that renders into an SDL2 window.

Xsdl is a kdrive X server: the drawing, protocol and extension code is
xorg-server's, cut down to what this one server needs, and the display and
input come from an SDL2 window. It is a lightweight display for X clients on
old Unix systems, emulated machines and remote hosts, without running XQuartz
itself. It only builds on macOS; elsewhere use Xephyr or Xvfb.

## Building

You need:

- XQuartz (<https://www.xquartz.org>), which provides the X libraries and
  protocol headers, xkbcomp, the XKB data and the fonts, in `/opt/X11`
- SDL2, e.g. `brew install sdl2` (Homebrew's sdl2-compat works)
- autoconf, automake and pkg-config, e.g. `brew install autoconf automake pkg-config`

Then:

    ./autogen.sh
    make
    make install

`configure` finds XQuartz by itself. Options: `--with-x11-prefix` if
XQuartz is not in `/opt/X11`, `--with-xkb-output` for where compiled keymaps
go (default `/tmp`), `--with-default-font-path`, `--enable-listen-tcp`.

## Running

    Xsdl :1 -screen 1152x900x8 -zoom 1 -listen tcp &
    DISPLAY=:1 twm &

Xsdl has no window manager of its own. See `Xsdl(1)` for the options.

## Origin

The sources come from xorg-server 26.1.99
(<https://gitlab.freedesktop.org/xorg/xserver>) plus `hw/kdrive/sdl/sdl.c`,
a new SDL2 backend loosely based on the Xsdl server removed from xorg-server
in 2009. See `COPYING` for the licenses.
