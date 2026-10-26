bindir = get_option('prefix') / get_option('bindir')
unit_dir = get_option('systemd_user_unit_dir')

conf = configuration_data()
conf.set('bindir', bindir)
conf.set('systemd_service', unit_dir != '' ? 'SystemdService=nabu.service' : '')

configure_file(
  input: 'org.freedesktop.Notifications.service.in',
  output: 'org.freedesktop.Notifications.service',
  configuration: conf,
  install_dir: get_option('datadir') / 'dbus-1' / 'services',
)

if unit_dir != ''
  configure_file(
    input: 'nabu.service.in',
    output: 'nabu.service',
    configuration: conf,
    install_dir: unit_dir,
  )
endif

install_data(
  'org.nuros.sanar.nabu.schema',
  install_dir: get_option('datadir') / 'cfgd' / 'schemas',
)
