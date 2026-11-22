/* SPDX-License-Identifier: GPL-3.0-only */
#include "languages.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct language *languages;
static size_t nlanguages;
static size_t languages_cap;

static char *next_token(char **cursor)
{
    char *p = *cursor;
    while (isspace((unsigned char)*p))
        p++;
    if (*p == '\0') {
        *cursor = p;
        return NULL;
    }
    char *start = p;
    while (*p != '\0' && !isspace((unsigned char)*p))
        p++;
    if (*p != '\0')
        *p++ = '\0';
    *cursor = p;
    return start;
}

static char *copy_string(const char *s)
{
    char *copy = strdup(s);
    if (copy == NULL)
        fprintf(stderr, "yalocc: out of memory\n");
    return copy;
}

static struct language *add_language(const char *name)
{
    if (nlanguages == languages_cap) {
        size_t new_cap = languages_cap == 0 ? 32 : languages_cap * 2;
        struct language *grown = realloc(languages, new_cap * sizeof(languages[0]));
        if (grown == NULL) {
            fprintf(stderr, "yalocc: out of memory\n");
            return NULL;
        }
        languages = grown;
        languages_cap = new_cap;
    }
    struct language *lang = &languages[nlanguages];
    memset(lang, 0, sizeof(*lang));
    lang->name = copy_string(name);
    if (lang->name == NULL)
        return NULL;
    nlanguages++;
    return lang;
}

static int append_names(const char *const **list, char *args)
{
    size_t count = 0;
    if (*list != NULL) {
        while ((*list)[count] != NULL)
            count++;
    }
    char *tok;
    while ((tok = next_token(&args)) != NULL) {
        const char **grown = realloc((void *)*list, (count + 2) * sizeof(grown[0]));
        if (grown == NULL) {
            fprintf(stderr, "yalocc: out of memory\n");
            return -1;
        }
        grown[count] = copy_string(tok);
        if (grown[count] == NULL)
            return -1;
        grown[count + 1] = NULL;
        count++;
        *list = grown;
    }
    return 0;
}

static int parse_bool(const char *s, bool *out)
{
    if (strcmp(s, "yes") == 0)
        *out = true;
    else if (strcmp(s, "no") == 0)
        *out = false;
    else
        return -1;
    return 0;
}

static int apply_line(struct language *lang, const char *key, char *args, const char *path, int lineno)
{
    if (strcmp(key, "ext") == 0)
        return append_names(&lang->exts, args);
    if (strcmp(key, "file") == 0)
        return append_names(&lang->filenames, args);
    if (strcmp(key, "line_comment") == 0) {
        char *tok = next_token(&args);
        if (tok == NULL)
            goto missing;
        lang->line_comment = copy_string(tok);
        return lang->line_comment == NULL ? -1 : 0;
    }
    if (strcmp(key, "block") == 0) {
        char *start = next_token(&args);
        char *end = next_token(&args);
        if (start == NULL || end == NULL)
            goto missing;
        lang->block_start = copy_string(start);
        lang->block_end = copy_string(end);
        return lang->block_start == NULL || lang->block_end == NULL ? -1 : 0;
    }
    if (strcmp(key, "strings") == 0 || strcmp(key, "docstrings") == 0) {
        char *tok = next_token(&args);
        bool value;
        if (tok == NULL)
            goto missing;
        if (parse_bool(tok, &value) != 0) {
            fprintf(stderr, "yalocc: %s:%d: expected yes or no: %s\n", path, lineno, tok);
            return -1;
        }
        if (strcmp(key, "strings") == 0)
            lang->track_strings = value;
        else
            lang->docstrings = value;
        return 0;
    }
    fprintf(stderr, "yalocc: %s:%d: unknown key: %s\n", path, lineno, key);
    return -1;

missing:
    fprintf(stderr, "yalocc: %s:%d: missing value for %s\n", path, lineno, key);
    return -1;
}

int load_languages(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "yalocc: cannot open language file %s\n", path);
        return -1;
    }

    char line[1024];
    int lineno = 0;
    struct language *current = NULL;
    while (fgets(line, sizeof(line), fp) != NULL) {
        lineno++;
        size_t len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1]))
            line[--len] = '\0';

        char *cursor = line;
        char *key = next_token(&cursor);
        if (key == NULL || key[0] == '#')
            continue;

        if (strcmp(key, "language") == 0) {
            while (isspace((unsigned char)*cursor))
                cursor++;
            if (*cursor == '\0') {
                fprintf(stderr, "yalocc: %s:%d: missing language name\n", path, lineno);
                fclose(fp);
                return -1;
            }
            current = add_language(cursor);
            if (current == NULL) {
                fclose(fp);
                return -1;
            }
            continue;
        }
        if (current == NULL) {
            fprintf(stderr, "yalocc: %s:%d: %s before any language\n", path, lineno, key);
            fclose(fp);
            return -1;
        }
        if (apply_line(current, key, cursor, path, lineno) != 0) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}

const struct language *find_language(const char *path)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    for (size_t i = 0; i < nlanguages; i++) {
        if (languages[i].filenames == NULL)
            continue;
        for (size_t j = 0; languages[i].filenames[j] != NULL; j++) {
            if (strcmp(base, languages[i].filenames[j]) == 0)
                return &languages[i];
        }
    }
    const char *dot = strrchr(base, '.');
    if (dot == NULL || dot == base)
        return NULL;
    const char *ext = dot + 1;
    for (size_t i = 0; i < nlanguages; i++) {
        if (languages[i].exts == NULL)
            continue;
        for (size_t j = 0; languages[i].exts[j] != NULL; j++) {
            if (strcmp(ext, languages[i].exts[j]) == 0)
                return &languages[i];
        }
    }
    return NULL;
}

const struct language *language_by_index(size_t index)
{
    if (index >= nlanguages)
        return NULL;
    return &languages[index];
}

size_t language_count(void)
{
    return nlanguages;
}
