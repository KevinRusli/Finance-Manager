#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "utils.h"

/* Financial forecasting and trend analysis */
int calculate_spending_trend(const char *category, int months_back, double *out_avg, double *out_trend);
int generate_forecast(int months_ahead, Forecast **out_forecasts, int *out_count);
double calculate_category_average(const char *category, int months_back);

/* Budget alerts */
int check_budget_alerts(char ***out_categories, double **out_percentages, int *out_count);

#endif /* ANALYTICS_H */

