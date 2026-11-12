# ADR-0002：Android 发布硬化（签名、FLAG_SECURE、edge-to-edge）

- 状态：已接受（0.3.0，2026-11-12）
- 相关：`android/`、`HANDOFF.md`、`docs/known-issues.md`

## 背景

1. 0.2.0 只有 debug 包，没有 release 签名；`FLAG_SECURE`、edge-to-edge 未处理。Android 16 上标题会被状态栏遮挡。
2. 手机已保存用户的 DeepSeek Key（Keystore 加密）。任何涉及卸载/换签名的迁移都可能清空 App 数据，不能擅自做。
3. 阶段 4 剩余项要求真机验证预算拒绝路径，但不得产生未授权的付费请求。
4. 真实 provider 401/402/429/5xx 无法在不影响用户 Key/不产生账单的前提下稳定触发。

## 决策

1. **Release 签名密钥放在仓库外**：`work/android-signing/petcard-release.jks`，密码只写入同目录 `signing.properties`；`work/` 已被 `.gitignore` 排除。`app/build.gradle` 仅在属性文件存在时启用 release 签名，缺省仍可构建未签名 release 包。用户需自行备份该目录；丢失后无法覆盖升级已发布安装。
2. **FLAG_SECURE 只在 release 启用**：根据 `ApplicationInfo.FLAG_DEBUGGABLE` 判断。debug 保留截屏能力，便于验收与取证；release 禁止截屏/录屏。
3. **edge-to-edge 用平台 API，不新增依赖**：API 30+ 使用 `setDecorFitsSystemWindows(false)` + `WindowInsets.Type.systemBars()/displayCutout()/ime()`；API 26–29 使用 `getSystemWindowInset*`。滚动容器按 insets 加 padding，输入法弹出时底部取 IME 与导航栏的较大值。
4. **预算拒绝用本机注入验证，零计费**：备份 `daily_api_budget.xml` 后通过 `run-as` 写入“今日已用 ¥5.00”；用不可达全局代理 `127.0.0.1:9` 作为兜底（若预算闸门失效，请求也无法到达 provider）；验证后恢复 prefs 与代理设置。全程不发送真实请求。
5. **Provider 错误码以单测覆盖映射**：`describeHttpError` 放宽为包内可见并补 401/403/402/429/5xx 映射与截断测试；真实错误码仍未触发，不在文档中伪造账单结论。

## 备选方案

- 给所有构建加 FLAG_SECURE：更安全，但调试/取证无法截屏；用户选择 release-only，放弃。
- Release 直接覆盖安装 debug：签名不同，必须卸载并丢失 App 数据；本轮不做迁移，保留为后续产品决策。
- 用 appops 禁止 App 联网作为预算测试兜底：Android 16 无 `INTERNET` app-op，不可用；改用全局代理。
- 真实 401 测试：需要临时替换用户 Key；未获授权，不做。

## 后果

- 每次 release 构建依赖本机 `work/android-signing`；换机/交接必须同时转移密钥与密码，且不得提交仓库。
- 当前手机装的仍是 debug 签名 0.3.0；将来切 release 需要用户确认卸载迁移（会丢 Key、预算与记忆）。
- Debug 包可截屏；release 包只会得到黑色截图，验收依赖 uiautomator 文本与实机查看。
- 预算测试的注入文件与截图证据保留在 `work/`（gitignore），不进入仓库。
