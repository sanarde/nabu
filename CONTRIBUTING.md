# Contributing

- Build and run the tests before sending a change: `meson setup build && meson test -C build`. The test `daemon-config` runs only when `cfgdd` is in `PATH`.
- For changes in C code also run the tests with sanitizers: `meson setup build-san -Db_sanitize=address,undefined && meson test -C build-san`.
- For changes in the popup also run `ctest` in its build directory.
- Keep each change limited to one area.
- Report bugs with the version, the steps to reproduce, and the observed output.

Source layout:

- `core`: notification store and history, shared by the other parts.
- `daemon`: `nabud`, the D-Bus interfaces and the cfgd settings.
- `cli`: the `nabu` tool.
- `popup`: `nabu-popup`, the window that shows the notifications (Qt, built with CMake).
- `data`: D-Bus service file, systemd unit and the cfgd schema.
