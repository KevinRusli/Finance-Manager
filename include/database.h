#ifndef DATABASE_H
#define DATABASE_H

#include "utils.h"

/* Database lifecycle */
int init_database(const char *db_path);
void close_database(void);

/* Transaction CRUD */
int add_transaction(const Transaction *t);
int edit_transaction(const Transaction *t);
int delete_transaction(int id);

/* Transaction queries */
int fetch_transactions_all(Transaction **out_list, int *out_count);
int fetch_transactions_by_month(const char *yyyymm, Transaction **out_list, int *out_count);

/* Budgets */
int add_or_update_budget(const Budget *b);
int get_budget_by_category(const char *category, Budget *out_budget);
int fetch_budgets(Budget **out_list, int *out_count);
int delete_budget(int id);
int update_budget(int id, const Budget *b);

/* Goals */
int add_goal(const Goal *g);
int edit_goal(const Goal *g);
int delete_goal(int id);
int fetch_goals(Goal **out_list, int *out_count);

/* Aggregations */
double get_total_by_type_for_month(const char *yyyymm, const char *type);
double get_spent_in_category_month(const char *category, const char *yyyymm);
int fetch_expense_totals_by_category(const char *yyyymm, char ***out_categories, double **out_totals, int *out_count);

/* Settings (key/value) */
int get_setting(const char *key, char *out_value, int out_size);
int set_setting(const char *key, const char *value);

/* Recurring Transactions */
int add_recurring_transaction(const RecurringTransaction *rt);
int edit_recurring_transaction(const RecurringTransaction *rt);
int delete_recurring_transaction(int id);
int fetch_recurring_transactions(RecurringTransaction **out_list, int *out_count);
int fetch_active_recurring_transactions(RecurringTransaction **out_list, int *out_count);
int process_recurring_transactions(void); /* Auto-create transactions based on schedule */

/* Advanced Queries */
int fetch_transactions_by_category(const char *category, Transaction **out_list, int *out_count);
int fetch_transactions_by_date_range(const char *start_date, const char *end_date, Transaction **out_list, int *out_count);
int fetch_transactions_search(const char *search_term, Transaction **out_list, int *out_count);
int get_monthly_totals(int months_back, char ***out_months, double **out_income, double **out_expense, int *out_count);
int get_category_trends(const char *category, int months_back, char ***out_months, double **out_amounts, int *out_count);

#endif /* DATABASE_H */


