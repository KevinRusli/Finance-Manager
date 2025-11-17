#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"
#include "database.h"
#include "stats.h"
#include "budget.h"
#include "goal.h"
#include "chart.h"

typedef struct { AppWidgets *app; int page; } NavData;

static void show_dashboard_cb(GtkButton *btn, gpointer data) {
    (void)btn;
    AppWidgets *app = (AppWidgets*)data;
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "dashboard");
}

static void show_notebook_page_cb(GtkButton *btn, gpointer data) {
    (void)btn;
    NavData *nd = (NavData*)data;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(nd->app->notebook), nd->page);
    gtk_stack_set_visible_child_name(GTK_STACK(nd->app->stack), "main");
}

enum { COL_T_ID, COL_T_TYPE, COL_T_CATEGORY, COL_T_AMOUNT, COL_T_DATE, COL_T_NOTE, N_COL_T };
enum { COL_B_ID, COL_B_CATEGORY, COL_B_LIMIT, COL_B_SPENT, COL_B_PROGRESS, N_COL_B };
enum { COL_G_ID, COL_G_NAME, COL_G_TARGET, COL_G_MONTHLY, COL_G_START, COL_G_PROJECTION, N_COL_G };


static void refresh_transactions(AppWidgets *app);
static void refresh_budgets(AppWidgets *app);
static void refresh_goals(AppWidgets *app);
static void update_reports(AppWidgets *app);

static void on_edit_budget(GtkButton *btn, gpointer data);
static void on_delete_budget(GtkButton *btn, gpointer data);


static void amount_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    (void)col; (void)user_data;
    double val = 0.0; char curbuf[32] = "$"; char out[64];
    gtk_tree_model_get(model, iter, COL_T_AMOUNT, &val, -1);
    if (get_setting("currency", curbuf, sizeof(curbuf)) != 0) strncpy(curbuf, "$", sizeof(curbuf));
    format_amount_currency(val, curbuf, out, sizeof(out));
    g_object_set(renderer, "text", out, NULL);
}

static void goal_amount_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    (void)col; (void)user_data;
    double val = 0.0;
    int col_idx = GPOINTER_TO_INT(user_data);
    gtk_tree_model_get(model, iter, col_idx, &val, -1);
    /* Use same currency format as Transactions and Budget */
    char curbuf[32] = "$";
    char out[64];
    if (get_setting("currency", curbuf, sizeof(curbuf)) != 0) strncpy(curbuf, "$", sizeof(curbuf));
    format_amount_currency(val, curbuf, out, sizeof(out));
    g_object_set(renderer, "text", out, NULL);
}

static void budget_amount_cell_data_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    (void)col; (void)user_data;
    double val = 0.0;
    int col_idx = GPOINTER_TO_INT(user_data);
    gtk_tree_model_get(model, iter, col_idx, &val, -1);
    char curbuf[32] = "$";
    char out[64];
    if (get_setting("currency", curbuf, sizeof(curbuf)) != 0) strncpy(curbuf, "$", sizeof(curbuf));
    format_amount_currency(val, curbuf, out, sizeof(out));
    g_object_set(renderer, "text", out, NULL);
}

static void progress_cell_func(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    (void)col; (void)user_data;
    int val = 0; char buf[32];
    gtk_tree_model_get(model, iter, COL_B_PROGRESS, &val, -1);
    snprintf(buf, sizeof(buf), "%d%%", val);
    if (val > 100) {
        g_object_set(renderer, "text", buf, "foreground", "#e74c3c", NULL);
    } else {
        g_object_set(renderer, "text", buf, "foreground", NULL, NULL);
    }
}


static void on_export_csv(GtkButton *b, gpointer data) { 
    (void)b; (void)data; 
    export_to_csv("transactions.csv"); 
}


static gboolean _toast_destroy_cb(gpointer win)
{
    gtk_widget_destroy(GTK_WIDGET(win));
    return G_SOURCE_REMOVE;
}

static void show_toast(AppWidgets *app, const char *msg, int ms)
{
    GtkWidget *popup = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_transient_for(GTK_WINDOW(popup), GTK_WINDOW(app->window));
    gtk_window_set_resizable(GTK_WINDOW(popup), FALSE);
    GtkWidget *label = gtk_label_new(msg);
    gtk_container_add(GTK_CONTAINER(popup), label);
    gtk_widget_set_name(label, "toast-label");
    gtk_widget_show_all(popup);
    /* center over parent roughly */
    gint px, py, pw, ph;
    gtk_window_get_position(GTK_WINDOW(app->window), &px, &py);
    gtk_window_get_size(GTK_WINDOW(app->window), &pw, &ph);
    gtk_window_move(GTK_WINDOW(popup), px + pw/2 - 100, py + 40);
    g_timeout_add(ms > 0 ? ms : 1500, _toast_destroy_cb, popup);
}

static void on_add_transaction(GtkButton *btn, gpointer data){
    (void)btn; 
    AppWidgets *app = (AppWidgets*)data;
    GtkWidget *d = gtk_dialog_new_with_buttons("Add Transaction", GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
        "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    GtkWidget *type = gtk_combo_box_text_new(); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type), "income"); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(type), "expense"); gtk_combo_box_set_active(GTK_COMBO_BOX(type), 1);
    GtkWidget *cat = gtk_entry_new(); GtkWidget *amt = gtk_entry_new(); GtkWidget *date = gtk_entry_new(); GtkWidget *note = gtk_entry_new();
    /* default date to today */
    char today[DATE_LEN];
    get_current_yyyymmdd(today);
    gtk_entry_set_text(GTK_ENTRY(date), today);
    gtk_entry_set_placeholder_text(GTK_ENTRY(date), "YYYY-MM-DD");
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Type"), 0,0,1,1); gtk_grid_attach(GTK_GRID(grid), type, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Category"), 0,1,1,1); gtk_grid_attach(GTK_GRID(grid), cat, 1,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Amount"), 0,2,1,1); gtk_grid_attach(GTK_GRID(grid), amt, 1,2,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Date"), 0,3,1,1); gtk_grid_attach(GTK_GRID(grid), date, 1,3,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Note"), 0,4,1,1); gtk_grid_attach(GTK_GRID(grid), note, 1,4,1,1);
    gtk_container_add(GTK_CONTAINER(c), grid);
    gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        Transaction t = {0};
        snprintf(t.type, TYPE_LEN, "%s", gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(type)));
        snprintf(t.category, CATEGORY_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(cat)));
        t.amount = atof(gtk_entry_get_text(GTK_ENTRY(amt)));
        snprintf(t.date, DATE_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(date)));
        snprintf(t.note, NOTE_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(note)));
        add_transaction(&t);
        refresh_transactions(app);
        refresh_budgets(app); /* Update budget spent amounts */
    }
    gtk_widget_destroy(d);
}

static void on_delete_transaction(GtkButton *btn, gpointer data){
    (void)btn; 
    AppWidgets *app = (AppWidgets*)data; 
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->transactions_view)); 
    GtkTreeIter it; GtkTreeModel *m; 
    if (gtk_tree_selection_get_selected(sel, &m, &it)) { 
        int id; gtk_tree_model_get(m, &it, COL_T_ID, &id, -1); 
        delete_transaction(id); 
        refresh_transactions(app);
        refresh_budgets(app); /* Update budget spent amounts */
    } 
}

static void on_edit_transaction(GtkButton *btn, gpointer data){
    (void)btn; 
    AppWidgets *app = (AppWidgets*)data; 
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->transactions_view)); 
    GtkTreeIter it; GtkTreeModel *m; 
    if (!gtk_tree_selection_get_selected(sel, &m, &it)) return; 
    Transaction t = {0};
    int id; char *type=NULL, *cat=NULL, *date=NULL, *note=NULL; double amount=0.0;
    gtk_tree_model_get(m, &it, COL_T_ID, &id, COL_T_TYPE, &type, COL_T_CATEGORY, &cat, COL_T_AMOUNT, &amount, COL_T_DATE, &date, COL_T_NOTE, &note, -1);
    t.id = id; snprintf(t.type, TYPE_LEN, "%s", type?type:""); snprintf(t.category, CATEGORY_LEN, "%s", cat?cat:""); t.amount = amount; snprintf(t.date, DATE_LEN, "%s", date?date:""); snprintf(t.note, NOTE_LEN, "%s", note?note:"");
    GtkWidget *d = gtk_dialog_new_with_buttons("Edit Transaction", GTK_WINDOW(app->window), GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    GtkWidget *typew = gtk_combo_box_text_new(); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(typew), "income"); gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(typew), "expense"); int active = strcmp(t.type, "income")==0?0:1; gtk_combo_box_set_active(GTK_COMBO_BOX(typew), active);
    GtkWidget *catw = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(catw), t.category);
    GtkWidget *amtw = gtk_entry_new(); char buf[64]; snprintf(buf, sizeof(buf), "%.2f", t.amount); gtk_entry_set_text(GTK_ENTRY(amtw), buf);
    GtkWidget *datew = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(datew), t.date);
    GtkWidget *notew = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(notew), t.note);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Type"), 0,0,1,1); gtk_grid_attach(GTK_GRID(grid), typew, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Category"), 0,1,1,1); gtk_grid_attach(GTK_GRID(grid), catw, 1,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Amount"), 0,2,1,1); gtk_grid_attach(GTK_GRID(grid), amtw, 1,2,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Date"), 0,3,1,1); gtk_grid_attach(GTK_GRID(grid), datew, 1,3,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Note"), 0,4,1,1); gtk_grid_attach(GTK_GRID(grid), notew, 1,4,1,1);
    gtk_container_add(GTK_CONTAINER(c), grid); gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        snprintf(t.type, TYPE_LEN, "%s", gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(typew)));
        snprintf(t.category, CATEGORY_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(catw)));
        t.amount = atof(gtk_entry_get_text(GTK_ENTRY(amtw)));
        snprintf(t.date, DATE_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(datew)));
        snprintf(t.note, NOTE_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(notew)));
        edit_transaction(&t); 
        refresh_transactions(app);
        refresh_budgets(app); /* Update budget spent amounts */
    }
    gtk_widget_destroy(d);
    g_free(type); g_free(cat); g_free(date); g_free(note);
}

static void on_add_budget(GtkButton *btn, gpointer data){
    (void)btn;
    AppWidgets *app = (AppWidgets*)data; 
    GtkWidget *d = gtk_dialog_new_with_buttons("Add/Update Budget", GTK_WINDOW(app->window), GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    GtkWidget *cat = gtk_entry_new(); GtkWidget *limit = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Category"), 0,0,1,1); gtk_grid_attach(GTK_GRID(grid), cat, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Monthly Limit"), 0,1,1,1); gtk_grid_attach(GTK_GRID(grid), limit, 1,1,1,1);
    gtk_container_add(GTK_CONTAINER(c), grid); gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        Budget b = {0}; snprintf(b.category, CATEGORY_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(cat))); b.monthly_limit = atof(gtk_entry_get_text(GTK_ENTRY(limit))); add_or_update_budget(&b); refresh_budgets(app);
    }
    gtk_widget_destroy(d);
}

static void on_add_goal(GtkButton *btn, gpointer data){
    (void)btn;
    AppWidgets *app = (AppWidgets*)data; 
    GtkWidget *d = gtk_dialog_new_with_buttons("Add Goal", GTK_WINDOW(app->window), GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    GtkWidget *name = gtk_entry_new(); GtkWidget *target = gtk_entry_new(); GtkWidget *monthly = gtk_entry_new(); GtkWidget *start = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(start), "YYYY-MM-DD");
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name"), 0,0,1,1); gtk_grid_attach(GTK_GRID(grid), name, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Target Amount"), 0,1,1,1); gtk_grid_attach(GTK_GRID(grid), target, 1,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Monthly Saving"), 0,2,1,1); gtk_grid_attach(GTK_GRID(grid), monthly, 1,2,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Start Date"), 0,3,1,1); gtk_grid_attach(GTK_GRID(grid), start, 1,3,1,1);
    gtk_container_add(GTK_CONTAINER(c), grid); gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        Goal g = {0}; snprintf(g.name, NAME_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(name))); g.target_amount = atof(gtk_entry_get_text(GTK_ENTRY(target))); g.monthly_saving = atof(gtk_entry_get_text(GTK_ENTRY(monthly))); snprintf(g.start_date, DATE_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(start))); add_goal(&g); refresh_goals(app);
    }
    gtk_widget_destroy(d);
}

static void on_edit_goal(GtkButton *btn, gpointer data){
    (void)btn;
    AppWidgets *app = (AppWidgets*)data; 
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->goals_view)); 
    GtkTreeIter it; GtkTreeModel *m; 
    if (!gtk_tree_selection_get_selected(sel, &m, &it)) return; 
    Goal g = {0}; 
    int id; char *name=NULL, *start=NULL; double target=0.0, monthly=0.0;
    gtk_tree_model_get(m, &it, COL_G_ID, &id, COL_G_NAME, &name, COL_G_TARGET, &target, COL_G_MONTHLY, &monthly, COL_G_START, &start, -1);
    g.id = id; snprintf(g.name, NAME_LEN, "%s", name?name:""); g.target_amount = target; g.monthly_saving = monthly; snprintf(g.start_date, DATE_LEN, "%s", start?start:"");
    GtkWidget *d = gtk_dialog_new_with_buttons("Edit Goal", GTK_WINDOW(app->window), GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    GtkWidget *namew = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(namew), g.name);
    GtkWidget *targetw = gtk_entry_new(); char bt[64]; snprintf(bt, sizeof(bt), "%.2f", g.target_amount); gtk_entry_set_text(GTK_ENTRY(targetw), bt);
    GtkWidget *monthlyw = gtk_entry_new(); char bm[64]; snprintf(bm, sizeof(bm), "%.2f", g.monthly_saving); gtk_entry_set_text(GTK_ENTRY(monthlyw), bm);
    GtkWidget *startw = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(startw), g.start_date);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name"), 0,0,1,1); gtk_grid_attach(GTK_GRID(grid), namew, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Target Amount"), 0,1,1,1); gtk_grid_attach(GTK_GRID(grid), targetw, 1,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Monthly Saving"), 0,2,1,1); gtk_grid_attach(GTK_GRID(grid), monthlyw, 1,2,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Start Date"), 0,3,1,1); gtk_grid_attach(GTK_GRID(grid), startw, 1,3,1,1);
    gtk_container_add(GTK_CONTAINER(c), grid); gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        snprintf(g.name, NAME_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(namew))); g.target_amount = atof(gtk_entry_get_text(GTK_ENTRY(targetw))); g.monthly_saving = atof(gtk_entry_get_text(GTK_ENTRY(monthlyw))); snprintf(g.start_date, DATE_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(startw))); edit_goal(&g); refresh_goals(app);
    }
    gtk_widget_destroy(d);
    g_free(name); g_free(start);
}

static void on_delete_goal(GtkButton *btn, gpointer data){ 
    (void)btn;
    AppWidgets *app = (AppWidgets*)data; 
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->goals_view)); 
    GtkTreeIter it; GtkTreeModel *m; 
    if (gtk_tree_selection_get_selected(sel, &m, &it)) { 
        int id; gtk_tree_model_get(m, &it, COL_G_ID, &id, -1); 
        delete_goal(id); 
        refresh_goals(app);
    } 
}

static void on_reports_month_changed(GtkEditable *e, gpointer data){ 
    (void)e; 
    update_reports((AppWidgets*)data); 
}

static void set_current_month(GtkButton *btn, gpointer data) {
    (void)btn;
    AppWidgets *app = (AppWidgets*)data;
    char yyyymm[9];
    get_current_yyyymm(yyyymm);
    gtk_entry_set_text(GTK_ENTRY(app->chart_month_entry), yyyymm);
}

static void on_chart_month_changed(GtkEditable *e, gpointer data){ 
    (void)e; 
    AppWidgets *app=(AppWidgets*)data; 
    gtk_widget_queue_draw(app->chart_area);
}

static void refresh_dashboard(AppWidgets *app) {
    /* Trigger updates for both chart and reports */
    gtk_widget_queue_draw(app->chart_area);
    update_reports(app);
}

static void refresh_transactions(AppWidgets *app)
{
    gtk_list_store_clear(app->transactions_store);
    Transaction *list = NULL; int count = 0;
    if (fetch_transactions_all(&list, &count) == 0) {
        for (int i = 0; i < count; ++i) {
            GtkTreeIter it;
            gtk_list_store_append(app->transactions_store, &it);
            gtk_list_store_set(app->transactions_store, &it,
                COL_T_ID, list[i].id,
                COL_T_TYPE, list[i].type,
                COL_T_CATEGORY, list[i].category,
                COL_T_AMOUNT, list[i].amount,
                COL_T_DATE, list[i].date,
                COL_T_NOTE, list[i].note,
                -1);
        }
        free(list);
    }
    /* Refresh dashboard after transaction changes */
    refresh_dashboard(app);
}

static void refresh_budgets(AppWidgets *app)
{
    gtk_list_store_clear(app->budgets_store);
    Budget *list = NULL; int count = 0;
    char yyyymm[9]; get_current_yyyymm(yyyymm);
    if (fetch_budgets(&list, &count) == 0) {
        for (int i = 0; i < count; ++i) {
            double spent = get_spent_in_category_month(list[i].category, yyyymm);
            double progress = list[i].monthly_limit > 0.0 ? spent / list[i].monthly_limit : 0.0;
            GtkTreeIter it; gtk_list_store_append(app->budgets_store, &it);
            gtk_list_store_set(app->budgets_store, &it,
                COL_B_ID, list[i].id,
                COL_B_CATEGORY, list[i].category,
                COL_B_LIMIT, list[i].monthly_limit,
                COL_B_SPENT, spent,
                COL_B_PROGRESS, (int)(progress * 100.0),
                -1);
        }
        free(list);
    }
}

static void refresh_goals(AppWidgets *app)
{
    gtk_list_store_clear(app->goals_store);
    Goal *list = NULL; int count = 0;
    if (fetch_goals(&list, &count) == 0) {
        for (int i = 0; i < count; ++i) {
            int months = 0; char proj[DATE_LEN] = "";
            calculate_goal_projection(&list[i], &months, proj);
            GtkTreeIter it; gtk_list_store_append(app->goals_store, &it);
            gtk_list_store_set(app->goals_store, &it,
                COL_G_ID, list[i].id,
                COL_G_NAME, list[i].name,
                COL_G_TARGET, list[i].target_amount,
                COL_G_MONTHLY, list[i].monthly_saving,
                COL_G_START, list[i].start_date,
                COL_G_PROJECTION, proj,
                -1);
        }
        free(list);
    }
}

static GtkWidget* build_transactions_tab(AppWidgets *app)
{
    app->transactions_store = gtk_list_store_new(N_COL_T,
        G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->transactions_store));
    app->transactions_view = view;
    GtkCellRenderer *r;
    GtkTreeViewColumn *c;
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("ID", r, "text", COL_T_ID, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Type", r, "text", COL_T_TYPE, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Category", r, "text", COL_T_CATEGORY, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Amount", r, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    /* use top-level cell data func to show currency prefix and formatting */
    gtk_tree_view_column_set_cell_data_func(c, r, (GtkTreeCellDataFunc)amount_cell_data_func, app, NULL);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Date", r, "text", COL_T_DATE, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Note", r, "text", COL_T_NOTE, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);

    GtkWidget *add_btn = gtk_button_new_with_label("Add");
    GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
    GtkWidget *export_btn = gtk_button_new_with_label("Export CSV");

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(btn_box), add_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), edit_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), del_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_box), export_btn, FALSE, FALSE, 0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sw), view);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);

    /* Handlers */
    g_signal_connect(export_btn, "clicked", G_CALLBACK(on_export_csv), NULL);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_transaction), app);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_transaction), app);
    g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_transaction), app);

    return vbox;
}

static GtkWidget* build_budgets_tab(AppWidgets *app)
{
    app->budgets_store = gtk_list_store_new(N_COL_B, G_TYPE_INT, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_INT);
    GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->budgets_store));
    app->budgets_view = view;
    GtkCellRenderer *r; GtkTreeViewColumn *c;
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Category", r, "text", COL_B_CATEGORY, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); 
    c = gtk_tree_view_column_new_with_attributes("Limit", r, NULL);
    gtk_tree_view_column_set_cell_data_func(c, r, budget_amount_cell_data_func, GINT_TO_POINTER(COL_B_LIMIT), NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); 
    c = gtk_tree_view_column_new_with_attributes("Spent", r, NULL);
    gtk_tree_view_column_set_cell_data_func(c, r, budget_amount_cell_data_func, GINT_TO_POINTER(COL_B_SPENT), NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Progress", r, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    /* colorize if progress > 100% */
    gtk_tree_view_column_set_cell_data_func(c, r, (GtkTreeCellDataFunc)progress_cell_func, app, NULL);
    GtkWidget *add_btn = gtk_button_new_with_label("Add / Update Budget");
    GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(btn_box), add_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), edit_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), del_btn, FALSE, FALSE, 0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sw), view);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);

    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_budget), app);
    g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_budget), app);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_budget), app);

    return vbox;
}

static GtkWidget* build_goals_tab(AppWidgets *app)
{
    app->goals_store = gtk_list_store_new(N_COL_G, G_TYPE_INT, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->goals_store));
    app->goals_view = view;
    GtkCellRenderer *r; GtkTreeViewColumn *c;
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("ID", r, "text", COL_G_ID, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Name", r, "text", COL_G_NAME, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); 
    c = gtk_tree_view_column_new_with_attributes("Target", r, NULL);
    gtk_tree_view_column_set_cell_data_func(c, r, goal_amount_cell_data_func, GINT_TO_POINTER(COL_G_TARGET), NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); 
    c = gtk_tree_view_column_new_with_attributes("Monthly", r, NULL);
    gtk_tree_view_column_set_cell_data_func(c, r, goal_amount_cell_data_func, GINT_TO_POINTER(COL_G_MONTHLY), NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Start", r, "text", COL_G_START, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);
    r = gtk_cell_renderer_text_new(); c = gtk_tree_view_column_new_with_attributes("Projected", r, "text", COL_G_PROJECTION, NULL); gtk_tree_view_append_column(GTK_TREE_VIEW(view), c);

    GtkWidget *add_btn = gtk_button_new_with_label("Add");
    GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(btn_box), add_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), edit_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), del_btn, FALSE, FALSE, 0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(sw), view);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);

    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_goal), app);
    g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_goal), app);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_goal), app);

    return vbox;
}

static void update_reports(AppWidgets *app)
{
    char month_buf[8 + 1] = "";
    const char *month = NULL;
    if (app && app->chart_month_entry && GTK_IS_ENTRY(app->chart_month_entry)) {
        month = gtk_entry_get_text(GTK_ENTRY(app->chart_month_entry));
    } else {
        get_current_yyyymm(month_buf);
        month = month_buf;
    }
    double income = get_total_income(month);
    double expense = get_total_expense(month);
    double balance = income - expense;
    
    char currency[32] = "$";
    if (get_setting("currency", currency, sizeof(currency)) != 0) strncpy(currency, "$", sizeof(currency));

    char *markup = g_markup_printf_escaped("<span font='16' color='#2ecc71'>%s%.2f</span>", currency, income);
    gtk_label_set_markup(GTK_LABEL(app->income_label), markup);
    g_free(markup);

    markup = g_markup_printf_escaped("<span font='16' color='#e74c3c'>%s%.2f</span>", currency, expense);
    gtk_label_set_markup(GTK_LABEL(app->expense_label), markup);
    g_free(markup);

    const char *color = balance >= 0 ? "#2ecc71" : "#e74c3c";
    markup = g_markup_printf_escaped("<span font='16' color='%s'>%s%.2f</span>", color, currency, balance);
    gtk_label_set_markup(GTK_LABEL(app->balance_label), markup);
    g_free(markup);
}

static gboolean on_chart_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    AppWidgets *app = (AppWidgets*)data;
    char month_buf[8 + 1] = "";
    const char *month = NULL;
    if (app && app->chart_month_entry && GTK_IS_ENTRY(app->chart_month_entry)) {
        month = gtk_entry_get_text(GTK_ENTRY(app->chart_month_entry));
    } else {
        get_current_yyyymm(month_buf);
        month = month_buf;
    }
    GtkAllocation a; gtk_widget_get_allocation(widget, &a);
    draw_expense_chart(cr, a.width, a.height, month);
    return FALSE;
}

static GtkWidget* build_reports_tab(AppWidgets *app)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    
    app->income_label = gtk_label_new("");
    app->expense_label = gtk_label_new("");
    app->balance_label = gtk_label_new("");
    
    /* Style the labels with modern look */
    GtkWidget *value_labels[] = {app->income_label, app->expense_label, app->balance_label};
    const char *colors[] = {"#2ecc71", "#e74c3c", "#3498db"};  /* Green, Red, Blue */
    const char *titles[] = {"Income", "Expense", "Balance"};

    for (int i = 0; i < 3; i++) {
        GtkWidget *frame = gtk_frame_new(NULL);
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

        /* Create and style title label */
        GtkWidget *title = gtk_label_new(NULL);
        char *title_markup = g_markup_printf_escaped("<span font_weight='bold'>%s</span>", titles[i]);
        gtk_label_set_markup(GTK_LABEL(title), title_markup);
        g_free(title_markup);

        /* Style the value label */
        char *value_markup = g_markup_printf_escaped("<span font='16' color='%s'>0.00</span>", colors[i]);
        gtk_label_set_markup(GTK_LABEL(value_labels[i]), value_markup);
        g_free(value_markup);

        gtk_container_add(GTK_CONTAINER(frame), box);
        gtk_box_pack_start(GTK_BOX(box), title, TRUE, TRUE, 6);
        gtk_box_pack_start(GTK_BOX(box), value_labels[i], TRUE, TRUE, 6);

        GtkStyleContext *style = gtk_widget_get_style_context(frame);
        gtk_style_context_add_class(style, "dashboard-panel");

        gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 6);
    }
    
    update_reports(app);
    return vbox;
}

static GtkWidget* build_charts_tab(AppWidgets *app)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    app->chart_area = gtk_drawing_area_new(); gtk_widget_set_size_request(app->chart_area, 400, 300);
    gtk_box_pack_start(GTK_BOX(vbox), app->chart_area, TRUE, TRUE, 0);
    /* Connect draw after show_all to avoid early draw callbacks during startup
       which can trigger code paths before all widgets are initialized. */
    /* g_signal_connect(app->chart_area, "draw", G_CALLBACK(on_chart_draw), app); */
    return vbox;
}

static void on_save_currency(GtkButton *btn, gpointer data)
{
    (void)btn;
    AppWidgets *app = (AppWidgets*)data;
    const char *raw = gtk_entry_get_text(GTK_ENTRY(app->currency_entry));
    if (!raw) return;
    /* Trim whitespace */
    char valbuf[64];
    size_t i = 0, j = strlen(raw);
    while (i < j && raw[i] == ' ') i++;
    while (j > i && raw[j-1] == ' ') j--;
    size_t len = j > i ? (j - i) : 0;
    if (len == 0) return;
    if (len >= sizeof(valbuf)) len = sizeof(valbuf) - 1;
    memcpy(valbuf, raw + i, len);
    valbuf[len] = '\0';

    if (!is_valid_currency_string(valbuf)) {
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Invalid currency format. Use 1-4 letters/symbols.");
        gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
        return;
    }
    int rc = set_setting("currency", valbuf);
    if (rc == 0) {
        /* update current label and refresh dashboard immediately */
        gtk_label_set_text(GTK_LABEL(app->currency_label), valbuf);
        refresh_dashboard(app);
        show_toast(app, "Currency saved", 1400);
    } else {
        /* show error dialog */
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Failed to save currency setting (code %d)", rc);
        gtk_dialog_run(GTK_DIALOG(d));
        gtk_widget_destroy(d);
    }
}

static GtkWidget* build_settings_tab(AppWidgets *app)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("Currency (symbol or code, e.g. $ or USD):"), FALSE, FALSE, 0);
    app->currency_entry = gtk_entry_new();
    char curval[64] = "";
    if (get_setting("currency", curval, sizeof(curval)) != 0 || strlen(curval) == 0) {
        snprintf(curval, sizeof(curval), "$");
    }
    gtk_entry_set_text(GTK_ENTRY(app->currency_entry), curval);
    gtk_box_pack_start(GTK_BOX(vbox), app->currency_entry, FALSE, FALSE, 0);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    gtk_box_pack_start(GTK_BOX(vbox), save_btn, FALSE, FALSE, 0);

    GtkWidget *current_label_title = gtk_label_new("Current currency:");
    app->currency_label = gtk_label_new(curval);
    gtk_box_pack_start(GTK_BOX(vbox), current_label_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->currency_label, FALSE, FALSE, 0);

    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_currency), app);
    return vbox;
}

GtkWidget* build_main_window(AppWidgets *app)
{
    g_print("[debug] build_main_window start\n");
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "Personal Finance Manager");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 900, 600);
    /* Create notebook as before */
    app->notebook = gtk_notebook_new();
    g_print("[debug] notebook created\n");

    GtkWidget *t_tab = build_transactions_tab(app);
    GtkWidget *b_tab = build_budgets_tab(app);
    GtkWidget *g_tab = build_goals_tab(app);
    GtkWidget *r_tab = build_reports_tab(app);
    GtkWidget *c_tab = build_charts_tab(app);
    GtkWidget *s_tab = build_settings_tab(app);

    g_print("[debug] tabs built (transactions,budgets,goals,reports,charts,settings)\n");

    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), t_tab, gtk_label_new("Transactions"));
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), b_tab, gtk_label_new("Budgets"));
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), g_tab, gtk_label_new("Goals"));
    gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), s_tab, gtk_label_new("Settings"));

    g_print("[debug] notebook pages appended\n");

    /* Top-level stack: page 1 = dashboard, page 2 = main notebook */
    app->stack = gtk_stack_new();

    g_print("[debug] stack created\n");

    /* Dashboard: show charts and reports side-by-side and navigation buttons */
    GtkWidget *dashboard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *btn_dashboard = gtk_button_new_with_label("Dashboard");
    GtkWidget *btn_transactions = gtk_button_new_with_label("Transactions");
    GtkWidget *btn_budgets = gtk_button_new_with_label("Budgets");
    GtkWidget *btn_goals = gtk_button_new_with_label("Goals");
    GtkWidget *btn_settings = gtk_button_new_with_label("Settings");
    gtk_box_pack_start(GTK_BOX(nav_box), btn_dashboard, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_box), btn_transactions, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_box), btn_budgets, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_box), btn_goals, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nav_box), btn_settings, FALSE, FALSE, 0);

    /* Month selection bar (shared between chart and report) */
    GtkWidget *month_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *month_label = gtk_label_new(NULL);
    char *markup = g_markup_printf_escaped("<span font_weight='bold'>Month:</span>");
    gtk_label_set_markup(GTK_LABEL(month_label), markup);
    g_free(markup);
    
    app->chart_month_entry = gtk_entry_new();
    char yyyymm[9];
    get_current_yyyymm(yyyymm);
    gtk_entry_set_text(GTK_ENTRY(app->chart_month_entry), yyyymm);
    gtk_entry_set_width_chars(GTK_ENTRY(app->chart_month_entry), 8);
    gtk_entry_set_max_length(GTK_ENTRY(app->chart_month_entry), 7);
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->chart_month_entry), "YYYY-MM");
    
    GtkWidget *current_month_btn = gtk_button_new_with_label("Current Month");
    
    gtk_box_pack_start(GTK_BOX(month_bar), month_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(month_bar), app->chart_month_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(month_bar), current_month_btn, FALSE, FALSE, 0);
    
    /* Style the month bar */
    GtkStyleContext *month_style = gtk_widget_get_style_context(month_bar);
    gtk_style_context_add_class(month_style, "month-bar");
    gtk_widget_set_margin_start(month_bar, 12);
    gtk_widget_set_margin_end(month_bar, 12);
    gtk_widget_set_margin_top(month_bar, 12);
    gtk_widget_set_margin_bottom(month_bar, 12);
    
    /* Content area with modern styling */
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24); /* increased spacing */
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    
    /* Create frames for chart and report sections */
    GtkWidget *chart_frame = gtk_frame_new(NULL);
    GtkWidget *report_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(chart_frame), GTK_SHADOW_NONE);
    gtk_frame_set_shadow_type(GTK_FRAME(report_frame), GTK_SHADOW_NONE);
    
    /* Style frames */
    GtkStyleContext *chart_style = gtk_widget_get_style_context(chart_frame);
    GtkStyleContext *report_style = gtk_widget_get_style_context(report_frame);
    gtk_style_context_add_class(chart_style, "dashboard-panel");
    gtk_style_context_add_class(report_style, "dashboard-panel");
    
    gtk_container_add(GTK_CONTAINER(chart_frame), c_tab);
    gtk_container_add(GTK_CONTAINER(report_frame), r_tab);
    
    gtk_box_pack_start(GTK_BOX(content), chart_frame, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content), report_frame, TRUE, TRUE, 0);

    /* Assemble dashboard: navigation, month selector, and content area */
    gtk_box_pack_start(GTK_BOX(dashboard), nav_box, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(dashboard), month_bar, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(dashboard), content, TRUE, TRUE, 6);
    

    g_signal_connect(current_month_btn, "clicked", G_CALLBACK(set_current_month), app);

    /* Wrap the notebook in a container with modern styling */
    GtkWidget *notebook_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *back_btn = gtk_button_new_with_label("← Back to Dashboard");
    
    /* Style the header bar */
    gtk_widget_set_margin_start(header_bar, 12);
    gtk_widget_set_margin_end(header_bar, 12);
    gtk_widget_set_margin_top(header_bar, 12);
    gtk_widget_set_margin_bottom(header_bar, 12);
    
    /* Add the back button to header */
    gtk_box_pack_start(GTK_BOX(header_bar), back_btn, FALSE, FALSE, 0);
    
    /* Style the notebook */
    gtk_widget_set_margin_start(app->notebook, 12);
    gtk_widget_set_margin_end(app->notebook, 12);
    gtk_widget_set_margin_bottom(app->notebook, 12);
    
    /* Pack everything */
    gtk_box_pack_start(GTK_BOX(notebook_container), header_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(notebook_container), app->notebook, TRUE, TRUE, 0);

    gtk_stack_add_titled(GTK_STACK(app->stack), dashboard, "dashboard", "Dashboard");
    gtk_stack_add_titled(GTK_STACK(app->stack), notebook_container, "main", "Main");
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "dashboard");

    g_print("[debug] stack pages added and dashboard set visible\n");

    /* Button callbacks to switch views */
    /* Connect navigation using helper callbacks */
    g_signal_connect(btn_dashboard, "clicked", G_CALLBACK(show_dashboard_cb), app);
    NavData *nd_t = g_new(NavData, 1); nd_t->app = app; nd_t->page = 0;
    g_signal_connect(btn_transactions, "clicked", G_CALLBACK(show_notebook_page_cb), nd_t);
    NavData *nd_b = g_new(NavData, 1); nd_b->app = app; nd_b->page = 1;
    g_signal_connect(btn_budgets, "clicked", G_CALLBACK(show_notebook_page_cb), nd_b);
    NavData *nd_g = g_new(NavData, 1); nd_g->app = app; nd_g->page = 2;
    g_signal_connect(btn_goals, "clicked", G_CALLBACK(show_notebook_page_cb), nd_g);
    NavData *nd_s = g_new(NavData, 1); nd_s->app = app; nd_s->page = 3;
    g_signal_connect(btn_settings, "clicked", G_CALLBACK(show_notebook_page_cb), nd_s);
    /* Back button returns to dashboard */
    g_signal_connect(back_btn, "clicked", G_CALLBACK(show_dashboard_cb), app);

    g_print("[debug] navigation signals connected\n");

    /* NOTE: gtk_stack_set_visible_child_name expects the stack pointer as first arg when used as callback
       so we will use a small helper below to set the correct child for some signals. To avoid creating
       additional functions, we'll connect with swapped and pass string names where supported. */

    /* Container */
    GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(container), app->stack, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(app->window), container);

    g_print("[debug] container packed into window\n");

    g_print("[debug] build_main_window end\n");
    return app->window;
}

void update_gui_tables(AppWidgets *app)
{
    refresh_transactions(app);
    refresh_budgets(app);
    refresh_goals(app);
}

/* Connect deferred handlers after the main window is shown to avoid early callbacks */
void connect_deferred_handlers(AppWidgets *app)
{
    if (!app) return;
    /* Populate tables after the UI is realized to avoid initialization-time callbacks */
    g_print("[debug] deferred: refreshing transactions\n");
    refresh_transactions(app);
    g_print("[debug] deferred: refreshing budgets\n");
    refresh_budgets(app);
    g_print("[debug] deferred: refreshing goals\n");
    refresh_goals(app);
    if (app->chart_area && GTK_IS_WIDGET(app->chart_area)) {
        g_signal_connect(app->chart_area, "draw", G_CALLBACK(on_chart_draw), app);
        /* Force an initial redraw */
        gtk_widget_queue_draw(app->chart_area);
    }
    if (app->chart_month_entry && GTK_IS_ENTRY(app->chart_month_entry)) {
        g_signal_connect(app->chart_month_entry, "changed", G_CALLBACK(on_chart_month_changed), app);
        g_signal_connect(app->chart_month_entry, "changed", G_CALLBACK(on_reports_month_changed), app);
    }
}

static void on_edit_budget(GtkButton *btn, gpointer data){
    (void)btn;
    AppWidgets *app = (AppWidgets*)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->budgets_view));
    GtkTreeIter it; GtkTreeModel *m;
    if (!gtk_tree_selection_get_selected(sel, &m, &it)) return;
    int id = 0; char *category = NULL; double limit = 0.0;
    gtk_tree_model_get(m, &it, COL_B_ID, &id, COL_B_CATEGORY, &category, COL_B_LIMIT, &limit, -1);
    if (!category) category = g_strdup("");

    GtkWidget *d = gtk_dialog_new_with_buttons("Edit Budget", GTK_WINDOW(app->window), GTK_DIALOG_MODAL, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *c = gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6); gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    GtkWidget *catw = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(catw), category);
    GtkWidget *limitw = gtk_entry_new(); char lb[64]; snprintf(lb, sizeof(lb), "%.2f", limit); gtk_entry_set_text(GTK_ENTRY(limitw), lb);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Category"), 0,0,1,1); gtk_grid_attach(GTK_GRID(grid), catw, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Monthly Limit"), 0,1,1,1); gtk_grid_attach(GTK_GRID(grid), limitw, 1,1,1,1);
    gtk_container_add(GTK_CONTAINER(c), grid); gtk_widget_show_all(d);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        Budget b = {0}; snprintf(b.category, CATEGORY_LEN, "%s", gtk_entry_get_text(GTK_ENTRY(catw))); b.monthly_limit = atof(gtk_entry_get_text(GTK_ENTRY(limitw)));
        /* update in-place */
        if (update_budget(id, &b) != 0) {
            /* fallback: try add_or_update by category */
            add_or_update_budget(&b);
        }
        refresh_budgets(app);
    }
    gtk_widget_destroy(d);
    g_free(category);
}

static void on_delete_budget(GtkButton *btn, gpointer data){
    (void)btn;
    AppWidgets *app = (AppWidgets*)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->budgets_view));
    GtkTreeIter it; GtkTreeModel *m;
    if (!gtk_tree_selection_get_selected(sel, &m, &it)) return;
    int id = 0; gtk_tree_model_get(m, &it, COL_B_ID, &id, -1);
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "Delete selected budget? This cannot be undone.");
    int resp = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    if (resp == GTK_RESPONSE_YES) {
        delete_budget(id);
        refresh_budgets(app);
    }
}