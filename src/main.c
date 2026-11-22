/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "languages.h"
#include "report.h"
#include "scan.h"

static bool language_name_exists(const char *name)
{
    if (strcasecmp(name, "Unknown") == 0)
        return true;
    for (size_t i = 0; i < language_count(); i++) {
        if (strcasecmp(language_by_index(i)->name, name) == 0)
            return true;
    }
    return false;
}

static void usage(FILE *out)
{
    fprintf(out, "Usage: yalocc [options] [paths...]\n");
    fprintf(out, "Count blank, comment, and code lines in source files.\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  -h, --help            Show this help\n");
    fprintf(out, "  -V, --version         Show version\n");
    fprintf(out, "      --by-file         Show counts for every file\n");
    fprintf(out, "      --exclude-dir N   Skip directories named N (repeatable)\n");
    fprintf(out, "      --include-lang N  Count only languages named N (repeatable)\n");
    fprintf(out, "      --exclude-lang N  Skip languages named N (repeatable)\n");
    fprintf(out, "      --list-languages  Show known language names\n");
    fprintf(out, "      --no-gitignore    Do not apply .gitignore files\n");
    fprintf(out, "      --lang-file F     Read language definitions from F\n");
    fprintf(out, "      --threads N       Use N counting threads (default: CPU count)\n");
}

static size_t default_thread_count(void)
{
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    return ncpu > 0 ? (size_t)ncpu : 1;
}

static int fail_hint(void)
{
    fprintf(stderr, "Try 'yalocc --help'.\n");
    return 1;
}

static bool option_is(const char *arg, size_t namelen, const char *name)
{
    return strlen(name) == namelen && strncmp(arg, name, namelen) == 0;
}

static bool take_value(int argc, char **argv, int *i, size_t namelen, const char *inline_value,
                       const char **value)
{
    if (inline_value != NULL) {
        *value = inline_value;
        return true;
    }
    if (*i + 1 >= argc) {
        fprintf(stderr, "yalocc: missing argument for %.*s\n", (int)namelen, argv[*i]);
        return false;
    }
    *i += 1;
    *value = argv[*i];
    return true;
}

static int load_default_languages(void)
{
    if (access(LANG_FILE, R_OK) == 0)
        return load_languages(LANG_FILE);

    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash != NULL) {
            *slash = '\0';
            char path[PATH_MAX + 16];
            snprintf(path, sizeof(path), "%s/languages.conf", exe);
            if (access(path, R_OK) == 0)
                return load_languages(path);
        }
    }
    fprintf(stderr, "yalocc: language file not found: %s (use --lang-file)\n", LANG_FILE);
    return -1;
}

static void list_languages(void)
{
    for (size_t i = 0; i < language_count(); i++)
        printf("%s\n", language_by_index(i)->name);
    printf("Unknown\n");
}

int main(int argc, char **argv)
{
    const char *paths[argc];
    size_t npaths = 0;
    bool by_file = false;
    const char *skip_dirs[argc];
    size_t nskip_dirs = 0;
    const char *include_langs[argc];
    size_t ninclude_langs = 0;
    const char *exclude_langs[argc];
    size_t nexclude_langs = 0;
    size_t threads = default_thread_count();
    const char *lang_file = NULL;
    bool list_langs = false;
    bool use_gitignore = true;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout);
            return 0;
        }
        if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            printf("yalocc %s\n", VERSION);
            return 0;
        }
        if (strcmp(arg, "--by-file") == 0) {
            by_file = true;
            continue;
        }
        if (strcmp(arg, "--no-gitignore") == 0) {
            use_gitignore = false;
            continue;
        }
        if (strcmp(arg, "--list-languages") == 0) {
            list_langs = true;
            continue;
        }
        const char *eq = strncmp(arg, "--", 2) == 0 ? strchr(arg, '=') : NULL;
        size_t namelen = eq != NULL ? (size_t)(eq - arg) : strlen(arg);
        const char *inline_value = eq != NULL ? eq + 1 : NULL;
        const char *value;
        if (option_is(arg, namelen, "--lang-file")) {
            if (!take_value(argc, argv, &i, namelen, inline_value, &value))
                return fail_hint();
            lang_file = value;
            continue;
        }
        if (option_is(arg, namelen, "--threads")) {
            if (!take_value(argc, argv, &i, namelen, inline_value, &value))
                return fail_hint();
            char *end;
            long parsed = strtol(value, &end, 10);
            if (end == value || *end != '\0' || parsed < 1) {
                fprintf(stderr, "yalocc: invalid thread count: %s\n", value);
                return 1;
            }
            threads = (size_t)parsed;
            continue;
        }
        if (option_is(arg, namelen, "--exclude-dir")) {
            if (!take_value(argc, argv, &i, namelen, inline_value, &value))
                return fail_hint();
            skip_dirs[nskip_dirs++] = value;
            continue;
        }
        if (option_is(arg, namelen, "--include-lang")) {
            if (!take_value(argc, argv, &i, namelen, inline_value, &value))
                return fail_hint();
            include_langs[ninclude_langs++] = value;
            continue;
        }
        if (option_is(arg, namelen, "--exclude-lang")) {
            if (!take_value(argc, argv, &i, namelen, inline_value, &value))
                return fail_hint();
            exclude_langs[nexclude_langs++] = value;
            continue;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "yalocc: unknown option: %s\n", arg);
            return fail_hint();
        }
        paths[npaths++] = arg;
    }

    if (npaths == 0)
        paths[npaths++] = ".";

    int load_rc = lang_file != NULL ? load_languages(lang_file) : load_default_languages();
    if (load_rc != 0)
        return 1;
    if (list_langs) {
        list_languages();
        return 0;
    }
    for (size_t i = 0; i < ninclude_langs; i++) {
        if (!language_name_exists(include_langs[i])) {
            fprintf(stderr, "yalocc: unknown language: %s (see --list-languages)\n", include_langs[i]);
            return 1;
        }
    }
    for (size_t i = 0; i < nexclude_langs; i++) {
        if (!language_name_exists(exclude_langs[i])) {
            fprintf(stderr, "yalocc: unknown language: %s (see --list-languages)\n", exclude_langs[i]);
            return 1;
        }
    }

    struct scan_config cfg = {
        .paths = paths,
        .npaths = npaths,
        .skip_dirs = skip_dirs,
        .nskip_dirs = nskip_dirs,
        .use_gitignore = use_gitignore,
        .include_langs = include_langs,
        .ninclude_langs = ninclude_langs,
        .exclude_langs = exclude_langs,
        .nexclude_langs = nexclude_langs,
        .threads = threads,
    };
    struct scan_result res;
    if (scan_run(&cfg, &res) != 0)
        return 1;

    print_report(res.files, res.nfiles, res.totals, by_file);

    int rc = res.walk_rc == 0 ? 0 : 1;
    scan_result_free(&res);
    return rc;
}
