/* SPDX-License-Identifier: GPL-3.0-only */
#include "walker.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gitignore.h"

#define PATH_BUF_SIZE 4096

static const char *const skipped_dirs[] = {".git", ".hg", ".svn", ".bzr", ".cvs", NULL};

static bool dir_is_skipped(const char *name, const char *const *skip_dirs, size_t nskip_dirs)
{
    for (size_t i = 0; skipped_dirs[i] != NULL; i++) {
        if (strcmp(name, skipped_dirs[i]) == 0)
            return true;
    }
    for (size_t i = 0; i < nskip_dirs; i++) {
        if (strcmp(name, skip_dirs[i]) == 0)
            return true;
    }
    return false;
}

static int walk_dir(const char *path, const char *const *skip_dirs, size_t nskip_dirs,
                    struct ignore_set *ignores, yalocc_file_cb cb, void *ctx)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "yalocc: cannot open directory %s: %s\n", path, strerror(errno));
        return 0;
    }

    size_t ignore_mark = 0;
    int rc = 0;
    if (ignores != NULL) {
        ignore_mark = ignores->count;
        rc = ignore_load(ignores, path);
    }
    struct dirent *entry;
    while (rc == 0 && (entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (dir_is_skipped(entry->d_name, skip_dirs, nskip_dirs))
            continue;

        char child[PATH_BUF_SIZE];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child)) {
            fprintf(stderr, "yalocc: path too long: %s/%s\n", path, entry->d_name);
            continue;
        }

        struct stat st;
        if (lstat(child, &st) == -1) {
            fprintf(stderr, "yalocc: cannot stat %s: %s\n", child, strerror(errno));
            continue;
        }

        if (ignores != NULL && ignore_matches(ignores, child, S_ISDIR(st.st_mode)))
            continue;

        if (S_ISDIR(st.st_mode))
            rc = walk_dir(child, skip_dirs, nskip_dirs, ignores, cb, ctx);
        else if (S_ISREG(st.st_mode))
            rc = cb(ctx, child);
    }

    closedir(dir);
    if (ignores != NULL)
        ignore_truncate(ignores, ignore_mark);
    return rc;
}

int walk_paths(const char *const *paths, size_t count, const char *const *skip_dirs,
               size_t nskip_dirs, bool use_gitignore, yalocc_file_cb cb, void *ctx)
{
    struct ignore_set ignore_storage = {0};
    struct ignore_set *ignores = use_gitignore ? &ignore_storage : NULL;
    int status = 0;
    for (size_t i = 0; i < count; i++) {
        struct stat st;
        if (lstat(paths[i], &st) == -1) {
            fprintf(stderr, "yalocc: cannot stat %s: %s\n", paths[i], strerror(errno));
            status = -1;
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (walk_dir(paths[i], skip_dirs, nskip_dirs, ignores, cb, ctx) != 0)
                status = -1;
        } else if (S_ISREG(st.st_mode)) {
            if (cb(ctx, paths[i]) != 0)
                status = -1;
        } else {
            fprintf(stderr, "yalocc: not a regular file or directory: %s\n", paths[i]);
            status = -1;
        }
    }
    ignore_free(&ignore_storage);
    return status;
}
