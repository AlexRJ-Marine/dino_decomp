#pragma once

#include <array>

namespace dino {

// Port-side mirror of the park budget ledger (`spPark` +0x00..+0x16c, see
// docs/decomp/economy.md). The panel shows the *yesterday* block; settle()
// rolls the current-day counters into it and recomputes the totals.
struct BudgetLedger {
    int food_total = 0;
    int food_count = 0;
    int staff_food_total = 0;  // role-0 employee bucket
    int secu_total = 0;
    int secu_count = 0;
    int dinoachat_total = 0;
    int dinoachat_count = 0;
    int laboaction_total = 0;
    int laboaction_count = 0;
    int construction_total = 0;
    int construction_count = 0;
    int ticket_total = 0;
    int ticket_count = 0;
    std::array<int, 7> shop_revenue{};  // kinds 1..6; index 0 unused
    std::array<int, 7> shop_sales{};
    int ventes_total = 0;
    int ventes_count = 0;

    int y_food_total = 0;
    int y_food_count = 0;
    int y_staff_food_total = 0;
    int y_secu_total = 0;
    int y_secu_count = 0;
    int y_dinoachat_total = 0;
    int y_dinoachat_count = 0;
    int y_laboaction_total = 0;
    int y_laboaction_count = 0;
    int y_construction_total = 0;
    int y_construction_count = 0;
    int y_ticket_total = 0;
    int y_ticket_count = 0;
    std::array<int, 7> y_shop_revenue{};
    std::array<int, 7> y_shop_sales{};
    int y_ventes_total = 0;
    int y_ventes_count = 0;

    // Current-day spend setters (retail 0x585c90 / 0x585cc0 / 0x585cf0 /
    // 0x585d20 / 0x585d50 / 0x585d80). Budget deduction is the caller's.
    void spend_food(int amount);
    void spend_dinoachat(int amount);
    void spend_laboaction(int amount);
    void spend_construction(int amount);
    void spend_secu(int amount);
    void spend_staff(int role, int amount);

    // Income setters (retail 0x585db0 / 0x585de0).
    void add_ticket(int amount, int count = 1);
    void add_sale(int kind, int amount);
    void add_vente(int amount);

    // 0x585e10: copy current -> yesterday, reset the current counters.
    void settle();

    // 0x586070: totals from the yesterday block.
    int expenses() const;
    int income() const;
    int net() const { return income() - expenses(); }
};

}  // namespace dino
