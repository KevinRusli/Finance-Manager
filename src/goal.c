#include <math.h>
#include "goal.h"
#include "utils.h"

int calculate_goal_projection(const Goal *g, int *out_months_needed, char projected_date_out[DATE_LEN])
{
    if (!g || g->monthly_saving <= 0.0 || g->target_amount <= 0.0) return -1;
    int months = (int)ceil(g->target_amount / g->monthly_saving);
    if (out_months_needed) *out_months_needed = months;
    if (projected_date_out) {
        add_months_to_yyyymmdd(g->start_date, months, projected_date_out);
    }
    return 0;
}


