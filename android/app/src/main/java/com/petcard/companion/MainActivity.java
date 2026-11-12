package com.petcard.companion;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.graphics.Insets;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
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
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private static final String PET_STATE_URL = "http://192.168.0.1:8420/state";
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final ArrayList<String[]> conversation = new ArrayList<>();
    private ApiKeyStore keyStore;
    private DailyBudget budget;
    private LocalMemory memory;
    private EditText keyInput;
    private EditText chatInput;
    private ScrollView pageScroll;
    private TextView status;
    private TextView answer;
    private TextView petState;
    private TextView memoryView;
    private TextView budgetView;
    private CheckBox sendToCloud;
    private CheckBox shareMemories;
    private Button askButton;
    private AiSuggestion lastSuggestion;
    private String lastPetContext = "";
    private boolean requestInFlight;

    @Override
    public void onCreate(Bundle state) {
        super.onCreate(state);
        if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0) {
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE);
        }
        keyStore = new ApiKeyStore(this);
        budget = new DailyBudget(this);
        memory = new LocalMemory(this);
        buildUi();
        applyEdgeToEdge();
        refreshBudget();
        refreshPetState();
    }

    private void applyEdgeToEdge() {
        Window window = getWindow();
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
        } else {
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
        pageScroll.setOnApplyWindowInsetsListener((view, insets) -> {
            int left;
            int top;
            int right;
            int bottom;
            if (Build.VERSION.SDK_INT >= 30) {
                Insets bars = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                Insets ime = insets.getInsets(WindowInsets.Type.ime());
                left = bars.left;
                top = bars.top;
                right = bars.right;
                bottom = Math.max(bars.bottom, ime.bottom);
            } else {
                left = insets.getSystemWindowInsetLeft();
                top = insets.getSystemWindowInsetTop();
                right = insets.getSystemWindowInsetRight();
                bottom = insets.getSystemWindowInsetBottom();
            }
            view.setPadding(left, top, right, bottom);
            return insets;
        });
        pageScroll.requestApplyInsets();
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
        sendToCloud.setText("我知道聊天内容、最近对话和宠物状态会发送给 DeepSeek");
        sendToCloud.setChecked(false);
        page.addView(sendToCloud);
        shareMemories = new CheckBox(this);
        shareMemories.setText("允许把已保存的本机记忆作为上下文发送（默认关闭）");
        shareMemories.setChecked(false);
        page.addView(shareMemories);
        chatInput = new EditText(this);
        chatInput.setHint("和宠物说句话");
        chatInput.setMinLines(2);
        chatInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES | InputType.TYPE_TEXT_FLAG_MULTI_LINE);
        page.addView(chatInput);
        askButton = new Button(this);
        askButton.setText("联网聊天（蜂窝网络）");
        askButton.setOnClickListener(v -> askProvider());
        page.addView(askButton);
        Button demo = new Button(this);
        demo.setText("离线体验假模型");
        demo.setOnClickListener(v -> {
            String prompt = chatInput.getText().toString();
            AiSuggestion suggestion = fakeSuggestion(prompt);
            rememberTurn("user", prompt.trim().isEmpty() ? "离线体验" : prompt.trim());
            rememberTurn("assistant", suggestion.replyText);
            showSuggestion(suggestion);
        });
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
        page.addView(memoryView);
        refreshMemory();
        Button deleteMemory = new Button(this);
        deleteMemory.setText("删除单条记忆");
        deleteMemory.setOnClickListener(v -> deleteMemories());
        page.addView(deleteMemory);
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
        status.setText("就绪。");
        page.addView(status);
        budgetView = new TextView(this);
        page.addView(budgetView);
        pageScroll = scroll;
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
        if (requestInFlight) { setStatus("上一个请求还在进行中，请稍候。"); return; }
        String prompt = chatInput.getText().toString().trim();
        if (prompt.isEmpty() || prompt.length() > 2000) { setStatus("请输入 1–2000 个字符。"); return; }
        if (!sendToCloud.isChecked()) { setStatus("请确认聊天内容会发送给 DeepSeek 后再联网聊天；也可使用离线假模型。"); return; }
        boolean includeMemories = shareMemories.isChecked();
        final String cachedPetContext = lastPetContext;
        requestInFlight = true;
        askButton.setEnabled(false);
        lastSuggestion = null;
        answer.setText("");
        setStatus("正在通过蜂窝网络请求 DeepSeek…");
        worker.execute(() -> {
            AiSuggestion result = null;
            String failure = null;
            try {
                String key = keyStore.read();
                if (key == null || key.isEmpty()) throw new IllegalStateException("请先保存 API Key。");
                Network mobile = findCellularNetwork();
                if (mobile == null) throw new IllegalStateException("未找到可用蜂窝网络；请开启手机移动数据。");
                String petContext = fetchPetContext();
                if (petContext.isEmpty()) petContext = cachedPetContext;
                List<String[]> history;
                synchronized (conversation) { history = new ArrayList<>(conversation); }
                List<String> memories = includeMemories ? memory.read() : Collections.emptyList();
                result = new DeepSeekProvider(mobile, budget).reply(key, prompt, new AiContext(petContext, memories, history));
            } catch (Exception error) {
                failure = safeMessage(error);
            }
            final AiSuggestion suggestion = result;
            final String errorText = failure;
            runOnUiThread(() -> {
                requestInFlight = false;
                askButton.setEnabled(true);
                refreshBudget();
                if (suggestion != null) {
                    rememberTurn("user", prompt);
                    rememberTurn("assistant", suggestion.replyText);
                    showSuggestion(suggestion);
                } else {
                    setStatus(errorText);
                }
            });
        });
    }

    private void rememberTurn(String role, String text) {
        String value = text == null ? "" : text.trim();
        if (value.isEmpty()) return;
        synchronized (conversation) {
            conversation.add(new String[]{role, value});
            while (conversation.size() > DeepSeekProvider.MAX_HISTORY_TURNS) conversation.remove(0);
        }
    }

    private Network findCellularNetwork() {
        ConnectivityManager manager = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        if (manager == null) return null;
        for (Network network : manager.getAllNetworks()) {
            NetworkCapabilities caps = manager.getNetworkCapabilities(network);
            if (caps != null && caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
                    && caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)
                    && caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                    && caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) return network;
        }
        return null;
    }

    private AiSuggestion fakeSuggestion(String prompt) {
        String text = prompt == null ? "" : prompt.trim();
        String lower = text.toLowerCase(Locale.ROOT);
        String action = "none";
        if (text.contains("喂") || lower.contains("feed")) action = "feed";
        else if (text.contains("玩") || lower.contains("play")) action = "play";
        else if (text.contains("摸") || lower.contains("pet")) action = "pet";
        else if (text.contains("睡") || lower.contains("sleep")) action = "sleep";
        else if (text.contains("醒") || lower.contains("wake")) action = "wake";
        List<String> memories = Collections.emptyList();
        if (lower.contains("remember") && !text.isEmpty()) {
            memories = Collections.singletonList(text.length() > 160 ? text.substring(0, 160) : text);
        }
        return new AiSuggestion(text.isEmpty() ? "我在这里陪着你。" : "听到啦：" + text, "happy", action, memories);
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
                    setStatus("已保存所选内容到本机记忆；默认不会发送给 AI。");
                }).show();
    }

    private void deleteMemories() {
        final List<String> memories = memory.read();
        if (memories.isEmpty()) { setStatus("本机没有记忆可删除。"); return; }
        CharSequence[] options = memories.toArray(new CharSequence[0]);
        boolean[] checked = new boolean[options.length];
        new AlertDialog.Builder(this).setTitle("选择要删除的记忆").setMultiChoiceItems(options, checked,
                (dialog, which, selected) -> checked[which] = selected)
                .setNegativeButton("取消", null)
                .setPositiveButton("删除所选", (dialog, which) -> {
                    List<String> selected = new ArrayList<>();
                    for (int i = 0; i < options.length; i++) if (checked[i]) selected.add(options[i].toString());
                    memory.delete(selected);
                    refreshMemory();
                    setStatus(selected.isEmpty() ? "未选择任何记忆。" : "已删除所选记忆。");
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
                if (wifi == null) throw new IllegalStateException("请先连接 PetCard 热点 adol-2D5025。");
                httpGet(wifi, "http://192.168.0.1:8420/action?do=" + action, 5000, 16384);
                runOnUiThread(() -> { setStatus("动作已发送到设备。"); refreshPetState(); });
            } catch (Exception error) { runOnUiThread(() -> setStatus("本地动作失败：" + safeMessage(error))); }
        });
    }

    private void refreshPetState() {
        worker.execute(() -> {
            String summary;
            String context = "";
            try {
                Network wifi = findLocalWifiNetwork();
                if (wifi == null) throw new IllegalStateException("请先连接 PetCard 热点 adol-2D5025。");
                JSONObject state = new JSONObject(httpGet(wifi, PET_STATE_URL, 4000, 16384));
                summary = "设备：" + state.optString("stage_name", "PetCore") + " · EXP " + state.optInt("exp")
                        + " · 互动 " + state.optInt("interactions") + (state.optInt("sleeping") == 1 ? " · 睡眠中" : "");
                context = petContextFrom(state);
            } catch (Exception error) {
                summary = "设备状态：" + safeMessage(error);
            }
            final String summaryText = summary;
            final String contextText = context;
            runOnUiThread(() -> { petState.setText(summaryText); lastPetContext = contextText; });
        });
    }

    private String fetchPetContext() {
        try {
            Network wifi = findLocalWifiNetwork();
            if (wifi == null) return "";
            return petContextFrom(new JSONObject(httpGet(wifi, PET_STATE_URL, 2500, 16384)));
        } catch (Exception ignored) {
            return "";
        }
    }

    private static String petContextFrom(JSONObject state) {
        return "stage=" + state.optString("stage_name", "?")
                + "; exp=" + state.optInt("exp")
                + "; hunger=" + state.optInt("hunger") + "/100"
                + "; mood=" + state.optInt("mood") + "/100"
                + "; energy=" + state.optInt("energy") + "/100"
                + "; intimacy=" + state.optInt("intimacy") + "/100"
                + "; sleeping=" + state.optInt("sleeping")
                + "; age_min=" + state.optLong("age_min")
                + "; interactions=" + state.optLong("interactions");
    }

    private Network findLocalWifiNetwork() {
        ConnectivityManager manager = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        if (manager == null) return null;
        Network validatedMatch = null;
        for (Network network : manager.getAllNetworks()) {
            NetworkCapabilities caps = manager.getNetworkCapabilities(network);
            if (caps == null || !caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
                    || !caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)) continue;
            if (!hasPetcardSubnet(manager.getLinkProperties(network))) continue;
            if (!caps.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) return network;
            if (validatedMatch == null) validatedMatch = network;
        }
        return validatedMatch;
    }

    private static boolean hasPetcardSubnet(android.net.LinkProperties link) {
        if (link == null) return false;
        for (android.net.LinkAddress address : link.getLinkAddresses()) {
            java.net.InetAddress ip = address.getAddress();
            if (!(ip instanceof java.net.Inet4Address)) continue;
            byte[] raw = ip.getAddress();
            if ((raw[0] & 0xff) == 192 && (raw[1] & 0xff) == 168 && (raw[2] & 0xff) == 0) return true;
        }
        return false;
    }

    private static String httpGet(Network network, String url, int timeoutMs, int limit) throws Exception {
        HttpURLConnection connection = (HttpURLConnection) network.openConnection(new URL(url));
        try {
            connection.setConnectTimeout(timeoutMs);
            connection.setReadTimeout(timeoutMs);
            int code = connection.getResponseCode();
            InputStream input = code >= 200 && code < 300 ? connection.getInputStream() : connection.getErrorStream();
            String body = readBounded(input, limit);
            if (code < 200 || code >= 300) throw new java.io.IOException("设备返回 HTTP " + code);
            return body;
        } catch (java.net.SocketTimeoutException timeout) {
            throw new java.io.IOException("访问 192.168.0.1 超时：请确认已连接 PetCard 热点 adol-2D5025，且设备已按电源键唤醒");
        } catch (java.net.ConnectException refused) {
            throw new java.io.IOException("无法访问 192.168.0.1：请确认已连接 PetCard 热点 adol-2D5025");
        } finally {
            connection.disconnect();
        }
    }

    private static String readBounded(InputStream stream, int limit) throws Exception {
        if (stream == null) throw new java.io.IOException("设备响应为空");
        try (InputStream input = stream; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] chunk = new byte[2048];
            int total = 0;
            int count;
            while ((count = input.read(chunk)) != -1) {
                total += count;
                if (total > limit) throw new IllegalStateException("设备状态响应过大。");
                output.write(chunk, 0, count);
            }
            return new String(output.toByteArray(), StandardCharsets.UTF_8);
        }
    }

    private void refreshBudget() {
        budgetView.setText("今日 App 预算：¥" + String.format(Locale.ROOT, "%.4f", budget.spentToday())
                + " / ¥5.00（本机估算，不含同一 Key 的其他用量）");
    }

    private void setStatus(String text) { status.setText(text); }
    private static String safeMessage(Exception error) { return error.getMessage() == null ? "请求失败" : error.getMessage(); }
    private int dp(int value) { return (int) (value * getResources().getDisplayMetrics().density + 0.5f); }

    @Override public void onDestroy() { worker.shutdownNow(); super.onDestroy(); }
}
