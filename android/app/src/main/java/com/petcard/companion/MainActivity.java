package com.petcard.companion;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Bundle;
import android.text.InputType;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private ApiKeyStore keyStore;
    private DailyBudget budget;
    private LocalMemory memory;
    private EditText keyInput;
    private EditText chatInput;
    private TextView status;
    private TextView answer;
    private TextView petState;
    private TextView memoryView;
    private CheckBox sendToCloud;
    private AiSuggestion lastSuggestion;
    private Network localWifi;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        keyStore = new ApiKeyStore(this);
        budget = new DailyBudget(this);
        memory = new LocalMemory(this);
        buildUi();
        refreshPetState();
    }

    private void buildUi() {
        int pad = dp(16);
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(pad, pad, pad, pad);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(page);

        TextView title = new TextView(this);
        title.setText("PetCard Companion");
        title.setTextSize(24);
        page.addView(title);
        petState = new TextView(this);
        page.addView(petState);
        Button refresh = new Button(this);
        refresh.setText("刷新设备状态");
        refresh.setOnClickListener(v -> refreshPetState());
        page.addView(refresh);

        TextView keyLabel = new TextView(this);
        keyLabel.setText("DeepSeek API Key（只保存在本机加密存储）");
        page.addView(keyLabel);
        keyInput = new EditText(this);
        keyInput.setSingleLine(true);
        keyInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        keyInput.setHint(keyStore.hasKey() ? "已保存；输入新值可替换" : "粘贴 API Key");
        page.addView(keyInput);
        Button saveKey = new Button(this);
        saveKey.setText("保存 API Key");
        saveKey.setOnClickListener(v -> saveKey());
        page.addView(saveKey);
        Button clearKey = new Button(this);
        clearKey.setText("删除已保存的 Key");
        clearKey.setOnClickListener(v -> clearKey());
        page.addView(clearKey);

        sendToCloud = new CheckBox(this);
        sendToCloud.setText("我知道发送后聊天内容会发给 DeepSeek");
        sendToCloud.setChecked(false);
        page.addView(sendToCloud);
        chatInput = new EditText(this);
        chatInput.setHint("和宠物说句话");
        chatInput.setMinLines(2);
        chatInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        page.addView(chatInput);
        Button ask = new Button(this);
        ask.setText("联网聊天（蜂窝网络）");
        ask.setOnClickListener(v -> askProvider());
        page.addView(ask);
        Button demo = new Button(this);
        demo.setText("离线体验假模型");
        demo.setOnClickListener(v -> showSuggestion(fakeSuggestion(chatInput.getText().toString())));
        page.addView(demo);

        answer = new TextView(this);
        answer.setTextSize(16);
        page.addView(answer);
        Button applyAction = new Button(this);
        applyAction.setText("确认并执行建议动作");
        applyAction.setOnClickListener(v -> confirmAction());
        page.addView(applyAction);
        Button saveMemory = new Button(this);
        saveMemory.setText("查看并保存 AI 记忆建议");
        saveMemory.setOnClickListener(v -> approveMemories());
        page.addView(saveMemory);
        memoryView = new TextView(this);
        List<String> memories = memory.read();
        memoryView.setText("本机记忆：" + (memories.isEmpty() ? "暂无" : String.join("；", memories)));
        page.addView(memoryView);
        Button clearMemory = new Button(this);
        clearMemory.setText("清空本机记忆");
        clearMemory.setOnClickListener(v -> new AlertDialog.Builder(this)
                .setTitle("清空本机记忆？")
                .setMessage("这会删除本机保存的所有 AI 记忆条目。")
                .setNegativeButton("取消", null)
                .setPositiveButton("清空", (dialog, which) -> { memory.clear(); refreshMemory(); setStatus("本机记忆已清空。"); })
                .show());
        page.addView(clearMemory);
        status = new TextView(this);
        status.setText("每日 App 预算目标：¥5.00（本机估算；调整系统日期或清除数据会影响累计；同一 Key 的其他用量不计）");
        page.addView(status);
        setContentView(scroll);
    }

    private void saveKey() {
        try {
            keyStore.save(keyInput.getText().toString());
            keyInput.setText("");
            keyInput.setHint("已保存；输入新值可替换");
            setStatus("Key 已加密保存到 Android Keystore 保护的本机存储。");
        } catch (Exception error) { setStatus("保存失败：" + safeMessage(error)); }
    }

    private void clearKey() {
        try { keyStore.clear(); keyInput.setText(""); keyInput.setHint("粘贴 API Key"); setStatus("已删除本机保存的 Key。"); }
        catch (Exception error) { setStatus("删除失败：" + safeMessage(error)); }
    }

    private void askProvider() {
        String prompt = chatInput.getText().toString().trim();
        if (prompt.isEmpty() || prompt.length() > 2000) { setStatus("请输入 1–2000 个字符。"); return; }
        if (!sendToCloud.isChecked()) { setStatus("请确认聊天内容会发送给 DeepSeek 后再联网聊天；也可使用离线假模型。"); return; }
        worker.execute(() -> {
            try {
                String key = keyStore.read();
                if (key == null || key.isEmpty()) throw new IllegalStateException("请先保存 API Key。");
                Network mobile = findCellularNetwork();
                if (mobile == null) throw new IllegalStateException("未找到可用蜂窝网络；请保持设备 Wi‑Fi 连接并开启手机蜂窝数据。");
                AiSuggestion suggestion = new DeepSeekProvider(mobile, budget).reply(key, prompt);
                runOnUiThread(() -> showSuggestion(suggestion));
            } catch (Exception error) { runOnUiThread(() -> setStatus(safeMessage(error))); }
        });
        setStatus("正在通过蜂窝网络请求 DeepSeek…");
    }

    private Network findCellularNetwork() {
        ConnectivityManager manager = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        if (manager == null) return null;
        for (Network network : manager.getAllNetworks()) {
            NetworkCapabilities caps = manager.getNetworkCapabilities(network);
            if (caps != null && caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
                    && caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                    && caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) return network;
        }
        return null;
    }

    private AiSuggestion fakeSuggestion(String prompt) {
        String text = prompt == null ? "" : prompt.trim();
        String action = "none";
        if (text.contains("喂")) action = "feed";
        else if (text.contains("玩")) action = "play";
        else if (text.contains("摸")) action = "pet";
        else if (text.contains("睡")) action = "sleep";
        else if (text.contains("醒")) action = "wake";
        return new AiSuggestion(text.isEmpty() ? "我在这里陪着你。" : "听到啦：" + text, "happy", action, java.util.Collections.emptyList());
    }

    private void showSuggestion(AiSuggestion suggestion) {
        lastSuggestion = suggestion;
        answer.setText("宠物：" + suggestion.replyText + "\n情绪建议：" + suggestion.mood + "\n动作建议：" + suggestion.action
                + (suggestion.memoryCandidates.isEmpty() ? "" : "\n记忆候选：" + String.join("；", suggestion.memoryCandidates)));
        setStatus("收到建议。状态数值由设备本地 PetCore 管理；动作和记忆均需你确认。");
    }

    private void confirmAction() {
        AiSuggestion suggestion = lastSuggestion;
        if (suggestion == null || "none".equals(suggestion.action)) { setStatus("当前没有待执行动作建议。"); return; }
        new AlertDialog.Builder(this).setTitle("执行宠物动作？")
                .setMessage("AI 建议执行“" + suggestion.action + "”。确认后将调用设备本地控制接口。")
                .setNegativeButton("取消", null)
                .setPositiveButton("确认", (dialog, which) -> executeLocalAction(suggestion.action)).show();
    }

    private void approveMemories() {
        AiSuggestion suggestion = lastSuggestion;
        if (suggestion == null || suggestion.memoryCandidates.isEmpty()) { setStatus("当前没有记忆候选。"); return; }
        CharSequence[] options = suggestion.memoryCandidates.toArray(new CharSequence[0]);
        boolean[] checked = new boolean[options.length];
        new AlertDialog.Builder(this).setTitle("选择保存到本机的记忆").setMultiChoiceItems(options, checked,
                (dialog, which, selected) -> checked[which] = selected)
                .setNegativeButton("取消", null)
                .setPositiveButton("保存所选", (dialog, which) -> {
                    for (int i = 0; i < options.length; i++) if (checked[i]) memory.add(options[i].toString());
                    refreshMemory();
                    setStatus("已保存所选内容到本机记忆；不会自动上传。");
                }).show();
    }

    private void refreshMemory() {
        List<String> memories = memory.read();
        memoryView.setText("本机记忆：" + (memories.isEmpty() ? "暂无" : String.join("；", memories)));
    }

    private void executeLocalAction(String action) {
        if (!(action.equals("feed") || action.equals("play") || action.equals("pet") || action.equals("sleep") || action.equals("wake"))) {
            setStatus("动作未通过本地允许列表校验。"); return;
        }
        worker.execute(() -> {
            try {
                Network wifi = findLocalWifiNetwork();
                if (wifi == null) throw new IllegalStateException("设备 Wi‑Fi 未连接。");
                HttpURLConnection connection = (HttpURLConnection) wifi.openConnection(new URL("http://192.168.0.1:8420/action?do=" + action));
                connection.setConnectTimeout(5000);
                connection.setReadTimeout(5000);
                int code = connection.getResponseCode();
                connection.disconnect();
                runOnUiThread(() -> setStatus(code == 200 ? "动作已发送到设备。" : "设备返回 HTTP " + code));
            } catch (Exception error) { runOnUiThread(() -> setStatus("本地动作失败：" + safeMessage(error))); }
        });
    }

    private void refreshPetState() {
        worker.execute(() -> {
            try {
                Network wifi = findLocalWifiNetwork();
                if (wifi == null) throw new IllegalStateException("请先连接设备 Wi‑Fi。");
                HttpURLConnection connection = (HttpURLConnection) wifi.openConnection(new URL("http://192.168.0.1:8420/state"));
                connection.setConnectTimeout(4000);
                connection.setReadTimeout(4000);
                String data;
                try (InputStream input = connection.getInputStream(); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                    byte[] chunk = new byte[2048]; int count;
                    while ((count = input.read(chunk)) != -1) { if (output.size() + count > 16384) throw new IllegalStateException("设备状态响应过大。"); output.write(chunk, 0, count); }
                    data = new String(output.toByteArray(), StandardCharsets.UTF_8);
                } finally { connection.disconnect(); }
                JSONObject state = new JSONObject(data);
                String summary = "设备：" + state.optString("stage_name", "PetCore") + " · EXP " + state.optInt("exp")
                        + " · 互动 " + state.optInt("interactions") + (state.optInt("sleeping") == 1 ? " · 睡眠中" : "");
                runOnUiThread(() -> petState.setText(summary));
            } catch (Exception error) { runOnUiThread(() -> petState.setText("设备状态：" + safeMessage(error))); }
        });
    }

    private Network findLocalWifiNetwork() {
        ConnectivityManager manager = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        if (manager == null) return null;
        for (Network network : manager.getAllNetworks()) {
            NetworkCapabilities caps = manager.getNetworkCapabilities(network);
            if (caps != null && caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return network;
        }
        return null;
    }

    private void setStatus(String text) { status.setText(text + "\n今日 App 预算预留/用量：¥" + String.format(java.util.Locale.ROOT, "%.4f", budget.spentToday())); }
    private static String safeMessage(Exception error) { return error.getMessage() == null ? "请求失败" : error.getMessage(); }
    private int dp(int value) { return (int) (value * getResources().getDisplayMetrics().density + 0.5f); }

    @Override public void onDestroy() { worker.shutdownNow(); super.onDestroy(); }
}
