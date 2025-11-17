#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <cairo/cairo.h>
#include "chart.h"
#include "database.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void draw_expense_chart(cairo_t *cr, int width, int height, const char *yyyymm)
{
    /* Allow skipping chart drawing at runtime for debugging: set FINANCE_STUB_CHART=1 */
    const char *stub_env = getenv("FINANCE_STUB_CHART");
    if (stub_env != NULL && strcmp(stub_env, "1") == 0) {
        /* draw a simple placeholder and return quickly */
        if (!cr) return;
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_set_source_rgb(cr, 0, 0, 0);
        const char *msg = "[chart drawing skipped: FINANCE_STUB_CHART=1]";
        cairo_text_extents_t ext;
        cairo_text_extents(cr, msg, &ext);
        cairo_move_to(cr, (width - ext.width) / 2.0, (height) / 2.0);
        cairo_show_text(cr, msg);
        return;
    }
    if (!cr) return;
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    char month[8 + 1];
    if (yyyymm) {
        snprintf(month, sizeof(month), "%s", yyyymm);
    } else {
        get_current_yyyymm(month);
    }

    char **cats = NULL; double *totals = NULL; int count = 0;
    if (fetch_expense_totals_by_category(month, &cats, &totals, &count) != 0 || count == 0) {
        /* Draw subtle placeholder circle */
        cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0);
        double cx = width / 2.0;
        double cy = height / 2.0;
        double radius = (width < height ? width : height) * 0.35;
        cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
        cairo_fill(cr);

        /* Draw dashed circle border */
        cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 1.0);
        double dashes[] = {4.0, 4.0};
        cairo_set_dash(cr, dashes, 2, 0.0);
        cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0.0);

        /* Draw "No data" text */
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 16);
        
        /* Center the text */
        cairo_text_extents_t extents;
        const char *msg = "No expense data yet";
        cairo_text_extents(cr, msg, &extents);
        cairo_move_to(cr, cx - extents.width/2, cy);
        cairo_show_text(cr, msg);
        
        /* Add helper text below */
        cairo_set_font_size(cr, 12);
        const char *help = "Add transactions to see the breakdown";
        cairo_text_extents(cr, help, &extents);
        cairo_move_to(cr, cx - extents.width/2, cy + 20);
        cairo_show_text(cr, help);
        return;
    }

    double sum = 0.0;
    for (int i = 0; i < count; ++i) sum += totals[i];
    if (sum <= 0.0) sum = 1.0;

    double cx = width / 2.0;
    double cy = height / 2.0;
    double radius = (width < height ? width : height) * 0.35;

    #ifndef M_PI
    #define M_PI 3.14159265358979323846
    #endif
    double angle = -M_PI / 2.0;
    for (int i = 0; i < count; ++i) {
        double r, g, b; color_from_category(cats[i], &r, &g, &b);
        double sweep = (totals[i] / sum) * 2 * M_PI;
        cairo_set_source_rgb(cr, r, g, b);
        cairo_move_to(cr, cx, cy);
        cairo_arc(cr, cx, cy, radius, angle, angle + sweep);
        cairo_close_path(cr);
        cairo_fill(cr);
        angle += sweep;
    }

    /* Legend */
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    double x = 20, y = 20;
    for (int i = 0; i < count; ++i) {
        double r, g, b; color_from_category(cats[i], &r, &g, &b);
        cairo_set_source_rgb(cr, r, g, b);
        cairo_rectangle(cr, x, y - 10, 12, 12);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
    char label[256];
    char currency[32] = "$";
    if (get_setting("currency", currency, sizeof(currency)) != 0) strncpy(currency, "$", sizeof(currency));
    snprintf(label, sizeof(label), "%s: %s%.2f", cats[i], currency, totals[i]);
        cairo_move_to(cr, x + 18, y);
        cairo_show_text(cr, label);
        y += 18;
    }

    for (int i = 0; i < count; ++i) free(cats[i]);
    free(cats); free(totals);
}

void draw_bar_chart(cairo_t *cr, int width, int height, int months_back)
{
    if (!cr) return;
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    
    char **months = NULL;
    double *income = NULL;
    double *expense = NULL;
    int count = 0;
    if (get_monthly_totals(months_back, &months, &income, &expense, &count) != 0 || count == 0) {
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, 20, height / 2);
        cairo_show_text(cr, "No data available for bar chart.");
        return;
    }
    
    double max_val = 0.0;
    for (int i = 0; i < count; ++i) {
        if (income[i] > max_val) max_val = income[i];
        if (expense[i] > max_val) max_val = expense[i];
    }
    if (max_val <= 0.0) max_val = 1.0;
    
    double margin = 60;
    double chart_width = width - 2 * margin;
    double chart_height = height - 2 * margin;
    double bar_width = chart_width / (count * 2.5);
    double spacing = bar_width * 0.3;
    
    for (int i = 0; i < count; ++i) {
        double x = margin + i * (bar_width * 2 + spacing);
        double income_height = (income[i] / max_val) * chart_height;
        double expense_height = (expense[i] / max_val) * chart_height;
        
        /* Income bar (green) */
        cairo_set_source_rgb(cr, 0.2, 0.8, 0.2);
        cairo_rectangle(cr, x, margin + chart_height - income_height, bar_width, income_height);
        cairo_fill(cr);
        
        /* Expense bar (red) */
        cairo_set_source_rgb(cr, 0.8, 0.2, 0.2);
        cairo_rectangle(cr, x + bar_width, margin + chart_height - expense_height, bar_width, expense_height);
        cairo_fill(cr);
        
        /* Month label */
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, months[i], &ext);
        cairo_move_to(cr, x + bar_width - ext.width / 2, height - margin + 15);
        cairo_show_text(cr, months[i]);
    }
    
    /* Y-axis labels */
    cairo_set_font_size(cr, 10);
    for (int i = 0; i <= 5; ++i) {
        double val = max_val * (1.0 - (double)i / 5.0);
        char label[32];
        snprintf(label, sizeof(label), "%.0f", val);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, label, &ext);
        cairo_move_to(cr, margin - ext.width - 5, margin + (chart_height * i / 5.0) + ext.height / 2);
        cairo_show_text(cr, label);
    }
    
    for (int i = 0; i < count; ++i) free(months[i]);
    free(months); free(income); free(expense);
}

void draw_line_chart(cairo_t *cr, int width, int height, const char *category, int months_back)
{
    if (!cr || !category) return;
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    
    char **months = NULL;
    double *amounts = NULL;
    int count = 0;
    if (get_category_trends(category, months_back, &months, &amounts, &count) != 0 || count == 0) {
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, 20, height / 2);
        cairo_show_text(cr, "No data available for this category.");
        return;
    }
    
    double max_val = 0.0;
    for (int i = 0; i < count; ++i) {
        if (amounts[i] > max_val) max_val = amounts[i];
    }
    if (max_val <= 0.0) max_val = 1.0;
    
    double margin = 60;
    double chart_width = width - 2 * margin;
    double chart_height = height - 2 * margin;
    
    /* Draw grid lines */
    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.5);
    for (int i = 0; i <= 5; ++i) {
        double y = margin + (chart_height * i / 5.0);
        cairo_move_to(cr, margin, y);
        cairo_line_to(cr, width - margin, y);
        cairo_stroke(cr);
    }
    
    /* Draw line */
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.8);
    cairo_set_line_width(cr, 2.0);
    for (int i = 0; i < count; ++i) {
        double x = margin + (chart_width * i / (count - 1));
        double y = margin + chart_height - (amounts[i] / max_val) * chart_height;
        if (i == 0) {
            cairo_move_to(cr, x, y);
        } else {
            cairo_line_to(cr, x, y);
        }
    }
    cairo_stroke(cr);
    
    /* Draw points */
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.8);
    for (int i = 0; i < count; ++i) {
        double x = margin + (chart_width * i / (count - 1));
        double y = margin + chart_height - (amounts[i] / max_val) * chart_height;
        cairo_arc(cr, x, y, 4, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    /* Labels */
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    for (int i = 0; i < count; i += (count > 6 ? 2 : 1)) {
        double x = margin + (chart_width * i / (count - 1));
        cairo_text_extents_t ext;
        cairo_text_extents(cr, months[i], &ext);
        cairo_move_to(cr, x - ext.width / 2, height - margin + 15);
        cairo_show_text(cr, months[i]);
    }
    
    for (int i = 0; i < count; ++i) free(months[i]);
    free(months); free(amounts);
}

void draw_forecast_chart(cairo_t *cr, int width, int height, int months_ahead)
{
    if (!cr) return;
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    
    Forecast *forecasts = NULL;
    int count = 0;
    if (generate_forecast(months_ahead, &forecasts, &count) != 0 || count == 0) {
        cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);
        cairo_move_to(cr, 20, height / 2);
        cairo_show_text(cr, "Insufficient data for forecasting.");
        return;
    }
    
    double max_val = 0.0;
    for (int i = 0; i < count; ++i) {
        if (forecasts[i].predicted_income > max_val) max_val = forecasts[i].predicted_income;
        if (forecasts[i].predicted_expense > max_val) max_val = forecasts[i].predicted_expense;
    }
    if (max_val <= 0.0) max_val = 1.0;
    
    double margin = 60;
    double chart_width = width - 2 * margin;
    double chart_height = height - 2 * margin;
    
    /* Draw bars for each month */
    double bar_width = chart_width / (count * 2.5);
    double spacing = bar_width * 0.3;
    
    for (int i = 0; i < count; ++i) {
        double x = margin + i * (bar_width * 2 + spacing);
        double income_height = (forecasts[i].predicted_income / max_val) * chart_height;
        double expense_height = (forecasts[i].predicted_expense / max_val) * chart_height;
        
        /* Predicted income (light green) */
        cairo_set_source_rgba(cr, 0.4, 0.9, 0.4, 0.7);
        cairo_rectangle(cr, x, margin + chart_height - income_height, bar_width, income_height);
        cairo_fill(cr);
        
        /* Predicted expense (light red) */
        cairo_set_source_rgba(cr, 0.9, 0.4, 0.4, 0.7);
        cairo_rectangle(cr, x + bar_width, margin + chart_height - expense_height, bar_width, expense_height);
        cairo_fill(cr);
        
        /* Month label */
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, forecasts[i].month, &ext);
        cairo_move_to(cr, x + bar_width - ext.width / 2, height - margin + 15);
        cairo_show_text(cr, forecasts[i].month);
    }
    
    free(forecasts);
}


