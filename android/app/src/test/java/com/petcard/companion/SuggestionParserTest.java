package com.petcard.companion;

import org.json.JSONException;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public final class SuggestionParserTest {
    private static String valid() {
        return "{\"schema_version\":1,\"reply_text\":\"hello\",\"mood_suggestion\":\"happy\","
                + "\"action_suggestion\":\"feed\",\"memory_candidates\":[\"likes tea\"]}";
    }

    @Test public void parsesValidSuggestion() throws Exception {
        AiSuggestion suggestion = SuggestionParser.parse(valid());
        assertEquals("hello", suggestion.replyText);
        assertEquals("happy", suggestion.mood);
        assertEquals("feed", suggestion.action);
        assertEquals(1, suggestion.memoryCandidates.size());
        assertEquals("likes tea", suggestion.memoryCandidates.get(0));
    }

    @Test public void rejectsUnknownField() {
        assertRejected(valid().replace("\"reply_text\"", "\"hunger\":50,\"reply_text\""));
    }

    @Test public void rejectsMissingField() {
        assertRejected("{\"schema_version\":1,\"reply_text\":\"hi\",\"mood_suggestion\":\"happy\","
                + "\"action_suggestion\":\"none\"}");
    }

    @Test public void rejectsWrongSchemaVersion() {
        assertRejected(valid().replace("\"schema_version\":1", "\"schema_version\":2"));
    }

    @Test public void rejectsUnknownMood() {
        assertRejected(valid().replace("\"happy\"", "\"excited\""));
    }

    @Test public void rejectsUnknownAction() {
        assertRejected(valid().replace("\"feed\"", "\"dance\""));
    }

    @Test public void rejectsEmptyReply() {
        assertRejected(valid().replace("\"hello\"", "\"  \""));
    }

    @Test public void rejectsLongReply() {
        StringBuilder reply = new StringBuilder();
        for (int i = 0; i < 601; i++) reply.append('a');
        assertRejected(valid().replace("\"hello\"", "\"" + reply + "\""));
    }

    @Test public void rejectsTooManyMemories() {
        assertRejected(valid().replace("[\"likes tea\"]", "[\"a\",\"b\",\"c\",\"d\"]"));
    }

    @Test public void rejectsLongMemory() {
        StringBuilder memory = new StringBuilder();
        for (int i = 0; i < 161; i++) memory.append('m');
        assertRejected(valid().replace("likes tea", memory.toString()));
    }

    @Test public void rejectsNumericReply() {
        assertRejected(valid().replace("\"hello\"", "123"));
    }

    @Test public void rejectsNumericMemory() {
        assertRejected(valid().replace("[\"likes tea\"]", "[123]"));
    }

    @Test public void rejectsMalformedJson() {
        assertRejected("{not json");
    }

    private static void assertRejected(String raw) {
        try {
            SuggestionParser.parse(raw);
            fail("expected JSONException for: " + raw);
        } catch (JSONException expected) {
            assertTrue(true);
        }
    }
}
