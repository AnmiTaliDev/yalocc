/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

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
    bool counted;
    bool binary;
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
    struct file_result *results;
    size_t nresults;
    size_t results_cap;
    size_t threads;
    size_t next_file;
    pthread_mutex_t queue_lock;
    struct totals *thread_totals;
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
    if (strcasecmp(name, "Unknown") == 0)
        return true;
    for (size_t i = 0; i < language_count(); i++) {
        if (strcasecmp(language_by_index(i)->name, name) == 0)
            return true;
    }
    return false;
}

static bool lang_allowed(const struct counter_ctx *ctx, size_t slot)
{
    const char *name = slot == language_count() ? "Unknown" : language_by_index(slot)->name;
    if (slot == language_count() && ctx->ninclude_langs == 0)
        return false;
    if (ctx->ninclude_langs > 0) {
        bool found = false;
        for (size_t i = 0; i < ctx->ninclude_langs; i++) {
            if (strcasecmp(name, ctx->include_langs[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    for (size_t i = 0; i < ctx->nexclude_langs; i++) {
        if (strcasecmp(name, ctx->exclude_langs[i]) == 0)
            return false;
    }
    return true;
}

static int collect_file(void *ctxp, const char *path)
{
    struct counter_ctx *ctx = ctxp;
    const struct language *lang = find_language(path);
    if (lang == NULL)
        lang = &unknown_language;
    size_t slot = slot_for(ctx, lang);
    if (!lang_allowed(ctx, slot))
        return 0;

    if (ctx->nresults == ctx->results_cap) {
        size_t new_cap = ctx->results_cap == 0 ? 256 : ctx->results_cap * 2;
        struct file_result *grown = realloc(ctx->results, new_cap * sizeof(ctx->results[0]));
        if (grown == NULL) {
            fprintf(stderr, "yalocc: out of memory\n");
            return -1;
        }
        ctx->results = grown;
        ctx->results_cap = new_cap;
    }
    struct file_result *fr = &ctx->results[ctx->nresults];
    fr->path = strdup(path);
    if (fr->path == NULL) {
        fprintf(stderr, "yalocc: out of memory\n");
        return -1;
    }
    fr->slot = slot;
    fr->counted = false;
    fr->binary = false;
    fr->blank = 0;
    fr->comment = 0;
    fr->code = 0;
    ctx->nresults++;
    return 0;
}

static void run_one(struct counter_ctx *ctx, size_t tid, size_t idx)
{
    struct file_result *fr = &ctx->results[idx];
    const struct language *lang = (fr->slot == language_count()) ? &unknown_language
        : language_by_index(fr->slot);

    struct line_counts counts;
    int rc = count_file(fr->path, lang, &counts);
    if (rc == 1) {
        fr->binary = true;
        return;
    }
    if (rc == -1) {
        fprintf(stderr, "yalocc: cannot read %s: %s\n", fr->path, strerror(errno));
        return;
    }

    fr->counted = true;
    fr->blank = counts.blank;
    fr->comment = counts.comment;
    fr->code = counts.code;

    struct totals *t = &ctx->thread_totals[tid * ctx->slots + fr->slot];
    t->files++;
    t->blank += counts.blank;
    t->comment += counts.comment;
    t->code += counts.code;
}

struct worker_arg {
    struct counter_ctx *ctx;
    size_t tid;
};

static void *worker_main(void *argp)
{
    struct worker_arg *arg = argp;
    struct counter_ctx *ctx = arg->ctx;
    for (;;) {
        pthread_mutex_lock(&ctx->queue_lock);
        size_t idx = ctx->next_file;
        if (idx < ctx->nresults)
            ctx->next_file++;
        pthread_mutex_unlock(&ctx->queue_lock);
        if (idx >= ctx->nresults)
            break;
        run_one(ctx, arg->tid, idx);
    }
    return NULL;
}

static void run_workers(struct counter_ctx *ctx)
{
    if (ctx->nresults == 0)
        return;

    size_t nworkers = ctx->threads < ctx->nresults ? ctx->threads : ctx->nresults;
    ctx->next_file = 0;
    pthread_mutex_init(&ctx->queue_lock, NULL);

    struct worker_arg *args = malloc(nworkers * sizeof(args[0]));
    pthread_t *tids = malloc((nworkers - 1) * sizeof(tids[0]));
    if (args == NULL || tids == NULL) {
        fprintf(stderr, "yalocc: out of memory\n");
        free(args);
        free(tids);
        nworkers = 1;
        args = NULL;
        tids = NULL;
    }

    size_t spawned = 0;
    for (size_t i = 1; i < nworkers; i++) {
        args[i].ctx = ctx;
        args[i].tid = i;
        if (pthread_create(&tids[i - 1], NULL, worker_main, &args[i]) != 0) {
            fprintf(stderr, "yalocc: cannot create thread, reducing parallelism\n");
            break;
        }
        spawned++;
    }

    struct worker_arg main_arg = {ctx, 0};
    worker_main(&main_arg);

    for (size_t i = 0; i < spawned; i++)
        pthread_join(tids[i], NULL);

    free(args);
    free(tids);
    pthread_mutex_destroy(&ctx->queue_lock);
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
    for (size_t i = 0; i < ctx->nresults; i++) {
        if (ctx->results[i].counted && ctx->results[i].slot == slot)
            count++;
    }
    if (count == 0)
        return;

    struct file_result *rows[count];
    size_t k = 0;
    for (size_t i = 0; i < ctx->nresults; i++) {
        if (ctx->results[i].counted && ctx->results[i].slot == slot)
            rows[k++] = &ctx->results[i];
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
        for (size_t i = 0; i < ctx->nresults; i++) {
            if (!ctx->results[i].counted)
                continue;
            size_t w = strlen(display_path(ctx->results[i].path)) + 1;
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
        .threads = threads,
    };

    int walk_rc = walk_paths(paths, npaths, skip_dirs, nskip_dirs, use_gitignore, collect_file, &ctx);

    struct totals thread_totals[threads * (nlangs + 1)];
    memset(thread_totals, 0, sizeof(thread_totals));
    ctx.thread_totals = thread_totals;

    run_workers(&ctx);

    for (size_t i = 0; i < threads; i++) {
        for (size_t j = 0; j <= nlangs; j++) {
            totals[j].files += thread_totals[i * (nlangs + 1) + j].files;
            totals[j].blank += thread_totals[i * (nlangs + 1) + j].blank;
            totals[j].comment += thread_totals[i * (nlangs + 1) + j].comment;
            totals[j].code += thread_totals[i * (nlangs + 1) + j].code;
        }
    }

    size_t nbinary = 0;
    for (size_t i = 0; i < ctx.nresults; i++) {
        if (ctx.results[i].binary)
            nbinary++;
    }
    if (nbinary > 0)
        fprintf(stderr, "yalocc: skipped %zu binary file%s\n", nbinary, nbinary == 1 ? "" : "s");

    print_report(&ctx);

    for (size_t i = 0; i < ctx.nresults; i++)
        free(ctx.results[i].path);
    free(ctx.results);
    return walk_rc == 0 ? 0 : 1;
}
