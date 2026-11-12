package com.petcard.companion;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

final class LocalMemory {
    static final int SCHEMA_VERSION = 1;
    private static final String PREFS = "local_pet_memory";
    private static final String ITEMS = "items";
    private final SharedPreferences preferences;

    LocalMemory(Context context) {
        preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    synchronized List<String> read() {
        String raw = preferences.getString(ITEMS, null);
        if (raw == null || raw.isEmpty()) return new ArrayList<>();
        JSONArray json;
        try {
            JSONObject root = new JSONObject(raw);
            if (root.optInt("version", 0) != SCHEMA_VERSION) return new ArrayList<>();
            json = root.optJSONArray("items");
            if (json == null) return new ArrayList<>();
        } catch (Exception notObject) {
            try { json = new JSONArray(raw); }
            catch (Exception broken) { return new ArrayList<>(); }
        }
        ArrayList<String> items = new ArrayList<>();
        for (int i = 0; i < json.length(); i++) {
            String item = json.optString(i, "").trim();
            if (!item.isEmpty() && item.length() <= 160 && !items.contains(item)) items.add(item);
        }
        return items;
    }

    synchronized void add(String item) {
        String value = item == null ? "" : item.trim();
        if (value.isEmpty() || value.length() > 160) return;
        List<String> items = read();
        if (!items.contains(value)) items.add(value);
        write(items);
    }

    synchronized void delete(List<String> selected) {
        if (selected == null || selected.isEmpty()) return;
        List<String> items = read();
        items.removeAll(selected);
        write(items);
    }

    synchronized void clear() {
        preferences.edit().remove(ITEMS).commit();
    }

    private void write(List<String> items) {
        while (items.size() > 50) items.remove(0);
        JSONArray json = new JSONArray();
        for (String entry : items) json.put(entry);
        JSONObject root = new JSONObject();
        try {
            root.put("version", SCHEMA_VERSION);
            root.put("items", json);
        } catch (Exception ignored) { return; }
        preferences.edit().putString(ITEMS, root.toString()).commit();
    }
}
