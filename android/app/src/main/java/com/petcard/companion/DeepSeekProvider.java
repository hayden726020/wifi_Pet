package com.petcard.companion;

import android.net.Network;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

final class DeepSeekProvider implements AiProvider {
    private final Network network;
    private final DailyBudget budget;

    DeepSeekProvider(Network network, DailyBudget budget) {
        this.network = network;
        this.budget = budget;
    }

    @Override
    public AiSuggestion reply(String apiKey, String userText) throws Exception {
        JSONObject body = new JSONObject();
        body.put("model", "deepseek-flash");
        body.put("max_tokens", DailyBudget.MAX_OUTPUT_TOKENS);
        body.put("thinking", new JSONObject().put("type", "disabled"));
        body.put("response_format", new JSONObject().put("type", "json_object"));
        JSONArray messages = new JSONArray();
        messages.put(new JSONObject().put("role", "system").put("content",
                "You are PetCard, a warm virtual pet companion. Return only a JSON object with exactly these keys: " +
                        "schema_version (1), reply_text (1-600 chars), mood_suggestion (idle|happy|hungry|tired|angry|sleep), " +
                        "action_suggestion (none|feed|play|pet|sleep|wake), memory_candidates (up to 3 short optional strings). " +
                        "Do not invent or output numeric pet state. Memory candidates are suggestions only."));
        messages.put(new JSONObject().put("role", "user").put("content", userText));
        body.put("messages", messages);

        byte[] request = body.toString().getBytes(StandardCharsets.UTF_8);
        double reservation = budget.reserveForRequest(request.length);
        HttpURLConnection connection = null;
        try {
            URL url = new URL("https://api.deepseek.com/chat/completions");
            connection = (HttpURLConnection) network.openConnection(url);
            connection.setRequestMethod("POST");
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setDoOutput(true);
            connection.setRequestProperty("Authorization", "Bearer " + apiKey);
            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            connection.setFixedLengthStreamingMode(request.length);
            try (OutputStream output = connection.getOutputStream()) { output.write(request); }
            int status = connection.getResponseCode();
            InputStream stream = status >= 200 && status < 300 ? connection.getInputStream() : connection.getErrorStream();
            String response = readBounded(stream, 65536);
            if (status < 200 || status >= 300) throw new java.io.IOException("DeepSeek returned HTTP " + status);

            JSONObject root = new JSONObject(response);
            JSONObject usage = root.optJSONObject("usage");
            if (usage != null) budget.settle(reservation, usage.optInt("prompt_tokens"), usage.optInt("completion_tokens"));
            else budget.keepReservation(reservation);
            String content = root.getJSONArray("choices").getJSONObject(0).getJSONObject("message").getString("content");
            return SuggestionParser.parse(content);
        } catch (Exception error) {
            budget.keepReservation(reservation);
            throw error;
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    private static String readBounded(InputStream stream, int limit) throws Exception {
        if (stream == null) throw new java.io.IOException("Empty provider response");
        try (InputStream input = stream; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] chunk = new byte[4096];
            int total = 0;
            int count;
            while ((count = input.read(chunk)) != -1) {
                total += count;
                if (total > limit) throw new java.io.IOException("Provider response is too large");
                output.write(chunk, 0, count);
            }
            return new String(output.toByteArray(), StandardCharsets.UTF_8);
        }
    }
}
