package com.petcard.companion;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

final class AiContext {
    final String petState;
    final List<String> memories;
    final List<String[]> history;

    AiContext(String petState, List<String> memories, List<String[]> history) {
        this.petState = petState == null ? "" : petState.trim();
        this.memories = Collections.unmodifiableList(new ArrayList<>(memories));
        ArrayList<String[]> copy = new ArrayList<>();
        for (String[] turn : history) {
            if (turn != null && turn.length == 2) copy.add(new String[]{turn[0], turn[1]});
        }
        this.history = Collections.unmodifiableList(copy);
    }

    static AiContext empty() {
        return new AiContext("", Collections.emptyList(), Collections.emptyList());
    }
}
