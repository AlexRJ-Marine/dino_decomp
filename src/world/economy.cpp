#include "economy.h"

namespace dino {

void BudgetLedger::spend_food(int amount) {
    food_total += amount;
    ++food_count;
}

void BudgetLedger::spend_dinoachat(int amount) {
    dinoachat_total += amount;
    ++dinoachat_count;
}

void BudgetLedger::spend_laboaction(int amount) {
    laboaction_total += amount;
    ++laboaction_count;
}

void BudgetLedger::spend_construction(int amount) {
    construction_total += amount;
    ++construction_count;
}

void BudgetLedger::spend_secu(int amount) {
    secu_total += amount;
    ++secu_count;
}

void BudgetLedger::spend_staff(int role, int amount) {
    if (role == 0) {
        staff_food_total += amount;
    }
}

void BudgetLedger::add_ticket(int amount, int count) {
    ticket_total += amount;
    ticket_count += count;
}

void BudgetLedger::add_sale(int kind, int amount) {
    if (kind >= 1 && kind <= 6) {
        shop_revenue[static_cast<std::size_t>(kind)] += amount;
        ++shop_sales[static_cast<std::size_t>(kind)];
    }
}

void BudgetLedger::add_vente(int amount) {
    ventes_total += amount;
    ++ventes_count;
}

void BudgetLedger::settle() {
    y_food_total = food_total;
    y_food_count = food_count;
    y_staff_food_total = staff_food_total;
    y_secu_total = secu_total;
    y_secu_count = secu_count;
    y_dinoachat_total = dinoachat_total;
    y_dinoachat_count = dinoachat_count;
    y_laboaction_total = laboaction_total;
    y_laboaction_count = laboaction_count;
    y_construction_total = construction_total;
    y_construction_count = construction_count;
    y_ticket_total = ticket_total;
    y_ticket_count = ticket_count;
    y_shop_revenue = shop_revenue;
    y_shop_sales = shop_sales;
    y_ventes_total = ventes_total;
    y_ventes_count = ventes_count;

    food_total = 0;
    food_count = 0;
    staff_food_total = 0;
    secu_total = 0;
    secu_count = 0;
    dinoachat_total = 0;
    dinoachat_count = 0;
    laboaction_total = 0;
    laboaction_count = 0;
    construction_total = 0;
    construction_count = 0;
    ticket_total = 0;
    ticket_count = 0;
    shop_revenue.fill(0);
    shop_sales.fill(0);
    ventes_total = 0;
    ventes_count = 0;
}

int BudgetLedger::expenses() const {
    return y_staff_food_total + y_food_total + y_secu_total +
           y_dinoachat_total + y_construction_total + y_laboaction_total;
}

int BudgetLedger::income() const {
    int total = y_ticket_total + y_ventes_total;
    for (std::size_t i = 1; i < y_shop_revenue.size(); ++i) {
        total += y_shop_revenue[i];
    }
    return total;
}

}  // namespace dino
