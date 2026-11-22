# yalocc

Command-line tool that counts blank, comment, and code lines in source files and reports totals per language.

## Dependencies

- C compiler (gcc or clang)
- meson
- ninja

## Build

```
meson setup build
meson compile -C build
```

The binary is written to `build/yalocc`.

## Usage

```
yalocc [options] [paths...]
```

Options:

```
  -h, --help       Show this help
  -V, --version    Show version
```

Paths may be files or directories. Without arguments the current directory is scanned. Example output:

```
-------------------------------------
Language     Files Blank Comment Code
-------------------------------------
C                4    65       4  441
C/C++ Header     3    12       3   33
-------------------------------------
SUM:             7    77       7  474
-------------------------------------
```

Files with unrecognized extensions are grouped under "Unknown". Binary files are skipped. Version control directories (.git, .hg, .svn, .bzr, .cvs) are not traversed.

## License

GPL-3.0-only, see LICENSE.
