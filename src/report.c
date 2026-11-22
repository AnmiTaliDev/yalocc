/* SPDX-License-Identifier: GPL-3.0-only */
#include "report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "languages.h"

struct report_entry {
    const char *name;
    size_t slot;
    struct totals totals;
};

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

static void print_file_rows(const struct file_result *results, size_t nresults, size_t slot,
                            size_t name_w, size_t files_w, size_t blank_w, size_t comment_w, size_t code_w)
{
    size_t count = 0;
    for (size_t i = 0; i < nresults; i++) {
        if (results[i].counted && results[i].slot == slot)
            count++;
    }
    if (count == 0)
        return;

    const struct file_result *rows[count];
    size_t k = 0;
    for (size_t i = 0; i < nresults; i++) {
        if (results[i].counted && results[i].slot == slot)
            rows[k++] = &results[i];
    }
    qsort(rows, count, sizeof(rows[0]), file_cmp);

    for (size_t i = 0; i < count; i++) {
        const struct file_result *fr = rows[i];
        printf(" %-*s %*ld %*ld %*ld %*ld\n", (int)name_w - 1, display_path(fr->path),
               (int)files_w, 1L, (int)blank_w, fr->blank, (int)comment_w, fr->comment,
               (int)code_w, fr->code);
    }
}

void print_report(const struct file_result *results, size_t nresults, const struct totals *totals,
                  bool by_file)
{
    size_t nlangs = language_count();

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
    if (by_file) {
        for (size_t i = 0; i < nresults; i++) {
            if (!results[i].counted)
                continue;
            size_t w = strlen(display_path(results[i].path)) + 1;
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
        if (by_file)
            print_file_rows(results, nresults, entries[i].slot, name_w, files_w, blank_w, comment_w, code_w);
    }
    print_dashes(table_w);
    printf("%-*s %*ld %*ld %*ld %*ld\n", (int)name_w, "SUM:", (int)files_w, sum.files,
           (int)blank_w, sum.blank, (int)comment_w, sum.comment, (int)code_w, sum.code);
    print_dashes(table_w);
}
