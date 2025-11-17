#include "budget.h"
#include "database.h"

int check_budget_status(const char *category, const char *yyyymm, double *out_spent, double *out_limit, double *out_progress)
{
    if (!category || !yyyymm) return -1;
    Budget b;
    int rc = get_budget_by_category(category, &b);
    if (rc != 0) {
        if (out_spent) *out_spent = 0.0;
        if (out_limit) *out_limit = 0.0;
        if (out_progress) *out_progress = 0.0;
        return rc; /* not found or error */
    }
    double spent = get_spent_in_category_month(category, yyyymm);
    double progress = 0.0;
    if (b.monthly_limit > 0.0) progress = spent / b.monthly_limit;
    if (out_spent) *out_spent = spent;
    if (out_limit) *out_limit = b.monthly_limit;
    if (out_progress) *out_progress = progress;
    return 0;
}


