package com.petcard.companion;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.Collections;

final class SuggestionParser {
    private static final Set<String> KEYS = Collections.unmodifiableSet(new HashSet<>(Arrays.asList(
            "schema_version", "reply_text", "mood_suggestion", "action_suggestion", "memory_candidates")));
    private static final Set<String> MOODS = new HashSet<>(Arrays.asList(
            "idle", "happy", "hungry", "tired", "angry", "sleep"));
    private static final Set<String> ACTIONS = new HashSet<>(Arrays.asList(
            "none", "feed", "play", "pet", "sleep", "wake"));

    private SuggestionParser() {}

    static AiSuggestion parse(String raw) throws JSONException {
        JSONObject value = new JSONObject(raw);
        Set<String> actualKeys = new HashSet<>();
        value.keys().forEachRemaining(actualKeys::add);
        if (!KEYS.equals(actualKeys) || value.getInt("schema_version") != 1) {
            throw new JSONException("Unsupported suggestion schema");
        }

        String reply = value.getString("reply_text").trim();
        String mood = value.getString("mood_suggestion");
        String action = value.getString("action_suggestion");
        JSONArray candidates = value.getJSONArray("memory_candidates");
        if (reply.isEmpty() || reply.length() > 600 || !MOODS.contains(mood)
                || !ACTIONS.contains(action) || candidates.length() > 3) {
            throw new JSONException("Suggestion is outside allowed limits");
        }

        ArrayList<String> memories = new ArrayList<>();
        for (int i = 0; i < candidates.length(); i++) {
            String memory = candidates.getString(i).trim();
            if (memory.isEmpty() || memory.length() > 160) {
                throw new JSONException("Invalid memory candidate");
            }
            memories.add(memory);
        }
        return new AiSuggestion(reply, mood, action, memories);
    }
}
