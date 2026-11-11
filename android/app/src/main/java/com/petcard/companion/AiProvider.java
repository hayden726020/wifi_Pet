package com.petcard.companion;

interface AiProvider {
    AiSuggestion reply(String apiKey, String userText) throws Exception;
}
