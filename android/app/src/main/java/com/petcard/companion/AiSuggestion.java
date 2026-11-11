package com.petcard.companion;

import java.util.List;
import java.util.ArrayList;
import java.util.Collections;

final class AiSuggestion {
    final String replyText;
    final String mood;
    final String action;
    final List<String> memoryCandidates;

    AiSuggestion(String replyText, String mood, String action, List<String> memoryCandidates) {
        this.replyText = replyText;
        this.mood = mood;
        this.action = action;
        this.memoryCandidates = Collections.unmodifiableList(new ArrayList<>(memoryCandidates));
    }
}
