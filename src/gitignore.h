/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef YALOCC_GITIGNORE_H
#define YALOCC_GITIGNORE_H

#include <stdbool.h>
#include <stddef.h>

struct ignore_rule {
    char *pattern;
    size_t base_len;
    bool negate;
    bool dir_only;
    bool any_depth;
};

struct ignore_set {
    struct ignore_rule *rules;
    size_t count;
    size_t cap;
};

int ignore_load(struct ignore_set *set, const char *dir);
void ignore_truncate(struct ignore_set *set, size_t count);
void ignore_free(struct ignore_set *set);
bool ignore_matches(const struct ignore_set *set, const char *path, bool is_dir);

#endif
