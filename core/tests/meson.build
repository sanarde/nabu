foreach name : ['notes', 'history']
  test_exe = executable(
    'test_' + name,
    'test_' + name + '.c',
    include_directories: nabu_core_inc,
    link_with: nabu_core,
  )
  test('core-' + name, test_exe)
endforeach
