/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef YALOCC_SCAN_H
#define YALOCC_SCAN_H

#include <stdbool.h>
#include <stddef.h>

#include "results.h"

struct scan_config {
    const char *const *paths;
    size_t npaths;
    const char *const *skip_dirs;
    size_t nskip_dirs;
    bool use_gitignore;
    const char *const *include_langs;
    size_t ninclude_langs;
    const char *const *exclude_langs;
    size_t nexclude_langs;
    size_t threads;
};

struct scan_result {
    struct file_result *files;
    size_t nfiles;
    struct totals *totals;
    int walk_rc;
};

int scan_run(const struct scan_config *cfg, struct scan_result *out);
void scan_result_free(struct scan_result *res);

#endif
