#ifndef GOAL_H
#define GOAL_H

#include "utils.h"

/* Calculate months needed and projected completion date given start_date and monthly_saving. */
int calculate_goal_projection(const Goal *g, int *out_months_needed, char projected_date_out[DATE_LEN]);

#endif /* GOAL_H */


