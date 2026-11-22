project(
  'yalocc',
  'c',
  version: '0.1.0',
  license: 'GPL-3.0-only',
  default_options: ['c_std=c99', 'warning_level=2'],
)

add_project_arguments(
  '-D_POSIX_C_SOURCE=200809L',
  '-DVERSION="' + meson.project_version() + '"',
  language: 'c',
)

executable(
  'yalocc',
  [
    'src/main.c',
    'src/languages.c',
    'src/counter.c',
    'src/walker.c',
  ],
  include_directories: 'src',
)
