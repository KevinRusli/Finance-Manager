#include "utils.h"
#include "database.h"
#include <ctype.h>
#include <math.h>

static int days_in_month(int year, int month)
{
    static const int dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int d = dim[month - 1];
    if (month == 2) {
        int leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        if (leap) d = 29;
    }
    return d;
}

void get_current_yyyymm(char out_yyyymm[8 + 1])
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (!lt) { strcpy(out_yyyymm, "1970-01"); return; }
    strftime(out_yyyymm, 10, "%Y-%m", lt);
}

void get_current_yyyymmdd(char out_date[DATE_LEN])
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (!lt) { strcpy(out_date, "1970-01-01"); return; }
    strftime(out_date, DATE_LEN, "%Y-%m-%d", lt);
}

int yyyymm_from_date(const char *yyyy_mm_dd, char out_yyyymm[8 + 1])
{
    if (!yyyy_mm_dd || strlen(yyyy_mm_dd) < 7) return -1;
    strncpy(out_yyyymm, yyyy_mm_dd, 7);
    out_yyyymm[7] = '\0';
    return 0;
}

int add_months_to_yyyymmdd(const char *yyyy_mm_dd, int months, char out_date[DATE_LEN])
{
    if (!yyyy_mm_dd || strlen(yyyy_mm_dd) < 10) return -1;
    int y = 0, m = 0, d = 0;
    if (sscanf(yyyy_mm_dd, "%d-%d-%d", &y, &m, &d) != 3) return -1;
    int total = y * 12 + (m - 1) + months;
    int ny = total / 12;
    int nm = (total % 12) + 1;
    int nd = d;
    int maxd = days_in_month(ny, nm);
    if (nd > maxd) nd = maxd;
    /* Use a temporary buffer larger than DATE_LEN to avoid snprintf truncation warnings
     * and then copy into the provided out_date with explicit truncation. */
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%04d-%02d-%02d", ny, nm, nd);
    /* Copy safely into out_date which has size DATE_LEN (including NUL) */
    strncpy(out_date, tmp, DATE_LEN - 1);
    out_date[DATE_LEN - 1] = '\0';
    return 0;
}

void color_from_category(const char *category, double *r, double *g, double *b)
{
    unsigned long hash = 5381;
    const unsigned char *p = (const unsigned char*) (category ? category : "");
    while (*p) hash = ((hash << 5) + hash) + *p++;
    /* Map to visually distinct pastel colors */
    *r = ((hash >> 16) & 0xFF) / 255.0 * 0.6 + 0.2;
    *g = ((hash >> 8) & 0xFF) / 255.0 * 0.6 + 0.2;
    *b = (hash & 0xFF) / 255.0 * 0.6 + 0.2;
}

int export_to_csv(const char *filename)
{
    if (!filename) return -1;
    FILE *f = fopen(filename, "w");
    if (!f) return -1;
    Transaction *list = NULL;
    int count = 0;
    int rc = fetch_transactions_all(&list, &count);
    if (rc != 0) { fclose(f); return rc; }
    fprintf(f, "id,type,category,amount,date,note\n");
    for (int i = 0; i < count; ++i) {
        /* naive CSV escaping for commas and quotes */
        char note_escaped[NOTE_LEN * 2 + 2];
        int pos = 0;
        note_escaped[pos++] = '"';
        for (const char *c = list[i].note; *c && pos < (int)sizeof(note_escaped)-2; ++c) {
            if (*c == '"') note_escaped[pos++] = '"';
            note_escaped[pos++] = *c;
        }
        note_escaped[pos++] = '"';
        note_escaped[pos] = '\0';
        fprintf(f, "%d,%s,%s,%.2f,%s,%s\n",
                list[i].id, list[i].type, list[i].category, list[i].amount, list[i].date, note_escaped);
    }
    fclose(f);
    free(list);
    return 0;
}

/* Format amount with simple thousands separator and currency prefix.
 * out receives something like "$1,234.56". out_size must be sufficient.
 */
void format_amount_currency(double amount, const char *currency, char *out, int out_size)
{
    if (!out || out_size <= 0) return;
    if (!currency) currency = "";
    /* We'll produce: <optional-sign><currency><integer_with_commas>.<two_decimals> */
    double absamt = fabs(amount);
    long long whole = (long long)floor(absamt);
    int cents = (int)llround((absamt - (double)whole) * 100.0);
    if (cents == 100) { whole += 1; cents = 0; }

    char intbuf[64];
    int ibpos = sizeof(intbuf) - 1; intbuf[ibpos] = '\0';
    int digit_count = 0;
    if (whole == 0) { intbuf[--ibpos] = '0'; }
    else {
        long long tmpwhole = whole;
        while (tmpwhole > 0 && ibpos > 0) {
            if (digit_count == 3) {
                intbuf[--ibpos] = ',';
                digit_count = 0;
            }
            intbuf[--ibpos] = '0' + (tmpwhole % 10);
            tmpwhole /= 10;
            digit_count++;
        }
    }
    const char *sign = amount < 0 ? "-" : "";
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s%s%s.%02d", sign, currency, &intbuf[ibpos], cents);
    strncpy(out, tmp, out_size-1); out[out_size-1] = '\0';
}

/* Very small validator for currency input: allow 1-4 chars of printable (letters or currency symbols)
 * Returns 1 if valid, 0 otherwise.
 */
int is_valid_currency_string(const char *s)
{
    if (!s) return 0;
    size_t len = strlen(s);
    if (len == 0 || len > 4) return 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (isalpha(c) || isdigit(c)) continue;
        /* Accept punctuation (covers $) and also allow high-bit bytes (UTF-8 symbols like €, £, ¥) */
        if (ispunct(c) || c >= 0x80) continue;
        return 0;
    }
    return 1;
}


