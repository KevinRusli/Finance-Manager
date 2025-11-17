#include <gtk/gtk.h>
#include <stdio.h>
#include "gui.h"
#include "database.h"

static gboolean on_destroy(GtkWidget *widget, gpointer data)
{
    (void)widget; (void)data;
    close_database();
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    if (init_database("finance.db") != 0) {
        fprintf(stderr, "Failed to initialize database.\n");
        return 1;
    }
    
    const char *min_mode = getenv("FINANCE_MINIMAL");
    if (min_mode && strcmp(min_mode, "1") == 0) {
        g_print("[debug] starting in MINIMAL GUI mode\n");
        GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(win), "Finance Manager - Minimal");
        gtk_window_set_default_size(GTK_WINDOW(win), 400, 200);
        GtkWidget *label = gtk_label_new("Minimal GUI - no complex widgets");
        gtk_container_add(GTK_CONTAINER(win), label);
        g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), NULL);
        gtk_widget_show_all(win);
        gtk_main();
        return 0;
    }

    AppWidgets app = {0};
    g_print("[debug] calling build_main_window\n");
    GtkWidget *win = build_main_window(&app);
    g_print("[debug] returned from build_main_window\n");
    g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), NULL);
    g_print("[debug] calling gtk_widget_show (top-level only)\n");
    gtk_widget_show(win); 
    g_print("[debug] returned from gtk_widget_show\n");

    
    g_print("[debug] connecting deferred handlers\n");
    connect_deferred_handlers(&app);
    g_print("[debug] deferred handlers connected\n");

    
    if (app.stack) {
        g_print("[debug] showing stack\n");
        gtk_widget_show(app.stack);
    }
    if (app.notebook) {
        g_print("[debug] showing notebook\n");
        gtk_widget_show(app.notebook);
    }
    
    g_print("[debug] calling gtk_widget_show_all on top-level to ensure everything visible\n");
    gtk_widget_show_all(win);
    g_print("[debug] returned from final gtk_widget_show_all\n");
    gtk_main();
    return 0;
}


