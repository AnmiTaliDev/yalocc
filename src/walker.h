/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef YALOCC_WALKER_H
#define YALOCC_WALKER_H

#include <stddef.h>

typedef int (*yalocc_file_cb)(void *ctx, const char *path);

int walk_paths(const char *const *paths, size_t count, yalocc_file_cb cb, void *ctx);

#endif
