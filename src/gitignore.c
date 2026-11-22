/* SPDX-License-Identifier: GPL-3.0-only */
#include "gitignore.h"

#include <ctype.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_BUF_SIZE 4096

static int add_rule(struct ignore_set *set, const struct ignore_rule *rule)
{
    if (set->count == set->cap) {
        size_t new_cap = set->cap == 0 ? 32 : set->cap * 2;
        struct ignore_rule *grown = realloc(set->rules, new_cap * sizeof(set->rules[0]));
        if (grown == NULL) {
            fprintf(stderr, "yalocc: out of memory\n");
            return -1;
        }
        set->rules = grown;
        set->cap = new_cap;
    }
    set->rules[set->count++] = *rule;
    return 0;
}

static int parse_rule(struct ignore_set *set, char *line, size_t base_len)
{
    size_t len = strlen(line);
    while (len > 0 && isspace((unsigned char)line[len - 1]))
        line[--len] = '\0';
    if (len == 0 || line[0] == '#')
        return 0;

    struct ignore_rule rule = {.base_len = base_len};
    char *p = line;
    if (p[0] == '!') {
        rule.negate = true;
        p++;
    } else if (p[0] == '\\' && (p[1] == '#' || p[1] == '!')) {
        p++;
    }

    len = strlen(p);
    if (len > 0 && p[len - 1] == '/') {
        rule.dir_only = true;
        p[--len] = '\0';
    }
    if (len == 0)
        return 0;

    if (p[0] == '/') {
        p++;
    } else if (strncmp(p, "**/", 3) == 0) {
        p += 3;
        rule.any_depth = true;
    } else if (strchr(p, '/') == NULL) {
        rule.any_depth = true;
    }
    if (*p == '\0')
        return 0;

    rule.pattern = strdup(p);
    if (rule.pattern == NULL) {
        fprintf(stderr, "yalocc: out of memory\n");
        return -1;
    }
    if (add_rule(set, &rule) != 0) {
        free(rule.pattern);
        return -1;
    }
    return 0;
}

int ignore_load(struct ignore_set *set, const char *dir)
{
    char path[PATH_BUF_SIZE];
    if (snprintf(path, sizeof(path), "%s/.gitignore", dir) >= (int)sizeof(path))
        return 0;
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return 0;

    size_t base_len = strlen(dir);
    char line[PATH_BUF_SIZE];
    int rc = 0;
    while (rc == 0 && fgets(line, sizeof(line), fp) != NULL)
        rc = parse_rule(set, line, base_len);
    fclose(fp);
    return rc;
}

void ignore_truncate(struct ignore_set *set, size_t count)
{
    while (set->count > count)
        free(set->rules[--set->count].pattern);
}

void ignore_free(struct ignore_set *set)
{
    ignore_truncate(set, 0);
    free(set->rules);
    set->rules = NULL;
    set->cap = 0;
}

static bool rule_matches(const struct ignore_rule *rule, const char *path, bool is_dir)
{
    if (rule->dir_only && !is_dir)
        return false;
    const char *rel = path + rule->base_len + 1;
    if (!rule->any_depth)
        return fnmatch(rule->pattern, rel, FNM_PATHNAME) == 0;
    for (const char *p = rel;; p++) {
        if (p == rel || p[-1] == '/') {
            if (fnmatch(rule->pattern, p, FNM_PATHNAME) == 0)
                return true;
        }
        if (*p == '\0')
            return false;
    }
}

bool ignore_matches(const struct ignore_set *set, const char *path, bool is_dir)
{
    bool ignored = false;
    for (size_t i = 0; i < set->count; i++) {
        if (rule_matches(&set->rules[i], path, is_dir))
            ignored = !set->rules[i].negate;
    }
    return ignored;
}
