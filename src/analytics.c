#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils.h"
#include "database.h"
#include "budget.h"
#include "analytics.h"

/* Calculate spending trend for a category over N months */
int calculate_spending_trend(const char *category, int months_back, double *out_avg, double *out_trend)
{
    if (!category || months_back < 1) return -1;
    
    char **months = NULL;
    double *amounts = NULL;
    int count = 0;
    
    if (get_category_trends(category, months_back, &months, &amounts, &count) != 0) {
        return -1;
    }
    
    if (count < 2) {
        if (out_avg) *out_avg = count == 1 ? amounts[0] : 0.0;
        if (out_trend) *out_trend = 0.0;
        for (int i = 0; i < count; ++i) free(months[i]);
        free(months); free(amounts);
        return 0;
    }
    
    /* Calculate average */
    double sum = 0.0;
    for (int i = 0; i < count; ++i) sum += amounts[i];
    double avg = sum / count;
    
    /* Calculate trend (linear regression slope) */
    double trend = 0.0;
    double x_sum = 0.0, y_sum = 0.0, xy_sum = 0.0, x2_sum = 0.0;
    for (int i = 0; i < count; ++i) {
        double x = (double)i;
        double y = amounts[i];
        x_sum += x;
        y_sum += y;
        xy_sum += x * y;
        x2_sum += x * x;
    }
    double n = (double)count;
    double denom = n * x2_sum - x_sum * x_sum;
    if (fabs(denom) > 1e-10) {
        trend = (n * xy_sum - x_sum * y_sum) / denom;
    }
    
    if (out_avg) *out_avg = avg;
    if (out_trend) *out_trend = trend;
    
    for (int i = 0; i < count; ++i) free(months[i]);
    free(months); free(amounts);
    return 0;
}

/* Calculate average spending for a category over N months */
double calculate_category_average(const char *category, int months_back)
{
    double avg = 0.0;
    calculate_spending_trend(category, months_back, &avg, NULL);
    return avg;
}

/* Generate financial forecast for N months ahead */
int generate_forecast(int months_ahead, Forecast **out_forecasts, int *out_count)
{
    if (months_ahead < 1 || months_ahead > 12) return -1;
    
    char **months = NULL;
    double *income = NULL;
    double *expense = NULL;
    int hist_count = 0;
    
    /* Get historical data (last 6 months) */
    if (get_monthly_totals(6, &months, &income, &expense, &hist_count) != 0) {
        return -1;
    }
    
    if (hist_count == 0) {
        free(months); free(income); free(expense);
        return -1;
    }
    
    /* Calculate averages */
    double avg_income = 0.0, avg_expense = 0.0;
    for (int i = 0; i < hist_count; ++i) {
        avg_income += income[i];
        avg_expense += expense[i];
    }
    avg_income /= hist_count;
    avg_expense /= hist_count;
    
    /* Calculate trends */
    double income_trend = 0.0, expense_trend = 0.0;
    if (hist_count >= 2) {
        double x_sum = 0.0, y_sum = 0.0, xy_sum = 0.0, x2_sum = 0.0;
        for (int i = 0; i < hist_count; ++i) {
            double x = (double)i;
            double y = income[i];
            x_sum += x; y_sum += y; xy_sum += x * y; x2_sum += x * x;
        }
        double n = (double)hist_count;
        double denom = n * x2_sum - x_sum * x_sum;
        if (fabs(denom) > 1e-10) {
            income_trend = (n * xy_sum - x_sum * y_sum) / denom;
        }
        
        x_sum = y_sum = xy_sum = x2_sum = 0.0;
        for (int i = 0; i < hist_count; ++i) {
            double x = (double)i;
            double y = expense[i];
            x_sum += x; y_sum += y; xy_sum += x * y; x2_sum += x * x;
        }
        denom = n * x2_sum - x_sum * x_sum;
        if (fabs(denom) > 1e-10) {
            expense_trend = (n * xy_sum - x_sum * y_sum) / denom;
        }
    }
    
    /* Allocate forecast array */
    Forecast *forecasts = (Forecast*)calloc(months_ahead, sizeof(Forecast));
    if (!forecasts) {
        for (int i = 0; i < hist_count; ++i) free(months[i]);
        free(months); free(income); free(expense);
        return -1;
    }
    
    /* Generate forecasts */
    char current_yyyymm[9];
    get_current_yyyymm(current_yyyymm);
    int y, m;
    sscanf(current_yyyymm, "%d-%d", &y, &m);
    
    for (int i = 0; i < months_ahead; ++i) {
        m++;
        if (m > 12) { m = 1; y++; }
        snprintf(forecasts[i].month, sizeof(forecasts[i].month), "%04d-%02d", y, m);
        
        /* Project based on average + trend */
        double months_from_now = (double)(i + 1);
        forecasts[i].predicted_income = avg_income + income_trend * (months_from_now + (double)hist_count);
        forecasts[i].predicted_expense = avg_expense + expense_trend * (months_from_now + (double)hist_count);
        forecasts[i].predicted_balance = forecasts[i].predicted_income - forecasts[i].predicted_expense;
        
        /* Ensure non-negative predictions */
        if (forecasts[i].predicted_income < 0) forecasts[i].predicted_income = 0;
        if (forecasts[i].predicted_expense < 0) forecasts[i].predicted_expense = 0;
    }
    
    for (int i = 0; i < hist_count; ++i) free(months[i]);
    free(months); free(income); free(expense);
    
    *out_forecasts = forecasts;
    *out_count = months_ahead;
    return 0;
}

int check_budget_alerts(char ***out_categories, double **out_percentages, int *out_count)
{
    if (!out_categories || !out_percentages || !out_count) return -1;
    
    Budget *budgets = NULL;
    int budget_count = 0;
    if (fetch_budgets(&budgets, &budget_count) != 0) {
        return -1;
    }
    
    char current_yyyymm[9];
    get_current_yyyymm(current_yyyymm);
    
    char **alert_cats = NULL;
    double *alert_percents = NULL;
    int alert_count = 0;
    int cap = 0;
    
    for (int i = 0; i < budget_count; ++i) {
        double spent = get_spent_in_category_month(budgets[i].category, current_yyyymm);
        double progress = 0.0;
        if (budgets[i].monthly_limit > 0.0) {
            progress = spent / budgets[i].monthly_limit;
        }
        
        /* Alert if over 80% of budget */
        if (progress >= 0.8) {
            if (cap < alert_count + 1) {
                int ncap = (cap == 0) ? 8 : cap * 2;
                char **nc = (char**)realloc(alert_cats, ncap * sizeof(char*));
                if (!nc) {
                    for (int j = 0; j < alert_count; ++j) free(alert_cats[j]);
                    free(alert_cats); free(alert_percents); free(budgets);
                    return -1;
                }
                alert_cats = nc;
                double *np = (double*)realloc(alert_percents, ncap * sizeof(double));
                if (!np) {
                    for (int j = 0; j < alert_count; ++j) free(alert_cats[j]);
                    free(alert_cats); free(alert_percents); free(budgets);
                    return -1;
                }
                alert_percents = np; cap = ncap;
            }
            alert_cats[alert_count] = strdup(budgets[i].category);
            alert_percents[alert_count] = progress * 100.0;
            alert_count++;
        }
    }
    
    free(budgets);
    *out_categories = alert_cats;
    *out_percentages = alert_percents;
    *out_count = alert_count;
    return 0;
}

