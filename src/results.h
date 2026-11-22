/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef YALOCC_RESULTS_H
#define YALOCC_RESULTS_H

#include <stdbool.h>
#include <stddef.h>

struct totals {
    long files;
    long blank;
    long comment;
    long code;
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

#endif
