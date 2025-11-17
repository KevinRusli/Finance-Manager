#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "utils.h"

typedef struct AppWidgets {
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *stack; /* top-level stack to switch between dashboard and main notebook */

    /* Transactions tab */
    GtkWidget *transactions_view;
    GtkListStore *transactions_store;

    /* Budgets tab */
    GtkWidget *budgets_view;
    GtkListStore *budgets_store;

    /* Goals tab */
    GtkWidget *goals_view;
    GtkListStore *goals_store;

    /* Reports tab */
    GtkWidget *reports_box;
    GtkWidget *income_label;
    GtkWidget *expense_label;
    GtkWidget *balance_label;

    /* Charts tab */
    GtkWidget *chart_area;
    GtkWidget *chart_month_entry;
    /* Settings */
    GtkWidget *currency_entry;
    GtkWidget *currency_label;
} AppWidgets;

GtkWidget* build_main_window(AppWidgets *app);
void update_gui_tables(AppWidgets *app);
/* Connect handlers that should run after widgets are shown (post-show_all) */
void connect_deferred_handlers(AppWidgets *app);

#endif /* GUI_H */


