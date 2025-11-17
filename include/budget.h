#ifndef BUDGET_H
#define BUDGET_H

#include "utils.h"

/* Returns 0 on success. Outputs spent, limit and progress [0..1] for a category in a given month (YYYY-MM). */
int check_budget_status(const char *category, const char *yyyymm, double *out_spent, double *out_limit, double *out_progress);

#endif /* BUDGET_H */


