#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "database.h"

static sqlite3 *g_db = NULL;

static int exec_sql(const char *sql)
{
    char *errmsg = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    }
    return rc;
}

int init_database(const char *db_path)
{
    if (g_db) return 0;
    if (sqlite3_open(db_path, &g_db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }
    const char *schema_transactions = "CREATE TABLE IF NOT EXISTS transactions (id INTEGER PRIMARY KEY AUTOINCREMENT, type TEXT, category TEXT, amount REAL, date TEXT, note TEXT)";
    const char *schema_budgets = "CREATE TABLE IF NOT EXISTS budgets (id INTEGER PRIMARY KEY AUTOINCREMENT, category TEXT UNIQUE, monthly_limit REAL)";
    const char *schema_goals = "CREATE TABLE IF NOT EXISTS goals (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, target_amount REAL, monthly_saving REAL, start_date TEXT)";
    const char *schema_settings = "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)";
    const char *schema_recurring = "CREATE TABLE IF NOT EXISTS recurring_transactions (id INTEGER PRIMARY KEY AUTOINCREMENT, type TEXT, category TEXT, amount REAL, frequency TEXT, start_date TEXT, end_date TEXT, note TEXT, is_active INTEGER DEFAULT 1)";
    if (exec_sql(schema_transactions) != SQLITE_OK) return -1;
    if (exec_sql(schema_budgets) != SQLITE_OK) return -1;
    if (exec_sql(schema_goals) != SQLITE_OK) return -1;
    if (exec_sql(schema_settings) != SQLITE_OK) return -1;
    if (exec_sql(schema_recurring) != SQLITE_OK) return -1;
    return 0;
}

void close_database(void)
{
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

int add_transaction(const Transaction *t)
{
    const char *sql = "INSERT INTO transactions(type, category, amount, date, note) VALUES(?,?,?,?,?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, t->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, t->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, t->amount);
    sqlite3_bind_text(stmt, 4, t->date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, t->note, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int edit_transaction(const Transaction *t)
{
    const char *sql = "UPDATE transactions SET type=?, category=?, amount=?, date=?, note=? WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, t->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, t->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, t->amount);
    sqlite3_bind_text(stmt, 4, t->date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, t->note, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, t->id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int delete_transaction(int id)
{
    const char *sql = "DELETE FROM transactions WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

static int grow_transactions(Transaction **list, int *cap, int needed)
{
    if (*cap >= needed) return 0;
    int ncap = (*cap == 0) ? 32 : *cap * 2;
    while (ncap < needed) ncap *= 2;
    Transaction *nl = (Transaction*)realloc(*list, ncap * sizeof(Transaction));
    if (!nl) return -1;
    *list = nl;
    *cap = ncap;
    return 0;
}

int fetch_transactions_all(Transaction **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, type, category, amount, date, note FROM transactions ORDER BY date DESC, id DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    int cap = 0; Transaction *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (grow_transactions(&list, &cap, count + 1) != 0) { sqlite3_finalize(stmt); free(list); return -1; }
        Transaction *t = &list[count++];
        t->id = sqlite3_column_int(stmt, 0);
        snprintf(t->type, TYPE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(t->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 2));
        t->amount = sqlite3_column_double(stmt, 3);
        snprintf(t->date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 4));
        const unsigned char *note = sqlite3_column_text(stmt, 5);
        snprintf(t->note, NOTE_LEN, "%s", note ? (const char*)note : "");
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int fetch_transactions_by_month(const char *yyyymm, Transaction **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, type, category, amount, date, note FROM transactions WHERE date LIKE ? || '%' ORDER BY date DESC, id DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, yyyymm, -1, SQLITE_TRANSIENT);
    int cap = 0; Transaction *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (grow_transactions(&list, &cap, count + 1) != 0) { sqlite3_finalize(stmt); free(list); return -1; }
        Transaction *t = &list[count++];
        t->id = sqlite3_column_int(stmt, 0);
        snprintf(t->type, TYPE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(t->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 2));
        t->amount = sqlite3_column_double(stmt, 3);
        snprintf(t->date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 4));
        const unsigned char *note = sqlite3_column_text(stmt, 5);
        snprintf(t->note, NOTE_LEN, "%s", note ? (const char*)note : "");
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int add_or_update_budget(const Budget *b)
{
    const char *sql = "INSERT INTO budgets(category, monthly_limit) VALUES(?, ?) ON CONFLICT(category) DO UPDATE SET monthly_limit=excluded.monthly_limit";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, b->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, b->monthly_limit);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int get_budget_by_category(const char *category, Budget *out_budget)
{
    const char *sql = "SELECT id, category, monthly_limit FROM budgets WHERE category=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, category, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out_budget->id = sqlite3_column_int(stmt, 0);
        snprintf(out_budget->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        out_budget->monthly_limit = sqlite3_column_double(stmt, 2);
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    return 1; /* not found */
}

int fetch_budgets(Budget **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, category, monthly_limit FROM budgets ORDER BY category";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    int cap = 0; Budget *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = (cap == 0) ? 16 : cap * 2;
            while (ncap < count + 1) ncap *= 2;
            Budget *tmp = (Budget*)realloc(list, ncap * sizeof(Budget));
            if (!tmp) { sqlite3_finalize(stmt); free(list); return -1; }
            list = tmp; cap = ncap;
        }
        Budget *b = &list[count++];
        b->id = sqlite3_column_int(stmt, 0);
        snprintf(b->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        b->monthly_limit = sqlite3_column_double(stmt, 2);
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int add_goal(const Goal *g)
{
    const char *sql = "INSERT INTO goals(name, target_amount, monthly_saving, start_date) VALUES(?,?,?,?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, g->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, g->target_amount);
    sqlite3_bind_double(stmt, 3, g->monthly_saving);
    sqlite3_bind_text(stmt, 4, g->start_date, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int edit_goal(const Goal *g)
{
    const char *sql = "UPDATE goals SET name=?, target_amount=?, monthly_saving=?, start_date=? WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, g->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, g->target_amount);
    sqlite3_bind_double(stmt, 3, g->monthly_saving);
    sqlite3_bind_text(stmt, 4, g->start_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, g->id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int delete_goal(int id)
{
    const char *sql = "DELETE FROM goals WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int delete_budget(int id)
{
    const char *sql = "DELETE FROM budgets WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int update_budget(int id, const Budget *b)
{
    const char *sql = "UPDATE budgets SET category=?, monthly_limit=? WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, b->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, b->monthly_limit);
    sqlite3_bind_int(stmt, 3, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int fetch_goals(Goal **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, name, target_amount, monthly_saving, start_date FROM goals ORDER BY id DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    int cap = 0; Goal *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = (cap == 0) ? 16 : cap * 2;
            while (ncap < count + 1) ncap *= 2;
            Goal *tmp = (Goal*)realloc(list, ncap * sizeof(Goal));
            if (!tmp) { sqlite3_finalize(stmt); free(list); return -1; }
            list = tmp; cap = ncap;
        }
        Goal *g = &list[count++];
        g->id = sqlite3_column_int(stmt, 0);
        snprintf(g->name, NAME_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        g->target_amount = sqlite3_column_double(stmt, 2);
        g->monthly_saving = sqlite3_column_double(stmt, 3);
        snprintf(g->start_date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 4));
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

double get_total_by_type_for_month(const char *yyyymm, const char *type)
{
    const char *sql = "SELECT COALESCE(SUM(amount),0) FROM transactions WHERE type=? AND date LIKE ? || '%'";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0.0;
    sqlite3_bind_text(stmt, 1, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, yyyymm, -1, SQLITE_TRANSIENT);
    double total = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

double get_spent_in_category_month(const char *category, const char *yyyymm)
{
    const char *sql = "SELECT COALESCE(SUM(amount),0) FROM transactions WHERE type='expense' AND category=? AND date LIKE ? || '%'";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0.0;
    sqlite3_bind_text(stmt, 1, category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, yyyymm, -1, SQLITE_TRANSIENT);
    double total = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return total;
}

int fetch_expense_totals_by_category(const char *yyyymm, char ***out_categories, double **out_totals, int *out_count)
{
    *out_categories = NULL; *out_totals = NULL; *out_count = 0;
    const char *sql = "SELECT category, COALESCE(SUM(amount),0) FROM transactions WHERE type='expense' AND date LIKE ? || '%' GROUP BY category ORDER BY 2 DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, yyyymm, -1, SQLITE_TRANSIENT);
    int cap = 0; int count = 0; char **cats = NULL; double *totals = NULL;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = cap == 0 ? 8 : cap * 2;
            while (ncap < count + 1) ncap *= 2;
            char **nc = (char**)realloc(cats, ncap * sizeof(char*));
            if (!nc) { 
                sqlite3_finalize(stmt); 
                if (cats) free(cats); 
                if (totals) free(totals); 
                return -1; 
            }
            cats = nc;
            double *nt = (double*)realloc(totals, ncap * sizeof(double));
            if (!nt) { 
                sqlite3_finalize(stmt); 
                if (cats) free(cats); 
                if (totals) free(totals); 
                return -1; 
            }
            totals = nt; cap = ncap;
        }
        const unsigned char *c = sqlite3_column_text(stmt, 0);
        const char *cat = c ? (const char*)c : "Uncategorized";
        cats[count] = strdup(cat);
        totals[count] = sqlite3_column_double(stmt, 1);
        count++;
    }
    sqlite3_finalize(stmt);
    *out_categories = cats; *out_totals = totals; *out_count = count;
    return 0;
}

int get_setting(const char *key, char *out_value, int out_size)
{
    if (!key || !out_value) return -1;
    const char *sql = "SELECT value FROM settings WHERE key=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(stmt, 0);
        snprintf(out_value, out_size, "%s", v ? (const char*)v : "");
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    return 1; /* not found */
}

int set_setting(const char *key, const char *value)
{
    if (!key || !value) return -1;
    const char *sql = "INSERT INTO settings(key, value) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

/* Recurring Transactions */
int add_recurring_transaction(const RecurringTransaction *rt)
{
    const char *sql = "INSERT INTO recurring_transactions(type, category, amount, frequency, start_date, end_date, note, is_active) VALUES(?,?,?,?,?,?,?,?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, rt->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rt->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, rt->amount);
    sqlite3_bind_text(stmt, 4, rt->frequency, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, rt->start_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, rt->end_date[0] ? rt->end_date : NULL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, rt->note, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, rt->is_active);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int edit_recurring_transaction(const RecurringTransaction *rt)
{
    const char *sql = "UPDATE recurring_transactions SET type=?, category=?, amount=?, frequency=?, start_date=?, end_date=?, note=?, is_active=? WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, rt->type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rt->category, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, rt->amount);
    sqlite3_bind_text(stmt, 4, rt->frequency, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, rt->start_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, rt->end_date[0] ? rt->end_date : NULL, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, rt->note, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, rt->is_active);
    sqlite3_bind_int(stmt, 9, rt->id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int delete_recurring_transaction(int id)
{
    const char *sql = "DELETE FROM recurring_transactions WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : -1;
}

int fetch_recurring_transactions(RecurringTransaction **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, type, category, amount, frequency, start_date, end_date, note, is_active FROM recurring_transactions ORDER BY id DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    int cap = 0; RecurringTransaction *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = (cap == 0) ? 16 : cap * 2;
            RecurringTransaction *tmp = (RecurringTransaction*)realloc(list, ncap * sizeof(RecurringTransaction));
            if (!tmp) { sqlite3_finalize(stmt); free(list); return -1; }
            list = tmp; cap = ncap;
        }
        RecurringTransaction *rt = &list[count++];
        rt->id = sqlite3_column_int(stmt, 0);
        snprintf(rt->type, TYPE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(rt->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 2));
        rt->amount = sqlite3_column_double(stmt, 3);
        snprintf(rt->frequency, 16, "%s", (const char*)sqlite3_column_text(stmt, 4));
        snprintf(rt->start_date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 5));
        const unsigned char *ed = sqlite3_column_text(stmt, 6);
        snprintf(rt->end_date, DATE_LEN, "%s", ed ? (const char*)ed : "");
        const unsigned char *note = sqlite3_column_text(stmt, 7);
        snprintf(rt->note, NOTE_LEN, "%s", note ? (const char*)note : "");
        rt->is_active = sqlite3_column_int(stmt, 8);
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int fetch_active_recurring_transactions(RecurringTransaction **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, type, category, amount, frequency, start_date, end_date, note, is_active FROM recurring_transactions WHERE is_active=1 ORDER BY id DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    int cap = 0; RecurringTransaction *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = (cap == 0) ? 16 : cap * 2;
            RecurringTransaction *tmp = (RecurringTransaction*)realloc(list, ncap * sizeof(RecurringTransaction));
            if (!tmp) { sqlite3_finalize(stmt); free(list); return -1; }
            list = tmp; cap = ncap;
        }
        RecurringTransaction *rt = &list[count++];
        rt->id = sqlite3_column_int(stmt, 0);
        snprintf(rt->type, TYPE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(rt->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 2));
        rt->amount = sqlite3_column_double(stmt, 3);
        snprintf(rt->frequency, 16, "%s", (const char*)sqlite3_column_text(stmt, 4));
        snprintf(rt->start_date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 5));
        const unsigned char *ed = sqlite3_column_text(stmt, 6);
        snprintf(rt->end_date, DATE_LEN, "%s", ed ? (const char*)ed : "");
        const unsigned char *note = sqlite3_column_text(stmt, 7);
        snprintf(rt->note, NOTE_LEN, "%s", note ? (const char*)note : "");
        rt->is_active = sqlite3_column_int(stmt, 8);
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int process_recurring_transactions(void)
{
    RecurringTransaction *list = NULL;
    int count = 0;
    if (fetch_active_recurring_transactions(&list, &count) != 0) return -1;
    
    char current_date[DATE_LEN];
    get_current_yyyymmdd(current_date);
    
    int created = 0;
    for (int i = 0; i < count; ++i) {
        /* Simple check: if start_date <= current_date and (no end_date or end_date >= current_date) */
        if (strcmp(list[i].start_date, current_date) <= 0) {
            if (list[i].end_date[0] == '\0' || strcmp(list[i].end_date, current_date) >= 0) {
                /* Check if transaction already exists for this period */
                Transaction check = {0};
                snprintf(check.type, TYPE_LEN, "%s", list[i].type);
                snprintf(check.category, CATEGORY_LEN, "%s", list[i].category);
                check.amount = list[i].amount;
                snprintf(check.date, DATE_LEN, "%s", current_date);
                /* Ensure note fits: "Recurring: " is 11 chars, so we have NOTE_LEN-11 for the note */
                int max_note_len = NOTE_LEN - 12; /* -12 for "Recurring: " + null terminator */
                if (max_note_len > 0) {
                    snprintf(check.note, NOTE_LEN, "Recurring: %.*s", max_note_len, list[i].note);
                } else {
                    strncpy(check.note, "Recurring", NOTE_LEN - 1);
                    check.note[NOTE_LEN - 1] = '\0';
                }
                
                /* Simple check: see if similar transaction exists today */
                Transaction *existing = NULL;
                int exist_count = 0;
                fetch_transactions_by_date_range(current_date, current_date, &existing, &exist_count);
                int found = 0;
                for (int j = 0; j < exist_count; ++j) {
                    if (strcmp(existing[j].type, check.type) == 0 &&
                        strcmp(existing[j].category, check.category) == 0 &&
                        fabs(existing[j].amount - check.amount) < 0.01) {
                        found = 1;
                        break;
                    }
                }
                free(existing);
                
                if (!found) {
                    add_transaction(&check);
                    created++;
                }
            }
        }
    }
    
    free(list);
    return created;
}

/* Advanced Queries */
int fetch_transactions_by_category(const char *category, Transaction **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, type, category, amount, date, note FROM transactions WHERE category=? ORDER BY date DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, category, -1, SQLITE_TRANSIENT);
    int cap = 0; Transaction *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = (cap == 0) ? 32 : cap * 2;
            Transaction *tmp = (Transaction*)realloc(list, ncap * sizeof(Transaction));
            if (!tmp) { sqlite3_finalize(stmt); free(list); return -1; }
            list = tmp; cap = ncap;
        }
        Transaction *t = &list[count++];
        t->id = sqlite3_column_int(stmt, 0);
        snprintf(t->type, TYPE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(t->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 2));
        t->amount = sqlite3_column_double(stmt, 3);
        snprintf(t->date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 4));
        const unsigned char *note = sqlite3_column_text(stmt, 5);
        snprintf(t->note, NOTE_LEN, "%s", note ? (const char*)note : "");
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int fetch_transactions_by_date_range(const char *start_date, const char *end_date, Transaction **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, type, category, amount, date, note FROM transactions WHERE date >= ? AND date <= ? ORDER BY date DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, start_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, end_date, -1, SQLITE_TRANSIENT);
    int cap = 0; Transaction *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = (cap == 0) ? 32 : cap * 2;
            Transaction *tmp = (Transaction*)realloc(list, ncap * sizeof(Transaction));
            if (!tmp) { sqlite3_finalize(stmt); free(list); return -1; }
            list = tmp; cap = ncap;
        }
        Transaction *t = &list[count++];
        t->id = sqlite3_column_int(stmt, 0);
        snprintf(t->type, TYPE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(t->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 2));
        t->amount = sqlite3_column_double(stmt, 3);
        snprintf(t->date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 4));
        const unsigned char *note = sqlite3_column_text(stmt, 5);
        snprintf(t->note, NOTE_LEN, "%s", note ? (const char*)note : "");
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int fetch_transactions_search(const char *search_term, Transaction **out_list, int *out_count)
{
    *out_list = NULL; *out_count = 0;
    const char *sql = "SELECT id, type, category, amount, date, note FROM transactions WHERE category LIKE ? OR note LIKE ? ORDER BY date DESC";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%%%s%%", search_term);
    sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
    int cap = 0; Transaction *list = NULL; int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cap < count + 1) {
            int ncap = (cap == 0) ? 32 : cap * 2;
            Transaction *tmp = (Transaction*)realloc(list, ncap * sizeof(Transaction));
            if (!tmp) { sqlite3_finalize(stmt); free(list); return -1; }
            list = tmp; cap = ncap;
        }
        Transaction *t = &list[count++];
        t->id = sqlite3_column_int(stmt, 0);
        snprintf(t->type, TYPE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 1));
        snprintf(t->category, CATEGORY_LEN, "%s", (const char*)sqlite3_column_text(stmt, 2));
        t->amount = sqlite3_column_double(stmt, 3);
        snprintf(t->date, DATE_LEN, "%s", (const char*)sqlite3_column_text(stmt, 4));
        const unsigned char *note = sqlite3_column_text(stmt, 5);
        snprintf(t->note, NOTE_LEN, "%s", note ? (const char*)note : "");
    }
    sqlite3_finalize(stmt);
    *out_list = list; *out_count = count;
    return 0;
}

int get_monthly_totals(int months_back, char ***out_months, double **out_income, double **out_expense, int *out_count)
{
    *out_months = NULL; *out_income = NULL; *out_expense = NULL; *out_count = 0;
    
    char current_yyyymm[9];
    get_current_yyyymm(current_yyyymm);
    int y, m;
    sscanf(current_yyyymm, "%d-%d", &y, &m);
    
    char **months = (char**)calloc(months_back, sizeof(char*));
    double *income = (double*)calloc(months_back, sizeof(double));
    double *expense = (double*)calloc(months_back, sizeof(double));
    if (!months || !income || !expense) {
        free(months); free(income); free(expense);
        return -1;
    }
    
    for (int i = 0; i < months_back; ++i) {
        months[i] = (char*)malloc(9);
        snprintf(months[i], 9, "%04d-%02d", y, m);
        income[i] = get_total_by_type_for_month(months[i], "income");
        expense[i] = get_total_by_type_for_month(months[i], "expense");
        m--;
        if (m < 1) { m = 12; y--; }
    }
    
    *out_months = months;
    *out_income = income;
    *out_expense = expense;
    *out_count = months_back;
    return 0;
}

int get_category_trends(const char *category, int months_back, char ***out_months, double **out_amounts, int *out_count)
{
    *out_months = NULL; *out_amounts = NULL; *out_count = 0;
    
    char current_yyyymm[9];
    get_current_yyyymm(current_yyyymm);
    int y, m;
    sscanf(current_yyyymm, "%d-%d", &y, &m);
    
    char **months = (char**)calloc(months_back, sizeof(char*));
    double *amounts = (double*)calloc(months_back, sizeof(double));
    if (!months || !amounts) {
        free(months); free(amounts);
        return -1;
    }
    
    for (int i = 0; i < months_back; ++i) {
        months[i] = (char*)malloc(9);
        snprintf(months[i], 9, "%04d-%02d", y, m);
        amounts[i] = get_spent_in_category_month(category, months[i]);
        m--;
        if (m < 1) { m = 12; y--; }
    }
    
    *out_months = months;
    *out_amounts = amounts;
    *out_count = months_back;
    return 0;
}


