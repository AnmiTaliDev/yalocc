/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef YALOCC_LANGUAGES_H
#define YALOCC_LANGUAGES_H

#include <stdbool.h>
#include <stddef.h>

struct language {
    const char *name;
    const char *line_comment;
    const char *block_start;
    const char *block_end;
    bool track_strings;
    bool docstrings;
    const char *const *exts;
};

const struct language *find_language(const char *path);
const struct language *language_by_index(size_t index);
size_t language_count(void);

#endif
