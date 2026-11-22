/* SPDX-License-Identifier: GPL-3.0-only */
#include "languages.h"

#include <string.h>

static const char *const c_exts[] = {"c", NULL};
static const char *const header_exts[] = {"h", "hh", "hpp", "hxx", NULL};
static const char *const cpp_exts[] = {"cpp", "cc", "cxx", NULL};
static const char *const python_exts[] = {"py", "pyw", NULL};
static const char *const shell_exts[] = {"sh", "bash", "zsh", NULL};
static const char *const shell_files[] = {".bashrc", ".bash_profile", ".zshrc", ".profile", NULL};
static const char *const go_exts[] = {"go", NULL};
static const char *const rust_exts[] = {"rs", NULL};
static const char *const java_exts[] = {"java", NULL};
static const char *const js_exts[] = {"js", "mjs", "cjs", NULL};
static const char *const ts_exts[] = {"ts", "tsx", NULL};
static const char *const css_exts[] = {"css", NULL};
static const char *const markdown_exts[] = {"md", "markdown", NULL};
static const char *const yaml_exts[] = {"yml", "yaml", NULL};
static const char *const json_exts[] = {"json", NULL};
static const char *const toml_exts[] = {"toml", NULL};
static const char *const lua_exts[] = {"lua", NULL};
static const char *const ruby_exts[] = {"rb", NULL};
static const char *const cmake_exts[] = {"cmake", NULL};
static const char *const cmake_files[] = {"CMakeLists.txt", NULL};
static const char *const dockerfile_exts[] = {"dockerfile", NULL};
static const char *const dockerfile_files[] = {"Dockerfile", NULL};
static const char *const makefile_exts[] = {"mk", NULL};
static const char *const makefile_files[] = {"Makefile", "makefile", "GNUmakefile", "BSDmakefile", NULL};
static const char *const meson_files[] = {"meson.build", "meson.options", "meson_options.txt", NULL};

static const struct language languages[] = {
    {"C", "//", "/*", "*/", true, false, c_exts, NULL},
    {"C/C++ Header", "//", "/*", "*/", true, false, header_exts, NULL},
    {"C++", "//", "/*", "*/", true, false, cpp_exts, NULL},
    {"Python", "#", NULL, NULL, true, true, python_exts, NULL},
    {"Shell", "#", NULL, NULL, true, false, shell_exts, shell_files},
    {"Go", "//", "/*", "*/", true, false, go_exts, NULL},
    {"Rust", "//", "/*", "*/", true, false, rust_exts, NULL},
    {"Java", "//", "/*", "*/", true, false, java_exts, NULL},
    {"JavaScript", "//", "/*", "*/", true, false, js_exts, NULL},
    {"TypeScript", "//", "/*", "*/", true, false, ts_exts, NULL},
    {"CSS", NULL, "/*", "*/", true, false, css_exts, NULL},
    {"Markdown", NULL, "<!--", "-->", false, false, markdown_exts, NULL},
    {"YAML", "#", NULL, NULL, false, false, yaml_exts, NULL},
    {"JSON", NULL, NULL, NULL, false, false, json_exts, NULL},
    {"TOML", "#", NULL, NULL, false, false, toml_exts, NULL},
    {"Lua", "--", "--[[", "]]", true, false, lua_exts, NULL},
    {"Ruby", "#", NULL, NULL, true, false, ruby_exts, NULL},
    {"CMake", "#", NULL, NULL, false, false, cmake_exts, cmake_files},
    {"Dockerfile", "#", NULL, NULL, false, false, dockerfile_exts, dockerfile_files},
    {"Makefile", "#", NULL, NULL, false, false, makefile_exts, makefile_files},
    {"Meson", "#", NULL, NULL, false, false, NULL, meson_files},
};

#define NLANGS (sizeof(languages) / sizeof(languages[0]))

const struct language *find_language(const char *path)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    for (size_t i = 0; i < NLANGS; i++) {
        if (languages[i].filenames == NULL)
            continue;
        for (size_t j = 0; languages[i].filenames[j] != NULL; j++) {
            if (strcmp(base, languages[i].filenames[j]) == 0)
                return &languages[i];
        }
    }
    const char *dot = strrchr(base, '.');
    if (dot == NULL || dot == base)
        return NULL;
    const char *ext = dot + 1;
    for (size_t i = 0; i < NLANGS; i++) {
        if (languages[i].exts == NULL)
            continue;
        for (size_t j = 0; languages[i].exts[j] != NULL; j++) {
            if (strcmp(ext, languages[i].exts[j]) == 0)
                return &languages[i];
        }
    }
    return NULL;
}

const struct language *language_by_index(size_t index)
{
    if (index >= NLANGS)
        return NULL;
    return &languages[index];
}

size_t language_count(void)
{
    return NLANGS;
}
