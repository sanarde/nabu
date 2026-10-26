nabu_core_inc = include_directories('src')

nabu_core = static_library(
  'nabu-core',
  'src/history.c',
  'src/notes.c',
  include_directories: nabu_core_inc,
)

subdir('tests')
