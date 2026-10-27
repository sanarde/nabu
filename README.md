# Sanar Nabu

> [!NOTE]
> **Repository Mirrors**
>
> - **git.nuros.org** ([sanarde/nabu](https://git.nuros.org/sanarde/nabu)): primary, self-hosted Forgejo instance, accounts restricted to the core team.
> - **GitHub** ([sanarde/nabu](https://github.com/sanarde/nabu)): mirror for external contributors. Issues and Pull Requests opened here are welcome and are reviewed and processed by the core team.

## About

Notification daemon for SanarDE that implements version 1.3 of the Desktop Notifications Specification and keeps a history of closed notifications. The popup that draws the notifications is a separate program.

## Dependencies

Daemon and command line tool:

- C17 compiler
- Meson (>= 1.0) and Ninja
- libdbus-1
- libcfgd, the client library of [cfgd](https://git.nuros.org/sanarde/cfgd)

Popup:

- CMake (>= 3.21)
- Qt (>= 6.8) with Core, Gui, Quick and DBus
- SanarStyle, the [sanar-style](https://git.nuros.org/sanarde/sanar-style) library

Optional, for the tests:

- dbus-run-session
- `cfgdd` in `PATH`, for the test that runs against a real cfgd
- Qt Test, for the popup test

## Build

```sh
meson setup build
meson compile -C build
meson test -C build
meson install -C build
```

The systemd user unit is installed only when its directory is given, for example `-Dsystemd_user_unit_dir=/usr/lib/systemd/user`.

The popup is built separately:

```sh
cmake -S popup -B popup/build
cmake --build popup/build
ctest --test-dir popup/build
cmake --install popup/build
```

The popup test starts `nabud`. Pass its directory with `-DNABU_DAEMON_DIR=build/daemon` when it is not installed. When SanarStyle is installed to a non-standard prefix, add it to `CMAKE_PREFIX_PATH` and to `QML_IMPORT_PATH` at run time.

## Running

`nabud` takes the name `org.freedesktop.Notifications` on the session bus and starts on demand through the installed D-Bus service file. With the systemd unit it is started by systemd instead. Only one notification daemon can run in a session, so the service file conflicts with the ones of other daemons.

`nabu-popup` is started with the session through the installed autostart entry. It shows at most five notifications at once, the newest first, and hides itself when there are none. The position of the window is chosen by the compositor.

Closed notifications are kept in `$XDG_STATE_HOME/nabu/history` (default `~/.local/state/nabu/history`). Notifications with the `transient` hint are not kept.

## Settings

Settings are read from cfgd. The schema `org.nuros.sanar.nabu` is installed to `share/cfgd/schemas`. When cfgd is not running, the defaults apply until it starts.

| Key | Default | Meaning |
| --- | --- | --- |
| `/org/nuros/sanar/nabu/default-timeout` | 5000 | Milliseconds until a notification closes when the sender gives no timeout (500 to 120000) |
| `/org/nuros/sanar/nabu/history-limit` | 200 | Number of closed notifications kept in the history (0 to 10000) |
| `/org/nuros/sanar/nabu/do-not-disturb` | false | Do not disturb mode |

In do not disturb mode only critical notifications are shown. The others go to the history with the close reason `undefined`. Critical notifications never expire.

## Command line

```
nabu list
nabu history
nabu clear
nabu dismiss ID
nabu invoke ID KEY
nabu dnd [on|off|status]
nabu watch
```

`list` prints `id`, urgency, application and summary of the active notifications, separated by tabs. `history` prints time, close reason, urgency, application and summary. `invoke` triggers an action of a notification. `watch` prints a line for every `posted` and `closed` notification and every change of `dnd`.

## D-Bus interface

Besides the standard interface, the daemon takes the name `org.nuros.sanar.nabu` on the session bus. The object is `/org/nuros/sanar/nabu` and the interface is `org.nuros.sanar.Nabu`.

- `GetActive()` returns the active notifications.
- `GetHistory()` returns the history, the oldest entry first.
- `ClearHistory()`
- `Dismiss(u id)` closes a notification as dismissed by the user.
- `Invoke(u id, s action_key)` emits `ActionInvoked` and closes the notification unless it is resident. It returns the error `org.nuros.sanar.Nabu.Error.NotFound` for an unknown notification or action.
- Property `DoNotDisturb` (`b`, read and write) on `org.freedesktop.DBus.Properties`. A change is stored in cfgd.
- Signals `Posted` with the notification and `Closed(u id, u reason)`.

Any client of the session bus can call these methods.

## Links

- [CONTRIBUTING](CONTRIBUTING.md)

## License

GPL-3.0-only. See [LICENSE](LICENSE).
