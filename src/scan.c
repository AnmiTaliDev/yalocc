/* SPDX-License-Identifier: GPL-3.0-only */
#include "scan.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "counter.h"
#include "languages.h"
#include "walker.h"

struct counter_ctx {
    size_t slots;
    struct totals *totals;
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

static void merge_totals(struct totals *dst, const struct totals *src, size_t nslots, size_t nthreads)
{
    for (size_t i = 0; i < nthreads; i++) {
        for (size_t j = 0; j < nslots; j++) {
            dst[j].files += src[i * nslots + j].files;
            dst[j].blank += src[i * nslots + j].blank;
            dst[j].comment += src[i * nslots + j].comment;
            dst[j].code += src[i * nslots + j].code;
        }
    }
}

int scan_run(const struct scan_config *cfg, struct scan_result *out)
{
    size_t nslots = language_count() + 1;
    memset(out, 0, sizeof(*out));
    out->totals = calloc(nslots, sizeof(out->totals[0]));
    struct totals *thread_totals = calloc(cfg->threads * nslots, sizeof(thread_totals[0]));
    if (out->totals == NULL || thread_totals == NULL) {
        fprintf(stderr, "yalocc: out of memory\n");
        free(out->totals);
        free(thread_totals);
        return -1;
    }

    struct counter_ctx ctx = {
        .slots = nslots,
        .totals = out->totals,
        .include_langs = cfg->include_langs,
        .ninclude_langs = cfg->ninclude_langs,
        .exclude_langs = cfg->exclude_langs,
        .nexclude_langs = cfg->nexclude_langs,
        .threads = cfg->threads,
        .thread_totals = thread_totals,
    };

    out->walk_rc = walk_paths(cfg->paths, cfg->npaths, cfg->skip_dirs, cfg->nskip_dirs,
                              cfg->use_gitignore, collect_file, &ctx);

    run_workers(&ctx);
    merge_totals(out->totals, thread_totals, nslots, cfg->threads);
    free(thread_totals);

    size_t nbinary = 0;
    for (size_t i = 0; i < ctx.nresults; i++) {
        if (ctx.results[i].binary)
            nbinary++;
    }
    if (nbinary > 0)
        fprintf(stderr, "yalocc: skipped %zu binary file%s\n", nbinary, nbinary == 1 ? "" : "s");

    out->files = ctx.results;
    out->nfiles = ctx.nresults;
    return 0;
}

void scan_result_free(struct scan_result *res)
{
    for (size_t i = 0; i < res->nfiles; i++)
        free(res->files[i].path);
    free(res->files);
    free(res->totals);
}
