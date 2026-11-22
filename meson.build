project(
  'yalocc',
  'c',
  version: '0.1.0',
  license: 'GPL-3.0-only',
  default_options: ['c_std=c99', 'warning_level=2'],
)

lang_dir = get_option('prefix') / get_option('datadir') / 'yalocc'

add_project_arguments(
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="' + meson.project_version() + '"',
  '-DLANG_FILE="' + lang_dir / 'languages.conf' + '"',
  language: 'c',
)

executable(
  'yalocc',
  [
    'src/main.c',
    'src/languages.c',
    'src/counter.c',
    'src/walker.c',
    'src/gitignore.c',
  ],
  include_directories: 'src',
  dependencies: dependency('threads'),
  install: true,
)

install_data('data/languages.conf', install_dir: lang_dir)

configure_file(input: 'data/languages.conf', output: 'languages.conf', copy: true)
