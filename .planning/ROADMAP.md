# PetCard 阶段路线图

更新：2026-11-11。当前代码基线 `22e4176` 已推送至 `origin/main`；阶段 3 手机 Companion 页面和触控验收已完成。

## 阶段 4：云端 AI 与记忆（进行中：APK 已安装，离线流程通过初验）

### 目标

通过手机提供文字聊天、稳定宠物人格、受控情绪/动作建议和可恢复的长期记忆。设备端 PetCore 继续拥有宠物数值状态的唯一写权限；模型只能给建议，不能提交饥饿、心情、精力、亲密度、经验或成长阶段数值。

### 当前架构建议

- 以 Android 原生 Companion 为 AI 客户端，沿用项目方案中手机负责本地控制与云端 AI 的分工。
- 手机访问设备 `192.168.0.1:8420` 时绑定设备 Wi-Fi `Network`；云端请求走手机蜂窝网络。避免依赖手机浏览器对无互联网 Wi-Fi 的路由行为。
- 用户已选择 DeepSeek API 的 `deepseek-flash`，基础地址 `https://api.deepseek.com`；聊天文本可以发送给 provider。
- API key 由用户在 Android 客户端输入并用 Android Keystore 加密保护；不通过聊天、仓库、设备固件或 Companion 静态网页传递/存储。
- 每日预算上限为 ¥5。客户端发送请求前按最新已知价格、保守峰时费率和人民币换算率预留最大请求额度；达到上限后拒绝新请求。该客户端限制不覆盖同一 DeepSeek key 在其他应用/设备上的用量，设置页需明确提示。
- 保留厂商无关的 `AiProvider` 接口；先用本地假模型跑通聊天和建议流程，再接 DeepSeek adapter。
- AI 输出采用严格 schema：回复文本、有限枚举的情绪建议、有限枚举的动作建议，以及可选记忆候选。拒绝未知字段/枚举、任意数值、畸形 JSON 和超长内容。
- 动作建议由手机端与设备端规则二次校验，只能映射到 PetCore 已有动作；睡觉状态、精力不足等本地约束优先。模型输出不得直接改写状态。
- 长期记忆默认手机本地保存，先区分原始对话、摘要和可删除的记忆条目；跨设备同步保持关闭，直到用户决定。
- API key 不得写入设备固件、仓库或 Companion 静态网页。真实 provider、模型、密钥存储方式与费用授权确认前，不发送真实聊天内容或发起付费请求。

### 里程碑

1. **4A 安全契约与假模型竖切**（代码初版完成，待 Android App 验收）
   - 定义聊天请求/回复、宠物快照、建议动作与记忆候选的共享数据契约。
   - 明确本地建议验证规则以及拒绝/忽略不合规建议的行为。
   - 用假模型连接聊天界面、记忆存储接口和设备本地动作；不调用互联网 API。
2. **4B Android Companion 基础**（APK 构建并安装；离线首页/假模型初验完成，网络和安全存储实机验收待办）
   - 建立可构建的 Android 工程，迁移/复用阶段 3 的状态和互动界面。
   - 实现设备 Wi-Fi 与蜂窝网络分离、断网恢复和本地状态同步。
   - 实现 Android Keystore key storage 与请求前 ¥5/日预算 gate。
3. **4C 本地记忆**
   - 保存近期上下文、用户可见的长期记忆与必要的宠物状态快照。
   - 提供查看、删除和清空记忆入口；定义 schema 版本与备份/迁移规则。
4. **4D 真实 provider 接入**
   - 目标为 DeepSeek `deepseek-flash` Chat Completions；官方 API 使用 `https://api.deepseek.com/chat/completions`，JSON Output 仍由本地 schema 二次验证。
   - 网络/超时/限流/预算不足时回退到离线养成；AI 输出仍经过本地 schema 和动作校验。
5. **4E 手机实机验收**
   - 验证本地 Wi-Fi 控制与蜂窝 API 请求可并行工作。
   - 验证聊天、记忆恢复/删除、错误恢复和设备状态不被模型任意改写。

### 已确认 / 仍待决策

- **已确认**：Android 原生客户端；DeepSeek `deepseek-flash`；API key 存 Android Keystore；聊天文本允许发送；每日 app 内预算 ¥5；长期记忆默认仅手机本地。
- **仍待决策**：记忆是否允许云端同步；最低 Android 版本/应用包名沿用项目默认前先核对设备兼容性。

### 当前实现证据与限制

- `android/` 含原生单页 Companion、离线假模型、DeepSeek `chat/completions` adapter、Android Keystore AES/GCM key 封装、每日预算预留/结算、严格 JSON schema、本机记忆审批与清空，以及设备 Wi‑Fi/蜂窝网络分别绑定。
- Java 源文件使用 `javac -encoding UTF-8` 对 Android 36 `android.jar` 编译通过；Gradle 8.13 `:app:assembleDebug` 成功，产物 `android/app/build/outputs/apk/debug/app-debug.apk`（26,694 B）。构建有 SDK XML 版本 4 与工具最多支持版本 3 的兼容警告及 Gradle 9.0 弃用提示。
- APK 已装到 Android 16 手机并启动；离线假模型返回默认回复、`happy`、`none`，预算为 ¥0.0000。手机当时未连 PetCard AP，设备 `/state` 显示 timeout。尚未做 Wi‑Fi/蜂窝分流、Keystore/动作/记忆实机验收。本轮没有真实 DeepSeek 请求或 API 费用。每日 ¥5 是仅限本 App 的估算/预算 gate，无法限制共享 Key 的其他用量，也无法代替 provider 账户限额。
- UTC 日界减少时区变化造成的额度重置，但用户修改系统日期或清除 App 数据仍可重置本地累计；不能将此客户端 gate 描述为严格账单硬上限。
- 复核将 Java 集合工厂方法替换为旧 Android 兼容实现；Keystore 偏好写入失败会报错；App 内发送聊天内容需勾选确认。

### 范围边界

- 本地状态与数值由 PetCore 养成引擎管理。
- AI 不获得 framebuffer、存档文件或设备 shell 访问。
- 不新增让模型写任意宠物属性的 HTTP 接口；现有 `/action` 仍只接受已知动作。
- 阶段 4 可开发契约、Android 骨架和假模型。用户已授权将聊天内容发送到 DeepSeek，并给出 ¥5/日上限；真实请求只能在用户把 key 输入 Android 客户端、预算 gate 可用后发起，且 app 必须拒绝超过剩余预算的请求。

## 已完成阶段

- 阶段 0：硬件/系统能力确认。
- 阶段 1：显示与按键原型实机验收。
- 阶段 2：离线养成实机验收。
- 阶段 3：本地 HTTP Companion 页面手机视觉及触控验收。

## 后续阶段

- 阶段 5：持久安装、开机启动、UI 恢复自动化、外壳与供电验收；在阶段 4 核心交付稳定后开始。
