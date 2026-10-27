sh = find_program('sh')
dbus_run_session = find_program('dbus-run-session', required: false)

if dbus_run_session.found()
  notify_helper = executable(
    'notify',
    'notify.c',
    dependencies: dbus_dep,
  )
  test(
    'cli',
    sh,
    args: [files('test_cli.sh'), nabu_cli, nabud, notify_helper],
    depends: [nabu_cli, nabud, notify_helper],
  )
endif
