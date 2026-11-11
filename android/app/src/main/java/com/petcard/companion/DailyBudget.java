package com.petcard.companion;

import android.content.Context;
import android.content.SharedPreferences;

import java.time.LocalDate;
import java.time.ZoneOffset;

final class DailyBudget {
    static final double LIMIT_CNY = 5.00;
    static final int MAX_OUTPUT_TOKENS = 512;

    // Official peak rates as of 2026-11-11; use 8 CNY/USD for FX headroom.
    private static final double INPUT_USD_PER_MILLION = 0.30;
    private static final double OUTPUT_USD_PER_MILLION = 1.20;
    private static final double CNY_PER_USD_GUARD = 8.0;
    private static final double SAFETY_MARGIN = 1.25;
    private static final String PREFS = "daily_api_budget";
    private static final String DAY = "day";
    private static final String SPENT = "reserved_cny";
    private final SharedPreferences preferences;

    DailyBudget(Context context) {
        preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    synchronized double reserveForRequest(int requestUtf8Bytes) throws BudgetExceededException {
        String today = LocalDate.now(ZoneOffset.UTC).toString();
        String storedDay = preferences.getString(DAY, "");
        double spent = today.equals(storedDay) ? Double.longBitsToDouble(preferences.getLong(SPENT, 0L)) : 0.0;
        double inputUpperBound = Math.max(1, requestUtf8Bytes);
        double usd = (inputUpperBound * INPUT_USD_PER_MILLION
                + MAX_OUTPUT_TOKENS * OUTPUT_USD_PER_MILLION) / 1_000_000.0;
        double reservation = usd * CNY_PER_USD_GUARD * SAFETY_MARGIN;
        if (spent + reservation > LIMIT_CNY) throw new BudgetExceededException();
        if (!store(today, spent + reservation)) throw new BudgetExceededException();
        return reservation;
    }

    synchronized void settle(double reservation, int promptTokens, int completionTokens) {
        String today = LocalDate.now(ZoneOffset.UTC).toString();
        String storedDay = preferences.getString(DAY, "");
        double spent = today.equals(storedDay) ? Double.longBitsToDouble(preferences.getLong(SPENT, 0L)) : 0.0;
        double actual = ((Math.max(0, promptTokens) * INPUT_USD_PER_MILLION
                + Math.max(0, completionTokens) * OUTPUT_USD_PER_MILLION) / 1_000_000.0)
                * CNY_PER_USD_GUARD * SAFETY_MARGIN;
        store(today, Math.max(0.0, spent - reservation) + actual);
    }

    synchronized void keepReservation(double reservation) {
        // A failed or incomplete response may still be billable. Keep the preflight reserve.
    }

    synchronized double spentToday() {
        String today = LocalDate.now(ZoneOffset.UTC).toString();
        if (!today.equals(preferences.getString(DAY, ""))) return 0.0;
        return Double.longBitsToDouble(preferences.getLong(SPENT, 0L));
    }

    private boolean store(String day, double amount) {
        return preferences.edit().putString(DAY, day).putLong(SPENT, Double.doubleToLongBits(amount)).commit();
    }

    static final class BudgetExceededException extends Exception {
        BudgetExceededException() { super("Daily DeepSeek budget reached"); }
    }
}
