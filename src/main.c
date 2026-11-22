/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "counter.h"
#include "languages.h"
#include "walker.h"

struct totals {
    long files;
    long blank;
    long comment;
    long code;
};

struct report_entry {
    const char *name;
    size_t slot;
    struct totals totals;
};

struct file_result {
    char *path;
    size_t slot;
    long blank;
    long comment;
    long code;
};

struct counter_ctx {
    size_t slots;
    struct totals *totals;
    bool by_file;
    const char *const *include_langs;
    size_t ninclude_langs;
    const char *const *exclude_langs;
    size_t nexclude_langs;
    struct file_result *files;
    size_t nfiles;
    size_t files_cap;
};

static const struct language unknown_language = {
    "Unknown", NULL, NULL, NULL, false, false, NULL, NULL,
};

static size_t slot_for(struct counter_ctx *ctx, const struct language *lang)
{
    for (size_t i = 0; i < language_count(); i++) {
        if (language_by_index(i) == lang)
            return i;
    }
    return ctx->slots - 1;
}

static bool language_name_exists(const char *name)
{
    if (strcmp(name, "Unknown") == 0)
        return true;
    for (size_t i = 0; i < language_count(); i++) {
        if (strcmp(language_by_index(i)->name, name) == 0)
            return true;
    }
    return false;
}

static bool lang_allowed(const struct counter_ctx *ctx, size_t slot)
{
    const char *name = slot == language_count() ? "Unknown" : language_by_index(slot)->name;
    if (ctx->ninclude_langs > 0) {
        bool found = false;
        for (size_t i = 0; i < ctx->ninclude_langs; i++) {
            if (strcmp(name, ctx->include_langs[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    for (size_t i = 0; i < ctx->nexclude_langs; i++) {
        if (strcmp(name, ctx->exclude_langs[i]) == 0)
            return false;
    }
    return true;
}

static int on_file(void *ctxp, const char *path)
{
    struct counter_ctx *ctx = ctxp;
    const struct language *lang = find_language(path);
    if (lang == NULL)
        lang = &unknown_language;
    size_t slot = slot_for(ctx, lang);
    if (!lang_allowed(ctx, slot))
        return 0;

    struct line_counts counts;
    int rc = count_file(path, lang, &counts);
    if (rc == 1) {
        fprintf(stderr, "yalocc: skipping binary file: %s\n", path);
        return 0;
    }
    if (rc == -1) {
        fprintf(stderr, "yalocc: cannot read %s: %s\n", path, strerror(errno));
        return 0;
    }

    struct totals *t = &ctx->totals[slot];
    t->files++;
    t->blank += counts.blank;
    t->comment += counts.comment;
    t->code += counts.code;

    if (ctx->by_file) {
        if (ctx->nfiles == ctx->files_cap) {
            size_t new_cap = ctx->files_cap == 0 ? 64 : ctx->files_cap * 2;
            struct file_result *grown = realloc(ctx->files, new_cap * sizeof(ctx->files[0]));
            if (grown == NULL) {
                fprintf(stderr, "yalocc: out of memory\n");
                return -1;
            }
            ctx->files = grown;
            ctx->files_cap = new_cap;
        }
        struct file_result *fr = &ctx->files[ctx->nfiles];
        fr->path = strdup(path);
        if (fr->path == NULL) {
            fprintf(stderr, "yalocc: out of memory\n");
            return -1;
        }
        fr->slot = slot;
        fr->blank = counts.blank;
        fr->comment = counts.comment;
        fr->code = counts.code;
        ctx->nfiles++;
    }
    return 0;
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
}

static const char *display_path(const char *path)
{
    return strncmp(path, "./", 2) == 0 ? path + 2 : path;
}

static size_t num_width(long value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", value);
    return strlen(buf);
}

static int entry_cmp(const void *a, const void *b)
{
    const struct report_entry *ea = a;
    const struct report_entry *eb = b;
    return strcmp(ea->name, eb->name);
}

static int file_cmp(const void *a, const void *b)
{
    const struct file_result *const *fa = a;
    const struct file_result *const *fb = b;
    return strcmp(display_path((*fa)->path), display_path((*fb)->path));
}

static void print_dashes(size_t width)
{
    for (size_t i = 0; i < width; i++)
        putchar('-');
    putchar('\n');
}

static void print_file_rows(struct counter_ctx *ctx, size_t slot, size_t name_w,
                            size_t files_w, size_t blank_w, size_t comment_w, size_t code_w)
{
    size_t count = 0;
    for (size_t i = 0; i < ctx->nfiles; i++) {
        if (ctx->files[i].slot == slot)
            count++;
    }
    if (count == 0)
        return;

    struct file_result *rows[count];
    size_t k = 0;
    for (size_t i = 0; i < ctx->nfiles; i++) {
        if (ctx->files[i].slot == slot)
            rows[k++] = &ctx->files[i];
    }
    qsort(rows, count, sizeof(rows[0]), file_cmp);

    for (size_t i = 0; i < count; i++) {
        const struct file_result *fr = rows[i];
        printf(" %-*s %*ld %*ld %*ld %*ld\n", (int)name_w - 1, display_path(fr->path),
               (int)files_w, 1L, (int)blank_w, fr->blank, (int)comment_w, fr->comment,
               (int)code_w, fr->code);
    }
}

static void print_report(struct counter_ctx *ctx)
{
    size_t nlangs = language_count();
    struct totals *totals = ctx->totals;

    struct report_entry entries[nlangs + 1];
    size_t nentries = 0;

    for (size_t i = 0; i < nlangs; i++) {
        if (totals[i].files > 0) {
            entries[nentries].name = language_by_index(i)->name;
            entries[nentries].slot = i;
            entries[nentries].totals = totals[i];
            nentries++;
        }
    }
    if (totals[nlangs].files > 0) {
        entries[nentries].name = "Unknown";
        entries[nentries].slot = nlangs;
        entries[nentries].totals = totals[nlangs];
        nentries++;
    }

    if (nentries == 0) {
        fprintf(stderr, "yalocc: no source files found\n");
        return;
    }

    qsort(entries, nentries, sizeof(entries[0]), entry_cmp);

    struct totals sum = {0, 0, 0, 0};
    for (size_t i = 0; i < nentries; i++) {
        sum.files += entries[i].totals.files;
        sum.blank += entries[i].totals.blank;
        sum.comment += entries[i].totals.comment;
        sum.code += entries[i].totals.code;
    }

    size_t name_w = strlen("Language");
    size_t files_w = strlen("Files");
    size_t blank_w = strlen("Blank");
    size_t comment_w = strlen("Comment");
    size_t code_w = strlen("Code");

    for (size_t i = 0; i < nentries; i++) {
        size_t w = strlen(entries[i].name);
        if (w > name_w)
            name_w = w;
        w = num_width(entries[i].totals.files);
        if (w > files_w)
            files_w = w;
        w = num_width(entries[i].totals.blank);
        if (w > blank_w)
            blank_w = w;
        w = num_width(entries[i].totals.comment);
        if (w > comment_w)
            comment_w = w;
        w = num_width(entries[i].totals.code);
        if (w > code_w)
            code_w = w;
    }
    if (ctx->by_file) {
        for (size_t i = 0; i < ctx->nfiles; i++) {
            size_t w = strlen(display_path(ctx->files[i].path)) + 1;
            if (w > name_w)
                name_w = w;
        }
    }
    if (num_width(sum.files) > files_w)
        files_w = num_width(sum.files);
    if (num_width(sum.blank) > blank_w)
        blank_w = num_width(sum.blank);
    if (num_width(sum.comment) > comment_w)
        comment_w = num_width(sum.comment);
    if (num_width(sum.code) > code_w)
        code_w = num_width(sum.code);

    size_t table_w = name_w + files_w + blank_w + comment_w + code_w + 4;

    print_dashes(table_w);
    printf("%-*s %*s %*s %*s %*s\n", (int)name_w, "Language", (int)files_w, "Files",
           (int)blank_w, "Blank", (int)comment_w, "Comment", (int)code_w, "Code");
    print_dashes(table_w);
    for (size_t i = 0; i < nentries; i++) {
        printf("%-*s %*ld %*ld %*ld %*ld\n", (int)name_w, entries[i].name,
               (int)files_w, entries[i].totals.files, (int)blank_w, entries[i].totals.blank,
               (int)comment_w, entries[i].totals.comment, (int)code_w, entries[i].totals.code);
        if (ctx->by_file)
            print_file_rows(ctx, entries[i].slot, name_w, files_w, blank_w, comment_w, code_w);
    }
    print_dashes(table_w);
    printf("%-*s %*ld %*ld %*ld %*ld\n", (int)name_w, "SUM:", (int)files_w, sum.files,
           (int)blank_w, sum.blank, (int)comment_w, sum.comment, (int)code_w, sum.code);
    print_dashes(table_w);
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
        if (strcmp(arg, "--exclude-dir") == 0 || strcmp(arg, "--include-lang") == 0 ||
            strcmp(arg, "--exclude-lang") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yalocc: missing argument for %s\n", arg);
                usage(stderr);
                return 1;
            }
            const char *value = argv[++i];
            if (strcmp(arg, "--exclude-dir") == 0) {
                skip_dirs[nskip_dirs++] = value;
                continue;
            }
            if (!language_name_exists(value)) {
                fprintf(stderr, "yalocc: unknown language: %s\n", value);
                return 1;
            }
            if (strcmp(arg, "--include-lang") == 0)
                include_langs[ninclude_langs++] = value;
            else
                exclude_langs[nexclude_langs++] = value;
            continue;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "yalocc: unknown option: %s\n", arg);
            usage(stderr);
            return 1;
        }
        paths[npaths++] = arg;
    }

    if (npaths == 0)
        paths[npaths++] = ".";

    size_t nlangs = language_count();
    struct totals totals[nlangs + 1];
    memset(totals, 0, sizeof(totals));

    struct counter_ctx ctx = {
        .slots = nlangs + 1,
        .totals = totals,
        .by_file = by_file,
        .include_langs = include_langs,
        .ninclude_langs = ninclude_langs,
        .exclude_langs = exclude_langs,
        .nexclude_langs = nexclude_langs,
    };
    int walk_rc = walk_paths(paths, npaths, skip_dirs, nskip_dirs, on_file, &ctx);

    print_report(&ctx);

    for (size_t i = 0; i < ctx.nfiles; i++)
        free(ctx.files[i].path);
    free(ctx.files);
    return walk_rc == 0 ? 0 : 1;
}
