check_inc = include_directories('../../core/tests')

dbus_run_session = find_program('dbus-run-session', required: false)
cfgdd_program = find_program('cfgdd', required: false)

if dbus_run_session.found()
  test_bus = executable(
    'test_bus',
    'test_bus.c',
    include_directories: check_inc,
    dependencies: dbus_dep,
  )
  test(
    'daemon-bus',
    dbus_run_session,
    args: ['--', test_bus, nabud],
    depends: nabud,
  )
  if cfgdd_program.found()
    test(
      'daemon-config',
      dbus_run_session,
      args: ['--', test_bus, nabud, cfgdd_program, meson.project_source_root() / 'data'],
      depends: nabud,
      timeout: 60,
    )
  endif
endif
