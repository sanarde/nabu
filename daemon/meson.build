dbus_dep = dependency('dbus-1')

nabud_src_inc = include_directories('src')

nabud_core = static_library(
  'nabud-core',
  'src/bus.c',
  'src/control.c',
  include_directories: [nabud_src_inc, nabu_core_inc],
  link_with: nabu_core,
  dependencies: dbus_dep,
)

nabud = executable(
  'nabud',
  'src/main.c',
  include_directories: [nabud_src_inc, nabu_core_inc],
  link_with: [nabud_core, nabu_core],
  dependencies: dbus_dep,
  install: true,
)

subdir('tests')
