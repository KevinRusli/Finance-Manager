#include "stats.h"
#include "database.h"

double get_total_income(const char *yyyymm)
{
    return get_total_by_type_for_month(yyyymm, "income");
}

double get_total_expense(const char *yyyymm)
{
    return get_total_by_type_for_month(yyyymm, "expense");
}

double get_balance(const char *yyyymm)
{
    return get_total_income(yyyymm) - get_total_expense(yyyymm);
}


