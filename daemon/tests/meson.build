dbus_run_session = find_program('dbus-run-session', required: false)

if dbus_run_session.found()
  test_bus = executable(
    'test_bus',
    'test_bus.c',
    dependencies: dbus_dep,
  )
  test(
    'daemon-bus',
    dbus_run_session,
    args: ['--', test_bus, nabud],
    depends: nabud,
  )
endif
