# yalocc

Command-line tool that counts blank, comment, and code lines in source files and reports totals per language.

## Dependencies

- C compiler 
- meson
- ninja

## Build

```
meson setup build
meson compile -C build
```

The binary is written to `build/yalocc`. `meson install -C build` also installs `data/languages.conf` to `<prefix>/share/yalocc/`.

## Packages

Pushing a tag `vX.Y.Z` builds deb, rpm, Arch (`.pkg.tar.zst`), and Alpine (`.apk`) packages and attaches them to a GitHub release. The tag must match the version in `meson.build`.

To build one package locally, run the matching script from the repository root in a container of that distribution:

```
docker run --rm -v "$PWD:/src" -w /src -e VERSION=0.1.0 -e URL=https://github.com/OWNER/yalocc \
  debian:stable sh packaging/build-deb.sh
```

Scripts: `build-deb.sh` (debian), `build-rpm.sh` (fedora), `build-arch.sh` (archlinux), `build-apk.sh` (alpine). Results are written to `dist/`.

## Usage

```
yalocc [options] [paths...]
```

Options:

```
  -h, --help            Show this help
  -V, --version         Show version
      --by-file         Show counts for every file
      --exclude-dir N   Skip directories named N (repeatable)
      --include-lang N  Count only languages named N (repeatable)
      --exclude-lang N  Skip languages named N (repeatable)
      --no-gitignore    Do not apply .gitignore files
      --list-languages  Show known language names
      --lang-file F     Read language definitions from F
      --threads N       Use N counting threads (default: CPU count)
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

Options taking a value accept both `--opt value` and `--opt=value`. Language names are case-insensitive.

Language definitions are read at startup from the file given with --lang-file, otherwise from `languages.conf` in the install data directory, otherwise from `languages.conf` next to the executable (meson copies it to `build/`). The file is a list of blocks, each starting with `language NAME`, followed by any of: `ext E...`, `file NAME...`, `line_comment TOKEN`, `block START END`, `strings yes|no`, `docstrings yes|no`. Lines starting with `#` are ignored.

Languages are detected by extension or by exact file name (for example Makefile, Dockerfile, CMakeLists.txt, meson.build). Files with unrecognized names are skipped unless `--include-lang Unknown` is given. Binary files are skipped and reported as one count on stderr. Version control directories (.git, .hg, .svn, .bzr, .cvs) are not traversed, and directories can be excluded with --exclude-dir. `.gitignore` files found inside the scanned directories are applied (comments, negation, anchored and directory-only patterns, `*`, `?`, `[]`, and a leading `**/`; other `**` act like `*`). Parent and global ignore files are not read. Use --no-gitignore to turn this off. Files are counted in parallel, one thread per CPU by default, configurable with --threads. With --by-file every counted file is listed under its language.

## License

GPL-3.0-only, see LICENSE.
