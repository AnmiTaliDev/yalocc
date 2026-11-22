/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef YALOCC_REPORT_H
#define YALOCC_REPORT_H

#include <stdbool.h>
#include <stddef.h>

#include "results.h"

void print_report(const struct file_result *results, size_t nresults, const struct totals *totals,
                  bool by_file);

#endif
