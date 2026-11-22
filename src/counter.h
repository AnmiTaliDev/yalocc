/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef YALOCC_COUNTER_H
#define YALOCC_COUNTER_H

#include "languages.h"

struct line_counts {
    long blank;
    long comment;
    long code;
};

int count_file(const char *path, const struct language *lang, struct line_counts *out);

#endif
