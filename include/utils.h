#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <malloc.h>
#ifndef strdup
#define strdup _strdup
#endif
#endif

#define TYPE_LEN 10
#define CATEGORY_LEN 32
#define DATE_LEN 16      /* YYYY-MM-DD (allow extra for safety) */
#define NOTE_LEN 128
#define NAME_LEN 64

typedef struct Transaction {
    int id;
    char type[TYPE_LEN];       /* "income" or "expense" */
    char category[CATEGORY_LEN];
    double amount;
    char date[DATE_LEN];       /* YYYY-MM-DD */
    char note[NOTE_LEN];
} Transaction;

typedef struct Budget {
    int id;
    char category[CATEGORY_LEN];
    double monthly_limit;
} Budget;

typedef struct Goal {
    int id;
    char name[NAME_LEN];
    double target_amount;
    double monthly_saving;
    char start_date[DATE_LEN]; /* YYYY-MM-DD */
} Goal;

typedef struct RecurringTransaction {
    int id;
    char type[TYPE_LEN];       /* "income" or "expense" */
    char category[CATEGORY_LEN];
    double amount;
    char frequency[16];        /* "weekly", "monthly", "yearly" */
    char start_date[DATE_LEN];
    char end_date[DATE_LEN];   /* NULL or date */
    char note[NOTE_LEN];
    int is_active;             /* 1 = active, 0 = inactive */
} RecurringTransaction;

typedef struct Forecast {
    char month[8];             /* YYYY-MM */
    double predicted_income;
    double predicted_expense;
    double predicted_balance;
} Forecast;

/* Date helpers */
void get_current_yyyymm(char out_yyyymm[8 + 1]);
void get_current_yyyymmdd(char out_date[DATE_LEN]);
int yyyymm_from_date(const char *yyyy_mm_dd, char out_yyyymm[8 + 1]);
int add_months_to_yyyymmdd(const char *yyyy_mm_dd, int months, char out_date[DATE_LEN]);

/* Misc helpers */
void color_from_category(const char *category, double *r, double *g, double *b);

/* Optional: CSV export */
int export_to_csv(const char *filename);

/* Formatting helpers */
void format_amount_currency(double amount, const char *currency, char *out, int out_size);
int is_valid_currency_string(const char *s);

/* Analytics and forecasting */
int calculate_spending_trend(const char *category, int months_back, double *out_avg, double *out_trend);
int generate_forecast(int months_ahead, Forecast **out_forecasts, int *out_count);
double calculate_category_average(const char *category, int months_back);

#endif /* UTILS_H */


