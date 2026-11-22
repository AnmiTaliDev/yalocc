/* SPDX-License-Identifier: GPL-3.0-only */
#include "counter.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static bool starts_with(const char *line, size_t len, size_t pos, const char *token)
{
    size_t token_len = strlen(token);
    if (pos + token_len > len)
        return false;
    return memcmp(line + pos, token, token_len) == 0;
}

int count_file(const char *path, const struct language *lang, struct line_counts *out)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return -1;

    long blank = 0;
    long comment = 0;
    long code = 0;
    bool in_block = false;
    bool in_doc = false;
    char in_string = 0;
    char doc_quote = 0;
    char *line = NULL;
    size_t cap = 0;
    ssize_t nread;

    while ((nread = getline(&line, &cap, file)) != -1) {
        size_t len = (size_t)nread;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            len--;

        if (memchr(line, '\0', len) != NULL) {
            free(line);
            fclose(file);
            return 1;
        }

        bool has_code = false;
        bool has_comment = false;
        size_t i = 0;

        while (i < len) {
            char c = line[i];

            if (in_block) {
                has_comment = true;
                if (starts_with(line, len, i, lang->block_end)) {
                    in_block = false;
                    i += strlen(lang->block_end);
                } else {
                    i++;
                }
                continue;
            }

            if (in_doc) {
                has_comment = true;
                if (c == doc_quote && i + 2 < len && line[i + 1] == doc_quote && line[i + 2] == doc_quote) {
                    in_doc = false;
                    i += 3;
                } else {
                    i++;
                }
                continue;
            }

            if (in_string) {
                if (c == '\\') {
                    i += 2;
                    continue;
                }
                if (c == in_string) {
                    in_string = 0;
                    i++;
                    continue;
                }
                has_code = true;
                i++;
                continue;
            }

            if (lang->docstrings && (c == '"' || c == '\'') && i + 2 < len && line[i + 1] == c && line[i + 2] == c) {
                in_doc = true;
                doc_quote = c;
                has_comment = true;
                i += 3;
                continue;
            }

            if (lang->block_start != NULL && starts_with(line, len, i, lang->block_start)) {
                in_block = true;
                has_comment = true;
                i += strlen(lang->block_start);
                continue;
            }

            if (lang->line_comment != NULL && starts_with(line, len, i, lang->line_comment)) {
                has_comment = true;
                break;
            }

            if (lang->track_strings && (c == '"' || c == '\'')) {
                in_string = c;
                i++;
                continue;
            }

            if (!isspace((unsigned char)c))
                has_code = true;
            i++;
        }

        if (in_string)
            has_code = true;

        if (!has_code && !has_comment)
            blank++;
        else if (has_code)
            code++;
        else
            comment++;
    }

    free(line);
    fclose(file);
    out->blank = blank;
    out->comment = comment;
    out->code = code;
    return 0;
}
