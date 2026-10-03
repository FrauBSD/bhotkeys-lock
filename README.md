[//]: # ($FrauBSD: bhotkeys-lock/README.md 2026-10-04 08:06:24 -0700 Devin Teske $)

# bhotkeys-lock

`Super+L` locks the screen.

One [bhotkeys](https://github.com/FrauBSD/bhotkeys) plugin. This
package ships `xlock-screen`, `xlock-invoke`, and `xlock-run`.
`xlock-screen` asks `xlock-invoke` to run `xlock` as Lock Screen.
`xlock-run` sets the window class and keeps a black underlay under
that lock so a random saver cannot leak the desktop. The plugin's
id is `lock`, which the chord list treats specially: the overlay
closes before the command runs, so the lock does not paint over an
open list. Under GNOME the plugin is off by default, because GNOME
Shell already locks on `Super+L`.

Home: [FrauBSD/bhotkeys-lock](https://github.com/FrauBSD/bhotkeys-lock)

## Opaque and sheer

A compositor such as picom draws each window at its own opacity. The
same `xlock` binary serves both locks. `xlock-invoke` tells them apart
by `WM_CLASS`.

Lock Desktop (`xlock-desktop`) is the sheer choice. `xlock` is started
with `+hide`, so the saver is drawn over a capture of the session and
the compositor leaves that window translucent. The desktop stays
visible through the lock: wallpaper, windows, and which workspace you
were on. With no compositor there is nothing to blend, so
`xlock-invoke` uses the opaque path instead.

Lock Screen (`xlock-screen`) hides the session. The same compositor
can still leave that window translucent, and the desktop shows
through. `xlock-run` keeps Lock Screen opaque. It sets the window
class, forces full opacity, and keeps a black underlay beneath
`xlock` for the whole lock.

## Requirements

- `bhotkeys`
- `xlock`
- `libX11` and `libXext` (to build `xlock-run`)

## Build / install

```sh
make install    # PREFIX=/usr/local by default
```

Installs `xlock-screen`, `xlock-invoke`, and `xlock-run` into
`${PREFIX}/bin`, and `lock` into `${PREFIX}/share/bhotkeys/plugins.d`.

## Plugin

```
id lock
label Lock screen
chord Super+l
enabled 0 gnome
command xlock-screen
```

Not offered at the greeter (`greeter 0`); there is nothing to lock.
