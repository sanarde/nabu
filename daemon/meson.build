dbus_dep = dependency('dbus-1')
cfgd_dep = dependency('libcfgd')

nabud_src_inc = include_directories('src')

nabud_core = static_library(
  'nabud-core',
  'src/bus.c',
  'src/config.c',
  'src/control.c',
  include_directories: [nabud_src_inc, nabu_core_inc],
  link_with: nabu_core,
  dependencies: [dbus_dep, cfgd_dep],
)

nabud = executable(
  'nabud',
  'src/main.c',
  include_directories: [nabud_src_inc, nabu_core_inc],
  link_with: [nabud_core, nabu_core],
  dependencies: [dbus_dep, cfgd_dep],
  install: true,
)

subdir('tests')
