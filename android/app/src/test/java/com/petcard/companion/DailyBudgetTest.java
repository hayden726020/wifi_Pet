package com.petcard.companion;

import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

public final class DailyBudgetTest {
    @Test public void reservationIsPositiveAndBelowDailyLimit() {
        double reservation = DailyBudget.reservationCny(2000);
        assertTrue(reservation > 0.0);
        assertTrue(reservation < DailyBudget.LIMIT_CNY);
    }

    @Test public void reservationGrowsWithRequestSize() {
        assertTrue(DailyBudget.reservationCny(8000) > DailyBudget.reservationCny(500));
    }

    @Test public void costPinsConservativeRates() {
        assertEquals(3.0, DailyBudget.costCny(1_000_000, 0), 1e-9);
        assertEquals(12.0, DailyBudget.costCny(0, 1_000_000), 1e-9);
        assertEquals(0.0, DailyBudget.costCny(0, 0), 0.0);
    }

    @Test public void maxOutputCostMatchesReservationFloor() {
        double outputOnly = DailyBudget.costCny(0, DailyBudget.MAX_OUTPUT_TOKENS);
        assertTrue(outputOnly > 0.0);
        assertTrue(outputOnly < 0.01);
    }
}
