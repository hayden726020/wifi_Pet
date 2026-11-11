package com.petcard.companion;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;

import java.util.ArrayList;
import java.util.List;

final class LocalMemory {
    private static final String PREFS = "local_pet_memory";
    private static final String ITEMS = "items";
    private final SharedPreferences preferences;

    LocalMemory(Context context) {
        preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    synchronized List<String> read() {
        JSONArray json;
        try { json = new JSONArray(preferences.getString(ITEMS, "[]")); }
        catch (Exception ignored) { json = new JSONArray(); }
        ArrayList<String> items = new ArrayList<>();
        for (int i = 0; i < json.length(); i++) {
            String item = json.optString(i, "").trim();
            if (!item.isEmpty() && item.length() <= 160) items.add(item);
        }
        return items;
    }

    synchronized void add(String item) {
        String value = item == null ? "" : item.trim();
        if (value.isEmpty() || value.length() > 160) return;
        List<String> items = read();
        if (!items.contains(value)) items.add(value);
        while (items.size() > 50) items.remove(0);
        JSONArray json = new JSONArray();
        for (String entry : items) json.put(entry);
        preferences.edit().putString(ITEMS, json.toString()).apply();
    }

    synchronized void clear() {
        preferences.edit().remove(ITEMS).apply();
    }
}
