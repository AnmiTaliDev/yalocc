/* SPDX-License-Identifier: GPL-3.0-only */
#include "languages.h"

#include <string.h>

static const char *const c_exts[] = {"c", NULL};
static const char *const header_exts[] = {"h", "hh", "hpp", "hxx", NULL};
static const char *const cpp_exts[] = {"cpp", "cc", "cxx", NULL};
static const char *const python_exts[] = {"py", "pyw", NULL};
static const char *const shell_exts[] = {"sh", "bash", "zsh", NULL};
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

static const struct language languages[] = {
    {"C", "//", "/*", "*/", true, false, c_exts},
    {"C/C++ Header", "//", "/*", "*/", true, false, header_exts},
    {"C++", "//", "/*", "*/", true, false, cpp_exts},
    {"Python", "#", NULL, NULL, true, true, python_exts},
    {"Shell", "#", NULL, NULL, true, false, shell_exts},
    {"Go", "//", "/*", "*/", true, false, go_exts},
    {"Rust", "//", "/*", "*/", true, false, rust_exts},
    {"Java", "//", "/*", "*/", true, false, java_exts},
    {"JavaScript", "//", "/*", "*/", true, false, js_exts},
    {"TypeScript", "//", "/*", "*/", true, false, ts_exts},
    {"CSS", NULL, "/*", "*/", true, false, css_exts},
    {"Markdown", NULL, "<!--", "-->", false, false, markdown_exts},
    {"YAML", "#", NULL, NULL, false, false, yaml_exts},
    {"JSON", NULL, NULL, NULL, false, false, json_exts},
    {"TOML", "#", NULL, NULL, false, false, toml_exts},
    {"Lua", "--", "--[[", "]]", true, false, lua_exts},
    {"Ruby", "#", NULL, NULL, true, false, ruby_exts},
};

#define NLANGS (sizeof(languages) / sizeof(languages[0]))

const struct language *find_language(const char *path)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    if (dot == NULL || dot == base)
        return NULL;
    const char *ext = dot + 1;
    for (size_t i = 0; i < NLANGS; i++) {
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
