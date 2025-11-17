#ifndef CHART_H
#define CHART_H

#include <cairo/cairo.h>
#include "utils.h"

/* Draw a pie chart of expenses by category for a given yyyymm onto provided Cairo context, within width x height. */
void draw_expense_chart(cairo_t *cr, int width, int height, const char *yyyymm);

/* Draw a bar chart showing monthly income/expense comparison */
void draw_bar_chart(cairo_t *cr, int width, int height, int months_back);

/* Draw a line chart showing spending trends over time */
void draw_line_chart(cairo_t *cr, int width, int height, const char *category, int months_back);

/* Draw forecast chart showing predicted future finances */
void draw_forecast_chart(cairo_t *cr, int width, int height, int months_ahead);

#endif /* CHART_H */


